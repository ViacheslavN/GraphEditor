#include "OSMBPTreeTagFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../bptreedb/StorageLib/WriteStreamPage.h"
#include "../../../../../bptreedb/StorageLib/ReadStreamPage.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void WriteTagHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeTagHeader& header)
            {
                try
                {
                    bptreedb::storage::TWriteStreamPage stream(ptrPager);
                    stream.Open(nPage, true);
                    stream.Write(SOSMBPTreeTagHeader::Magic);
                    stream.Write(SOSMBPTreeTagHeader::Version);
                    stream.Write(header.nPageSize);
                    stream.Write(header.nTreeInfoPage);
                    stream.Write(header.nMaxLen);
                    stream.Write(header.nNodeCount);
                    stream.Write(header.nTagCount);
                    stream.Write(header.nRawBytes);
                    stream.Write(header.nTruncated);
                    stream.Close();
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to write the tag header, page {0}", nPage, exc);
                    throw;
                }
            }

            SOSMBPTreeTagHeader ReadTagHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage)
            {
                try
                {
                    bptreedb::storage::TReadStreamPage stream(ptrPager);
                    stream.Open(nPage);
                    if (stream.ReadIntu64() != SOSMBPTreeTagHeader::Magic)
                        throw CommonLib::CExcBase("not an OSM tag file");
                    uint32_t nVersion = stream.ReadIntu32();
                    if (nVersion != SOSMBPTreeTagHeader::Version)
                        throw CommonLib::CExcBase("unsupported version {0}", nVersion);

                    SOSMBPTreeTagHeader header;
                    header.nPageSize = stream.ReadIntu32();
                    header.nTreeInfoPage = stream.ReadInt64();
                    header.nMaxLen = stream.ReadIntu32();
                    header.nNodeCount = stream.ReadIntu64();
                    header.nTagCount = stream.ReadIntu64();
                    header.nRawBytes = stream.ReadIntu64();
                    header.nTruncated = stream.ReadIntu64();
                    return header;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to read the tag header, page {0}", nPage, exc);
                    throw;
                }
            }

            COSMBPTreeTagFile::COSMBPTreeTagFile() {}

            COSMBPTreeTagFile::~COSMBPTreeTagFile()
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }

            void COSMBPTreeTagFile::Open(const std::string& sTagFile, uint32_t nPageSize, uint32_t nTreeCacheNodes, uint32_t nPagerCachePages)
            {
                try
                {
                    Close();
                    m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                    m_ptrStorage.reset(new bptreedb::storage::CFileStorage(m_ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    m_ptrStorage->Open(sTagFile.c_str(), false, 0, nPageSize);
                    m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, m_ptrStorage, nPagerCachePages));
                    m_header = ReadTagHeader(m_ptrPager, 0);
                    if (m_header.nPageSize != nPageSize)
                        throw CommonLib::CExcBase("the page size of the file is {0}, not {1}", m_header.nPageSize, nPageSize);
                    m_ptrTree.reset(new TTagTree(m_header.nTreeInfoPage, m_ptrPager, m_ptrAlloc, nTreeCacheNodes, nPageSize, m_header.nMaxLen));
                }
                catch (std::exception& exc)
                {
                    m_ptrTree.reset();
                    m_ptrPager.reset();
                    m_ptrStorage.reset();
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to open the tag file {0}", sTagFile, exc);
                    throw;
                }
            }

            void COSMBPTreeTagFile::Close()
            {
                m_ptrTree.reset();
                m_ptrPager.reset();
                if (m_ptrStorage.get())
                {
                    m_ptrStorage->Close();
                    m_ptrStorage.reset();
                }
            }

            bool COSMBPTreeTagFile::Get(int64_t nId, std::string& sTags)
            {
                if (!m_ptrTree)
                    throw CommonLib::CExcBase("[OSMBPTree] the tag file isn't open");
                auto it = m_ptrTree->find(nId);
                bool bFound = !it.IsNull();
                if (bFound)
                {
                    const bptreedb::StringValue& value = it.Value();
                    sTags.assign((const char*)value.CStr(), value.StrLen());
                }
                m_ptrTree->CheckCache();    // find only adds the read nodes to the cache
                return bFound;
            }

            bool COSMBPTreeTagFile::Get(int64_t nId, std::vector<std::pair<std::string, std::string> >& tags)
            {
                std::string sTags;
                if (!Get(nId, sTags))
                {
                    tags.clear();
                    return false;
                }
                DecodeTags(sTags.data(), sTags.size(), tags);
                return true;
            }
        }
    }
}
