#include "OSMBPTreeNodeFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../bptreedb/StorageLib/WriteStreamPage.h"
#include "../../../../../bptreedb/StorageLib/ReadStreamPage.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void WriteNodeHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeNodeHeader& header)
            {
                try
                {
                    bptreedb::storage::TWriteStreamPage stream(ptrPager);
                    stream.Open(nPage, true);
                    stream.Write(SOSMBPTreeNodeHeader::Magic);
                    stream.Write(SOSMBPTreeNodeHeader::Version);
                    stream.Write(header.nPageSize);
                    stream.Write(header.nTreeInfoPage);
                    stream.Write(header.nNodeCount);
                    stream.Write(header.nMinId);
                    stream.Write(header.nMaxId);
                    stream.Write(header.minPoint.lat);
                    stream.Write(header.minPoint.lon);
                    stream.Write(header.maxPoint.lat);
                    stream.Write(header.maxPoint.lon);
                    stream.Close();
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to write the node header, page {0}", nPage, exc);
                    throw;
                }
            }

            SOSMBPTreeNodeHeader ReadNodeHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage)
            {
                try
                {
                    bptreedb::storage::TReadStreamPage stream(ptrPager);
                    stream.Open(nPage);
                    uint64_t nMagic = stream.ReadIntu64();
                    if (nMagic != SOSMBPTreeNodeHeader::Magic)
                        throw CommonLib::CExcBase("not an OSM node file");
                    uint32_t nVersion = stream.ReadIntu32();
                    if (nVersion != SOSMBPTreeNodeHeader::Version)
                        throw CommonLib::CExcBase("unsupported version {0}", nVersion);

                    SOSMBPTreeNodeHeader header;
                    header.nPageSize = stream.ReadIntu32();
                    header.nTreeInfoPage = stream.ReadInt64();
                    header.nNodeCount = stream.ReadIntu64();
                    header.nMinId = stream.ReadInt64();
                    header.nMaxId = stream.ReadInt64();
                    header.minPoint.lat = stream.ReadInt32();
                    header.minPoint.lon = stream.ReadInt32();
                    header.maxPoint.lat = stream.ReadInt32();
                    header.maxPoint.lon = stream.ReadInt32();
                    return header;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to read the node header, page {0}", nPage, exc);
                    throw;
                }
            }

            // ---------------- the node file

            COSMBPTreeNodeFile::COSMBPTreeNodeFile() {}

            COSMBPTreeNodeFile::~COSMBPTreeNodeFile()
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }

            void COSMBPTreeNodeFile::Open(const std::string& sNodeFile, uint32_t nPageSize, uint32_t nTreeCacheNodes, uint32_t nPagerCachePages)
            {
                try
                {
                    Close();
                    m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                    m_ptrStorage.reset(new bptreedb::storage::CFileStorage(m_ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    m_ptrStorage->Open(sNodeFile.c_str(), false, 0, nPageSize);
                    m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, m_ptrStorage, nPagerCachePages));

                    m_header = ReadNodeHeader(m_ptrPager, 0);
                    if (m_header.nPageSize != nPageSize)
                        throw CommonLib::CExcBase("the page size of the file is {0}, not {1}", m_header.nPageSize, nPageSize);

                    m_ptrTree.reset(new TNodeTree(m_header.nTreeInfoPage, m_ptrPager, m_ptrAlloc, nTreeCacheNodes, nPageSize));
                }
                catch (std::exception& exc)
                {
                    m_ptrTree.reset();
                    m_ptrPager.reset();
                    m_ptrStorage.reset();
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to open the node file {0}", sNodeFile, exc);
                    throw;
                }
            }

            void COSMBPTreeNodeFile::Close()
            {
                m_ptrTree.reset();
                m_ptrPager.reset();
                if (m_ptrStorage.get())
                {
                    m_ptrStorage->Close();
                    m_ptrStorage.reset();
                }
            }

            bool COSMBPTreeNodeFile::Get(int64_t nId, SOSMPointE7& point)
            {
                if (!m_ptrTree)
                    throw CommonLib::CExcBase("[OSMBPTree] the node file isn't open");
                auto it = m_ptrTree->find(nId);
                bool bFound = !it.IsNull();
                if (bFound)
                    point = it.Value();
                m_ptrTree->CheckCache();    // find only adds the read nodes to the cache
                return bFound;
            }
        }
    }
}
