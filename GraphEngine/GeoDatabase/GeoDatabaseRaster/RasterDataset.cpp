#include "RasterDataset.h"
#include "RasterCursor.h"
#include "TIFFReader.h"
#include "../../GisGeometry/Envelope.h"
#include "../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"

#include <algorithm>
#include <cmath>
#include <filesystem>

namespace GraphEngine
{
    namespace GeoDatabase {

        CRasterDataset::CRasterDataset(CommonLib::CGuid workspaceId, const std::string& sFilePath, const std::string& sName, const std::string& sViewName) :
            TBase(workspaceId, dtTypeRaster, sName, sViewName)
            , m_sFilePath(sFilePath)
        {
            try
            {
                m_ptrReader = CreateReader(m_sFilePath);
                m_geoInfo = m_ptrReader->GetGeoInfo();

                double x1 = m_geoInfo.originX;
                double x2 = m_geoInfo.originX + m_ptrReader->GetWidth() * m_geoInfo.pixelSizeX;
                double y1 = m_geoInfo.originY;
                double y2 = m_geoInfo.originY + m_ptrReader->GetHeight() * m_geoInfo.pixelSizeY;

                m_bbox.type = CommonLib::bbox_type_normal;
                m_bbox.xMin = (std::min)(x1, x2);
                m_bbox.xMax = (std::max)(x1, x2);
                m_bbox.yMin = (std::min)(y1, y2);
                m_bbox.yMax = (std::max)(y1, y2);

                InitSpatialReference();
                m_ptrExtent = std::make_shared<Geometry::CEnvelope>(m_bbox, m_ptrSpatialReference);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to open raster dataset {0}", sFilePath, exc);
                throw;
            }
        }

        CRasterDataset::~CRasterDataset()
        {

        }

        bool CRasterDataset::IsSupportedFile(const std::string& sFilePath)
        {
            return CTIFFReader::IsTIFFFile(sFilePath);
        }

        IRasterReaderPtr CRasterDataset::CreateReader(const std::string& sFilePath)
        {
            if(CTIFFReader::IsTIFFFile(sFilePath))
            {
                std::shared_ptr<CTIFFReader> ptrReader = std::make_shared<CTIFFReader>();
                ptrReader->Open(sFilePath);
                return ptrReader;
            }

            throw CommonLib::CExcBase("Unsupported raster format, file: {0}", sFilePath);
        }

        void CRasterDataset::InitSpatialReference()
        {
            m_ptrSpatialReference.reset();

            if(m_geoInfo.epsgCode > 0)
            {
                try
                {
                    m_ptrSpatialReference = std::make_shared<Geometry::CSpatialReferenceProj4>(m_geoInfo.epsgCode);
                    if(!m_ptrSpatialReference->IsValid())
                        m_ptrSpatialReference.reset();
                }
                catch (std::exception&)
                {
                    m_ptrSpatialReference.reset();
                }
            }

            if(!m_ptrSpatialReference.get())
            {
                std::filesystem::path prjPath = std::filesystem::u8path(m_sFilePath);
                prjPath.replace_extension(".prj");
                std::error_code ec;
                if(std::filesystem::is_regular_file(prjPath, ec))
                {
                    try
                    {
                        m_ptrSpatialReference = std::make_shared<Geometry::CSpatialReferenceProj4>(prjPath.u8string(), Geometry::eSPRefTypePRJFilePath);
                        if(!m_ptrSpatialReference->IsValid())
                            m_ptrSpatialReference.reset();
                    }
                    catch (std::exception&)
                    {
                        m_ptrSpatialReference.reset();
                    }
                }
            }

            if(!m_ptrSpatialReference.get())
                m_ptrSpatialReference = std::make_shared<Geometry::CSpatialReferenceProj4>(m_bbox);
        }

        Geometry::ISpatialReferencePtr CRasterDataset::GetSpatialReference() const
        {
            return m_ptrSpatialReference;
        }

        double CRasterDataset::GetResolution() const
        {
            return std::fabs(m_geoInfo.pixelSizeX);
        }

        eRasterPixelType CRasterDataset::GetPixelType() const
        {
            return m_ptrReader->GetPixelType();
        }

        int CRasterDataset::GetBandCount() const
        {
            return m_ptrReader->GetBandCount();
        }

        IRasterCursorPtr CRasterDataset::Search(IRasterSpatialFilterPtr ptrFilter)
        {
            try
            {
                return std::make_shared<CRasterCursor>(m_ptrReader->Clone(), ptrFilter, m_ptrSpatialReference);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("RasterDataset failed to search", exc);
                throw;
            }
        }

        void CRasterDataset::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            TBase::Save(pObj);
            pObj->AddPropertyString("FilePath", m_sFilePath);
        }

        int CRasterDataset::GetWidth() const
        {
            return m_ptrReader->GetWidth();
        }

        int CRasterDataset::GetHeight() const
        {
            return m_ptrReader->GetHeight();
        }

        eRasterPixelType CRasterDataset::GetSourcePixelType() const
        {
            return m_ptrReader->GetSourcePixelType();
        }

        int CRasterDataset::GetSourceBandCount() const
        {
            return m_ptrReader->GetSourceBandCount();
        }

        Geometry::IEnvelopePtr CRasterDataset::GetExtent() const
        {
            return m_ptrExtent;
        }

        bool CRasterDataset::IsGeoreferenced() const
        {
            return m_geoInfo.bGeoreferenced;
        }

        const SRasterGeoInfo& CRasterDataset::GetGeoInfo() const
        {
            return m_geoInfo;
        }

        const std::string& CRasterDataset::GetFilePath() const
        {
            return m_sFilePath;
        }
    }
}
