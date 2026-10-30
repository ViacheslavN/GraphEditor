#include "OSMTagWriter.h"
#include "OSMBPTreeUtils.h"
#include "OSMBPTreeTagFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

#include <algorithm>
#include <cstring>
#include <limits>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void STagBatch::Add(int64_t nId, int nTagCount, const readosm_tag* pTags)
            {
                ids.push_back(nId);
                offsets.push_back((uint32_t)data.size());
                for (int i = 0; i < nTagCount; ++i)
                {
                    if (i != 0)
                        data += TagSeparator;
                    const readosm_tag& tag = pTags[i];
                    data += tag.key ? tag.key : "";
                    data += TagKeySeparator;
                    data += tag.value ? tag.value : "";
                }
                nTags += (uint64_t)nTagCount;
            }

            // The source of TBPMap::BulkLoad (the tag thread): the values of the queued batches in the id order, the
            // others aside. A value longer than the maximum is cut at the last tag that fits.
            class CTagWriter::CSource
            {
            public:
                struct SAside
                {
                    int64_t     id;
                    std::string tags;
                };

                CSource(CTagWriter& writer, TTagTree& tree) : m_writer(writer), m_tree(tree), m_nMaxLen(tree.MaxValueLen()) {}

                bool Next(int64_t& nId, bptreedb::StringValue& value)
                {
                    for (;;)
                    {
                        if (m_nPos == m_batch.Count())
                        {
                            m_batch.Clear();
                            m_nPos = 0;
                            if (!m_writer.Pop(m_batch))
                                return false;
                            m_writer.m_header.nTagCount += m_batch.nTags;
                            continue;
                        }
                        size_t i = m_nPos++;
                        const char* pTags = m_batch.data.data() + m_batch.offsets[i];
                        uint32_t nLen = Fit(pTags, m_batch.Size(i));
                        m_writer.m_nRawBytes += nLen;
                        if (m_batch.ids[i] <= m_nLastId)
                        {
                            m_aside.push_back(SAside{ m_batch.ids[i], std::string(pTags, nLen) });
                            m_writer.m_nUnsorted++;
                            continue;
                        }
                        m_nLastId = m_batch.ids[i];
                        nId = m_batch.ids[i];
                        value = m_tree.MakeValue(pTags, nLen);
                        m_writer.m_nNodes++;
                        return true;
                    }
                }

                // the length of the tags that fit (whole tags, at most the maximum)
                uint32_t Fit(const char* pTags, size_t nLen)
                {
                    if (nLen <= m_nMaxLen)
                        return (uint32_t)nLen;
                    m_writer.m_header.nTruncated++;
                    size_t nCut = m_nMaxLen;
                    while (nCut > 0 && pTags[nCut] != TagSeparator)
                        --nCut;
                    return (uint32_t)nCut;
                }

                std::vector<SAside>& Aside() { return m_aside; }

            private:
                CTagWriter& m_writer;
                TTagTree&   m_tree;
                uint32_t    m_nMaxLen;
                STagBatch   m_batch;
                size_t      m_nPos = 0;
                int64_t     m_nLastId = (std::numeric_limits<int64_t>::min)();
                std::vector<SAside> m_aside;
            };

            CTagWriter::CTagWriter(const SOSMBPTreeSettings& settings, const std::string& sTagFile) :
                    m_settings(settings), m_sFile(sTagFile)
            {
                m_thread = std::thread([this]() { Run(); });
            }

            CTagWriter::~CTagWriter()
            {
                Stop();
            }

            bool CTagWriter::Push(STagBatch& batch)
            {
                {
                    std::unique_lock<std::mutex> lock(m_mutex);
                    m_cvPush.wait(lock, [this]() { return m_bStop || m_bFinished || m_queue.size() < m_settings.nTagQueueBatches; });
                    if (m_bStop || m_bFinished)
                        return false;
                    m_queue.emplace_back(std::move(batch));
                }
                m_cvPop.notify_one();
                batch = STagBatch();
                return true;
            }

            bool CTagWriter::Pop(STagBatch& batch)
            {
                auto start = TClock::now();
                std::unique_lock<std::mutex> lock(m_mutex);
                m_cvPop.wait(lock, [this]() { return m_bStop || m_bInputDone || !m_queue.empty(); });
                m_nWaitNs += NanosecondsSince(start);
                if (m_bStop || m_queue.empty())
                    return false;
                batch = std::move(m_queue.front());
                m_queue.pop_front();
                lock.unlock();
                m_cvPush.notify_one();
                return true;
            }

            void CTagWriter::Finish()
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bInputDone = true;
                }
                m_cvPop.notify_all();
                if (m_thread.joinable())
                    m_thread.join();
                if (m_error)
                    std::rethrow_exception(m_error);
            }

            void CTagWriter::Stop()
            {
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bStop = true;
                }
                m_cvPop.notify_all();
                m_cvPush.notify_all();
                if (m_thread.joinable())
                    m_thread.join();
            }

            void CTagWriter::Run()
            {
                auto start = TClock::now();
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
                    std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage(new bptreedb::storage::CFileStorage(ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    ptrStorage->Open(m_sFile.c_str(), true, 0, m_settings.nTagPageSize);
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, ptrStorage, m_settings.nTagPagerCachePages));

                    m_header.nPageSize = m_settings.nTagPageSize;
                    m_header.nMaxLen = m_settings.nTagMaxLen;
                    int64_t nHeaderPage = ptrPager->GetNewFilePageAddr();     // 0: the first page of the new file
                    {
                        TTagTree tree(-1, ptrPager, ptrAlloc, m_settings.nTagTreeCacheNodes, m_settings.nTagPageSize, m_settings.nTagMaxLen, true);
                        tree.InnitTree(CreateTagCompressParams(m_settings.nTagMaxLen, m_settings.nTagCompressLevel), true, 0,
                                       bptreedb::storage::eEmptyType, 0, bptreedb::storage::eEmptyType);
                        m_header.nTreeInfoPage = tree.GetPageBTreeInfo();

                        // the tags in the id order, then the others
                        CSource source(*this, tree);
                        tree.BulkLoad(source);

                        std::vector<CSource::SAside>& aside = source.Aside();
                        std::stable_sort(aside.begin(), aside.end(), [](const CSource::SAside& a, const CSource::SAside& b) { return a.id < b.id; });
                        for (const CSource::SAside& item : aside)
                        {
                            {
                                std::lock_guard<std::mutex> lock(m_mutex);
                                if (m_bStop)
                                    break;
                            }
                            if (!tree.find(item.id).IsNull())
                            {
                                ++m_nDuplicates;
                                continue;
                            }
                            tree.insert(item.id, item.tags);
                            m_nNodes++;
                        }
                        tree.Flush();
                    }

                    m_header.nNodeCount = m_nNodes.load();
                    m_header.nRawBytes = m_nRawBytes.load();
                    WriteTagHeader(ptrPager, nHeaderPage, m_header);
                    ptrPager->Flush();
                    ptrStorage->Close();
                }
                catch (std::exception& exc)
                {
                    try
                    {
                        CommonLib::CExcBase::RegenExcT("[CTagWriter] failed to write the tag file {0}", m_sFile, exc);
                    }
                    catch (...)
                    {
                        std::lock_guard<std::mutex> lock(m_mutex);
                        m_error = std::current_exception();
                    }
                }
                catch (...)
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_error = std::current_exception();
                }
                m_nWorkNs = NanosecondsSince(start) - (std::min)(m_nWaitNs.load(), NanosecondsSince(start));
                {
                    std::lock_guard<std::mutex> lock(m_mutex);
                    m_bFinished = true;
                }
                m_cvPush.notify_all();
            }
        }
    }
}
