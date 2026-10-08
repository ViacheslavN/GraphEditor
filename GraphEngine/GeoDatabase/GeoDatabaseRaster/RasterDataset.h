#pragma once

#include "../DataSetBase.h"
#include "RasterReader.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        // Raster file dataset. The format reader is chosen by the file content (currently TIFF / GeoTIFF).
        // Search() opens an independent reader per cursor, so cursors can be used from different threads.
        //
        // Spatial reference: EPSG code from GeoTIFF keys -> <name>.prj next to the file -> guessed from the extent.
        // Pixel type / band count are those of the blocks returned by the cursor (see CTIFFReader:
        // paletted -> RGB uint8, 1/2/4 bit -> uint8 per sample, ...); the stored ones are GetSource*.
        class CRasterDataset : public IDataSetBase<IRasterDataset>
        {
        public:
            typedef IDataSetBase<IRasterDataset> TBase;

            CRasterDataset(CommonLib::CGuid workspaceId, const std::string& sFilePath, const std::string& sName, const std::string& sViewName);
            virtual ~CRasterDataset();

            static bool IsSupportedFile(const std::string& sFilePath);

            // IRasterDataset
            virtual Geometry::ISpatialReferencePtr	 GetSpatialReference() const;
            virtual double                           GetResolution() const;
            virtual eRasterPixelType                 GetPixelType() const;
            virtual int                              GetBandCount() const;
            virtual IRasterCursorPtr				 Search(IRasterSpatialFilterPtr ptrFilter);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;

            // CRasterDataset
            int GetWidth() const;
            int GetHeight() const;
            eRasterPixelType GetSourcePixelType() const;
            int GetSourceBandCount() const;
            Geometry::IEnvelopePtr GetExtent() const;
            bool IsGeoreferenced() const;
            const SRasterGeoInfo& GetGeoInfo() const;
            const std::string& GetFilePath() const;

        private:
            static IRasterReaderPtr CreateReader(const std::string& sFilePath);
            void InitSpatialReference();

        private:
            std::string m_sFilePath;
            IRasterReaderPtr m_ptrReader;
            SRasterGeoInfo m_geoInfo;
            CommonLib::bbox m_bbox;
            Geometry::ISpatialReferencePtr m_ptrSpatialReference;
            Geometry::IEnvelopePtr m_ptrExtent;
        };

        typedef std::shared_ptr<CRasterDataset> CRasterDatasetPtr;
    }
}
