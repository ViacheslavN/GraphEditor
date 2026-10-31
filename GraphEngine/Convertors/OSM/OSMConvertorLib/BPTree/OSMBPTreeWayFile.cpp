#include "OSMBPTreeWayFile.h"

#include "../../../../../bptreedb/StorageLib/FileStorage.h"
#include "../../../../../bptreedb/StorageLib/FilePager.h"
#include "../../../../../bptreedb/StorageLib/WriteStreamPage.h"
#include "../../../../../bptreedb/StorageLib/ReadStreamPage.h"
#include "../../../../../CommonLib/alloc/simpleAlloc.h"
#include "../../../../../GeometryCompression/CompressedShapeReader.h"

#include <cmath>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            void WriteWayHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage, const SOSMBPTreeWayHeader& header)
            {
                try
                {
                    bptreedb::storage::TWriteStreamPage stream(ptrPager);
                    stream.Open(nPage, true);
                    stream.Write(SOSMBPTreeWayHeader::Magic);
                    stream.Write(SOSMBPTreeWayHeader::Version);
                    stream.Write(header.nPageSize);
                    stream.Write(header.nTreeInfoPage);
                    stream.Write(header.nWayCount);
                    stream.Write(header.nRefCount);
                    stream.Write(header.nMissingRefs);
                    stream.Write(header.nWaysWithMissing);
                    stream.Write(header.nWaysWithoutShape);
                    stream.Write(header.nShapeBytes);
                    stream.Write(header.nMinId);
                    stream.Write(header.nMaxId);
                    stream.Close();
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to write the way header, page {0}", nPage, exc);
                    throw;
                }
            }

            SOSMBPTreeWayHeader ReadWayHeader(bptreedb::storage::IFilePagerPtr ptrPager, int64_t nPage)
            {
                try
                {
                    bptreedb::storage::TReadStreamPage stream(ptrPager);
                    stream.Open(nPage);
                    if (stream.ReadIntu64() != SOSMBPTreeWayHeader::Magic)
                        throw CommonLib::CExcBase("not an OSM way file");
                    uint32_t nVersion = stream.ReadIntu32();
                    if (nVersion != SOSMBPTreeWayHeader::Version)
                        throw CommonLib::CExcBase("unsupported version {0}", nVersion);

                    SOSMBPTreeWayHeader header;
                    header.nPageSize = stream.ReadIntu32();
                    header.nTreeInfoPage = stream.ReadInt64();
                    header.nWayCount = stream.ReadIntu64();
                    header.nRefCount = stream.ReadIntu64();
                    header.nMissingRefs = stream.ReadIntu64();
                    header.nWaysWithMissing = stream.ReadIntu64();
                    header.nWaysWithoutShape = stream.ReadIntu64();
                    header.nShapeBytes = stream.ReadIntu64();
                    header.nMinId = stream.ReadInt64();
                    header.nMaxId = stream.ReadInt64();
                    return header;
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to read the way header, page {0}", nPage, exc);
                    throw;
                }
            }

            COSMBPTreeWayFile::COSMBPTreeWayFile() {}

            COSMBPTreeWayFile::~COSMBPTreeWayFile()
            {
                try
                {
                    Close();
                }
                catch (...)
                {
                }
            }

            void COSMBPTreeWayFile::Open(const std::string& sWayFile, uint32_t nPageSize, uint32_t nTreeCacheNodes, uint32_t nPagerCachePages)
            {
                try
                {
                    Close();
                    m_ptrAlloc.reset(new CommonLib::CSimpleAlloc(false));
                    m_ptrStorage.reset(new bptreedb::storage::CFileStorage(m_ptrAlloc, 1, bptreedb::storage::IStorageCipherPtr()));
                    m_ptrStorage->Open(sWayFile.c_str(), false, 0, nPageSize);
                    m_ptrPager.reset(new bptreedb::storage::CFilePager(m_ptrAlloc, m_ptrStorage, nPagerCachePages));
                    m_header = ReadWayHeader(m_ptrPager, 0);
                    if (m_header.nPageSize != nPageSize)
                        throw CommonLib::CExcBase("the page size of the file is {0}, not {1}", m_header.nPageSize, nPageSize);
                    m_ptrTree.reset(new TWayShapeTree(m_header.nTreeInfoPage, m_ptrPager, m_ptrAlloc, nTreeCacheNodes, nPageSize, ShapeInlineSize(nPageSize)));
                }
                catch (std::exception& exc)
                {
                    m_ptrTree.reset();
                    m_ptrPager.reset();
                    m_ptrStorage.reset();
                    CommonLib::CExcBase::RegenExcT("[OSMBPTree] failed to open the way file {0}", sWayFile, exc);
                    throw;
                }
            }

            void COSMBPTreeWayFile::Close()
            {
                m_ptrTree.reset();
                m_ptrPager.reset();
                if (m_ptrStorage.get())
                {
                    m_ptrStorage->Close();
                    m_ptrStorage.reset();
                }
            }

            bool COSMBPTreeWayFile::GetBlob(int64_t nId, std::vector<byte_t>& blob)
            {
                if (!m_ptrTree)
                    throw CommonLib::CExcBase("[OSMBPTree] the way file isn't open");
                bptreedb::IBlobPtr ptrBlob = m_ptrTree->GetBlob(nId);
                m_ptrTree->CheckCache();    // find only adds the read nodes to the cache
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

            bool COSMBPTreeWayFile::GetPoints(int64_t nId, std::vector<SOSMPointE7>& points)
            {
                std::vector<byte_t> blob;
                if (!GetBlob(nId, blob))
                {
                    points.clear();
                    return false;
                }
                DecodePoints(blob.data(), blob.size(), points);
                return true;
            }

            void COSMBPTreeWayFile::DecodePoints(const byte_t* pBlob, size_t nSize, std::vector<SOSMPointE7>& points)
            {
                GeometryCompression::CCompressedShapeReader reader;
                reader.Open(pBlob, nSize);
                points.clear();
                points.reserve(reader.PointCount());
                CommonLib::GisXYPoint pt;
                while (reader.NextPoint(pt))
                    points.push_back(SOSMPointE7((int32_t)std::llround(pt.y * SOSMPointE7::Scale), (int32_t)std::llround(pt.x * SOSMPointE7::Scale)));
            }
        }
    }
}
