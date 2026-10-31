#include "OSMSharedWayReader.h"
#include "OSMBPTreeWayFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../bptreedb/BPtreeLib/Blob.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../../CommonLib/stream/MemoryStream.h"

#include <cstring>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            CSharedWayReader::CSharedWayReader(std::shared_ptr<bptreedb::storage::CFileStorage> ptrStorage, const SOSMBPTreeWayHeader& header,
                                               uint32_t nCacheMB, uint64_t nDefaultMemory) :
                    m_ptrStorage(ptrStorage), m_header(header), m_ptrParams(CreateShapeCompressParams())
            {
                try
                {
                    CommonLib::IAllocPtr ptrAlloc(new CommonLib::CSimpleAlloc(false));
                    std::shared_ptr<bptreedb::storage::CFilePager> ptrPager(new bptreedb::storage::CFilePager(ptrAlloc, m_ptrStorage, 64));
                    TWayShapeTree tree(m_header.nTreeInfoPage, ptrPager, ptrAlloc, 64, m_header.nPageSize, ShapeInlineSize(m_header.nPageSize));
                    BuildLeafIndex(tree, m_keys, m_addrs);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[CSharedWayReader] failed to read the leaf index", exc);
                    throw;
                }

                // a decoded leaf: its ids and shapes
                uint64_t nLeaves = (std::max)((size_t)1, m_keys.size());
                uint64_t nLeafBytes = (m_header.nWayCount * (sizeof(int64_t) + sizeof(uint32_t)) + m_header.nShapeBytes) / nLeaves + 256;
                m_cache.SetCapacity(CacheLeavesFor(m_keys.size(), nLeafBytes, nCacheMB, nDefaultMemory ? nDefaultMemory : PhysicalMemory() / 4));
            }

            CSharedWayReader::~CSharedWayReader()
            {
            }

            CSharedWayReader::CLookup::CLookup(CSharedWayReader& reader) : m_reader(reader)
            {
                m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, reader.m_ptrStorage, 4));
                m_ptrKeys.reset(new TNodeIdEncoder(bptreedb::TAllocsSetPtr(), reader.m_ptrParams, bptreedb::eLeafKey));
            }

            CSharedWayReader::CLookup::~CLookup()
            {
            }

            bool CSharedWayReader::CLookup::GetBlob(int64_t nId, const byte_t*& pData, uint32_t& nSize)
            {
                const std::vector<int64_t>& keys = m_reader.m_keys;
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
                size_t i = (size_t)(it - ids.begin());
                pData = m_ptrLeaf->data.data() + m_ptrLeaf->offsets[i];
                nSize = m_ptrLeaf->Size(i);
                return true;
            }

            bool CSharedWayReader::CLookup::GetPoints(int64_t nId, std::vector<SOSMPointE7>& points)
            {
                const byte_t* pData = nullptr;
                uint32_t nSize = 0;
                if (!GetBlob(nId, pData, nSize))
                {
                    points.clear();
                    return false;
                }
                COSMBPTreeWayFile::DecodePoints(pData, nSize, points);
                return true;
            }

            CSharedWayReader::TLeafPtr CSharedWayReader::CLookup::Decode(int64_t nAddr)
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

                    std::shared_ptr<SLeaf> ptrLeaf = std::make_shared<SLeaf>();
                    if (nCount != 0)
                    {
                        TNodeIdEncoder::TValueMemSet keys{ TNodeIdEncoder::TAlloc(m_ptrAlloc) };
                        CommonLib::CReadMemoryStream keyStream;
                        keyStream.AttachBuffer((byte_t*)pData + nHead + stream.Pos(), nKeySize);
                        m_ptrKeys->Decode(nCount, keys, &keyStream, nKeySize, nullptr);
                        ptrLeaf->ids.assign(keys.begin(), keys.end());

                        // the values (CBlobValueEncoder): int64 addr; 0 - uint16 size, the bytes; otherwise the stream pages
                        CommonLib::CReadMemoryStream values;
                        values.AttachBuffer((byte_t*)pData + nHead + stream.Pos() + nKeySize, nValueSize);
                        ptrLeaf->offsets.resize(nCount);
                        for (uint32_t i = 0; i < nCount; ++i)
                        {
                            ptrLeaf->offsets[i] = (uint32_t)ptrLeaf->data.size();
                            int64_t nBlobAddr = values.ReadInt64();
                            if (nBlobAddr == 0)
                            {
                                uint16_t nBlobSize = values.Readintu16();
                                size_t nPos = ptrLeaf->data.size();
                                ptrLeaf->data.resize(nPos + nBlobSize);
                                if (nBlobSize)
                                    values.ReadBytes(ptrLeaf->data.data() + nPos, nBlobSize);
                            }
                            else
                            {
                                std::shared_ptr<bptreedb::CPageBlob> ptrBlob = std::make_shared<bptreedb::CPageBlob>(m_ptrPager, nBlobAddr);
                                size_t nBlobSize = (size_t)ptrBlob->Size();
                                size_t nPos = ptrLeaf->data.size();
                                ptrLeaf->data.resize(nPos + nBlobSize);
                                if (nBlobSize)
                                    ptrBlob->GetReadStream()->ReadBytes(ptrLeaf->data.data() + nPos, nBlobSize);
                            }
                        }
                    }
                    return ptrLeaf;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[CSharedWayReader] failed to decode the leaf {0}", nAddr, exc);
                    throw;
                }
            }
        }
    }
}
