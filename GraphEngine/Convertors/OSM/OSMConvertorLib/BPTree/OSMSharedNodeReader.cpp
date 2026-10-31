#include "OSMSharedNodeReader.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../../CommonLib/stream/MemoryStream.h"

#include <cstring>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CSharedNodeReader::CSharedNodeReader(std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage, const SOSMBPTreeNodeHeader& header,
                                                 uint32_t nCacheLeaves, uint32_t nCacheMB, uint64_t nDefaultMemory) :
                    m_ptrStorage(ptrStorage), m_header(header), m_ptrParams(CreateCompressParams())
            {
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, m_ptrStorage, 64));
                    TNodeTree tree(m_header.nTreeInfoPage, ptrPager, ptrAlloc, 64, m_header.nPageSize);
                    BuildLeafIndex(tree, m_keys, m_addrs);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[CSharedNodeReader] failed to read the leaf index", exc);
                    throw;
                }

                uint64_t nLeaves = nCacheLeaves;
                if (nLeaves == 0)
                {
                    // a decoded node takes 16 bytes (id, point)
                    uint64_t nLeafBytes = (m_header.nNodeCount / (std::max)((size_t)1, m_keys.size())) * (sizeof(int64_t) + sizeof(SOSMPointE7)) + 256;
                    nLeaves = CacheLeavesFor(m_keys.size(), nLeafBytes, nCacheMB, nDefaultMemory ? nDefaultMemory : PhysicalMemory() / 2);
                }
                m_cache.SetCapacity(nLeaves);
            }

            CSharedNodeReader::~CSharedNodeReader()
            {
            }

            // ---------------- the lookups of a thread

            CSharedNodeReader::CLookup::CLookup(CSharedNodeReader& reader) : m_reader(reader)
            {
                m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, reader.m_ptrStorage, 4));
                m_ptrKeys.reset(new TNodeIdEncoder(bptreedb::TAllocsSetPtr(), reader.m_ptrParams, bptreedb::eLeafKey));
                m_ptrValues.reset(new TPointCompressor(bptreedb::TAllocsSetPtr(), reader.m_ptrParams, bptreedb::eLeafValue));
                m_ptrValues->SetKeepStatistics(false);
            }

            CSharedNodeReader::CLookup::~CLookup()
            {
            }

            bool CSharedNodeReader::CLookup::Get(int64_t nId, SOSMPointE7& point)
            {
                const std::vector<int64_t>& keys = m_reader.m_keys;
                // the last leaf of the thread, otherwise the index
                if (!(m_ptrLeaf && InLeaf(keys, m_nLeaf, nId)))
                {
                    size_t nLeaf = 0;
                    if (!FindLeaf(keys, nId, nLeaf))
                        return false;
                    if (nLeaf != m_nLeaf || !m_ptrLeaf)
                    {
                        int64_t nAddr = m_reader.m_addrs[nLeaf];
                        m_ptrLeaf = m_reader.m_cache.Get(nAddr, [this, nAddr]() { return Decode(nAddr); });
                        m_nLeaf = nLeaf;
                    }
                }
                const std::vector<int64_t>& ids = m_ptrLeaf->ids;
                auto it = std::lower_bound(ids.begin(), ids.end(), nId);
                if (it == ids.end() || *it != nId)
                    return false;
                point = m_ptrLeaf->points[(size_t)(it - ids.begin())];
                return true;
            }

            CSharedNodeReader::TLeafPtr CSharedNodeReader::CLookup::Decode(int64_t nAddr)
            {
                try
                {
                    bptreedb::storage::IFilePagePtr ptrPage = m_ptrPager->ReadPage(nAddr);
                    const byte_t* pData = ptrPage->GetPayLoadData();
                    uint32_t nSize = ptrPage->GetSize();
                    bool bLeaf = false;
                    memcpy(&bLeaf, pData, sizeof(bool));
                    if (!bLeaf)
                        throw CommonLib::CExcBase("not a leaf");

                    // the leaf flag, next, prev; the compressor: count, key size, value size, keys, values
                    size_t nHead = sizeof(bool) + 2 * sizeof(int64_t);
                    CommonLib::CReadMemoryStream stream;
                    stream.AttachBuffer((byte_t*)pData + nHead, nSize - nHead);
                    uint32_t nCount = stream.ReadIntu32();
                    uint32_t nKeySize = stream.ReadIntu32();
                    uint32_t nValueSize = stream.ReadIntu32();
                    if (stream.Pos() + (size_t)nKeySize + nValueSize > nSize - nHead)
                        throw CommonLib::CExcBase("wrong sizes {0}, {1}", nKeySize, nValueSize);

                    TNodeIdEncoder::TValueMemSet keys{ TNodeIdEncoder::TAlloc(m_ptrAlloc) };
                    TPointCompressor::TValueMemSet values{ TPointCompressor::TAlloc(m_ptrAlloc) };
                    if (nCount != 0)
                    {
                        CommonLib::CReadMemoryStream keyStream;
                        keyStream.AttachBuffer((byte_t*)pData + nHead + stream.Pos(), nKeySize);
                        m_ptrKeys->Decode(nCount, keys, &keyStream, nKeySize, nullptr);
                        CommonLib::CReadMemoryStream valueStream;
                        valueStream.AttachBuffer((byte_t*)pData + nHead + stream.Pos() + nKeySize, nValueSize);
                        m_ptrValues->Decode(nCount, values, &valueStream, nValueSize, nullptr);
                    }

                    std::shared_ptr<SLeaf> ptrLeaf = std::make_shared<SLeaf>();
                    ptrLeaf->ids.assign(keys.begin(), keys.end());
                    ptrLeaf->points.assign(values.begin(), values.end());
                    return ptrLeaf;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[CSharedNodeReader] failed to decode the leaf {0}", nAddr, exc);
                    throw;
                }
            }
        }
    }
}
