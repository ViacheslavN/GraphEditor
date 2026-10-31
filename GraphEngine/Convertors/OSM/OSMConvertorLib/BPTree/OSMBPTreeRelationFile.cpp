#include "OSMBPTreeRelationFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../bptreedb/StorageLib/WriteStreamPage.h"
#include "../../../../../bptreedb/StorageLib/ReadStreamPage.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            namespace
            {
                // the counts of the header after the tree info page, in the order
                template <class THeader, class TFn>
                void ForCounts(THeader& h, TFn fn)
                {
                    fn(h.nRelationCount); fn(h.nMemberCount); fn(h.nNodeMembers); fn(h.nWayMembers); fn(h.nRelationMembers);
                    fn(h.nMissingNodes); fn(h.nMissingWays); fn(h.nWithShape); fn(h.nPolygons); fn(h.nAreasWithoutRings);
                    fn(h.nUnclosedWays); fn(h.nRecordBytes); fn(h.nShapeBytes);
                }
            }

            void WriteRelationHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeRelationHeader& header)
            {
                try
                {
                    bptreedb::storage::TWriteStreamPage stream(ptrPager);
                    stream.Open(nPage, true);
                    stream.Write(SOSMBPTreeRelationHeader::Magic);
                    stream.Write(SOSMBPTreeRelationHeader::Version);
                    stream.Write(header.nPageSize);
                    stream.Write(header.nTreeInfoPage);
                    ForCounts(header, [&](const uint64_t& nValue) { stream.Write(nValue); });
                    stream.Write(header.nMinId);
                    stream.Write(header.nMaxId);
                    stream.Close();
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to write the relation header, page {0}", nPage, exc);
                    throw;
                }
            }

            SOSMBPTreeRelationHeader ReadRelationHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage)
            {
                try
                {
                    bptreedb::storage::TReadStreamPage stream(ptrPager);
                    stream.Open(nPage);
                    if (stream.ReadIntu64() != SOSMBPTreeRelationHeader::Magic)
                        throw CommonLib::CExcBase("not an OSM relation file");
                    uint32_t nVersion = stream.ReadIntu32();
                    if (nVersion != SOSMBPTreeRelationHeader::Version)
                        throw CommonLib::CExcBase("unsupported version {0}", nVersion);

                    SOSMBPTreeRelationHeader header;
                    header.nPageSize = stream.ReadIntu32();
                    header.nTreeInfoPage = stream.ReadInt64();
                    ForCounts(header, [&](uint64_t& nValue) { nValue = stream.ReadIntu64(); });
                    header.nMinId = stream.ReadInt64();
                    header.nMaxId = stream.ReadInt64();
                    return header;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to read the relation header, page {0}", nPage, exc);
                    throw;
                }
            }

            COSMBPTreeRelationFile::COSMBPTreeRelationFile() {}

            COSMBPTreeRelationFile::~COSMBPTreeRelationFile()
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }

            void COSMBPTreeRelationFile::Open(const std::string& sRelationFile, uint32_t nPageSize, uint32_t nTreeCacheNodes, uint32_t nPagerCachePages)
            {
                try
                {
                    Close();
                    m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                    m_ptrStorage.reset(new bptreedb::storage::CFileStorage(m_ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    m_ptrStorage->Open(sRelationFile.c_str(), false, 0, nPageSize);
                    m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, m_ptrStorage, nPagerCachePages));
                    m_header = ReadRelationHeader(m_ptrPager, 0);
                    if (m_header.nPageSize != nPageSize)
                        throw CommonLib::CExcBase("the page size of the file is {0}, not {1}", m_header.nPageSize, nPageSize);
                    m_ptrTree.reset(new TRelationTree(m_header.nTreeInfoPage, m_ptrPager, m_ptrAlloc, nTreeCacheNodes, nPageSize, ShapeInlineSize(nPageSize)));
                }
                catch (std::exception& exc)
                {
                    m_ptrTree.reset();
                    m_ptrPager.reset();
                    m_ptrStorage.reset();
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to open the relation file {0}", sRelationFile, exc);
                    throw;
                }
            }

            void COSMBPTreeRelationFile::Close()
            {
                m_ptrTree.reset();
                m_ptrPager.reset();
                if (m_ptrStorage.get())
                {
                    m_ptrStorage->Close();
                    m_ptrStorage.reset();
                }
            }

            bool COSMBPTreeRelationFile::GetBlob(int64_t nId, std::vector<byte_t>& blob)
            {
                if (!m_ptrTree)
                    throw CommonLib::CExcBase("[OSMBPTree] the relation file isn't open");
                bptreedb::IBlobPtr ptrBlob = m_ptrTree->GetBlob(nId);
                m_ptrTree->CheckCache();
                if (!ptrBlob)
                {
                    blob.clear();
                    return false;
                }
                blob.resize((size_t)ptrBlob->Size());
                if (!blob.empty())
                    ptrBlob->GetReadStream()->ReadBytes(blob.data(), blob.size());
                return true;
            }

            bool COSMBPTreeRelationFile::GetRelation(int64_t nId, SRelationRecord& record)
            {
                std::vector<byte_t> blob;
                if (!GetBlob(nId, blob))
                {
                    record = SRelationRecord();
                    return false;
                }
                DecodeRelationRecord(blob.data(), blob.size(), record);
                return true;
            }
        }
    }
}
