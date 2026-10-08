#pragma once

#include "../GeoDatabase.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        typedef std::shared_ptr<class IRasterReader> IRasterReaderPtr;

        // Pixel -> map transform of a raster (north-up, no rotation):
        //   X = originX + col * pixelSizeX,  Y = originY + row * pixelSizeY
        // origin is the outer corner of the upper-left pixel; pixelSizeY is negative for north-up images.
        struct SRasterGeoInfo
        {
            bool   bGeoreferenced = false;
            double originX = 0.;
            double originY = 0.;
            double pixelSizeX = 1.;
            double pixelSizeY = -1.;
            int    epsgCode = 0;          // 0 - unknown / user defined
            std::string sSource;          // "geotiff", "worldfile" or empty
        };

        // Format reader used by CRasterDataset / CRasterCursor (one reader per cursor, not thread safe).
        // ReadWindow writes pixel-interleaved samples (b0 b1 .. bN per pixel), rows top to bottom,
        // in GetPixelType() / GetBandCount() of the reader.
        class IRasterReader
        {
        public:
            IRasterReader(){}
            virtual ~IRasterReader(){}

            virtual void Open(const std::string& sFilePath) = 0;   // UTF-8 path
            virtual IRasterReaderPtr Clone() const = 0;            // new independent reader on the same file

            virtual int GetWidth() const = 0;
            virtual int GetHeight() const = 0;
            virtual int GetBandCount() const = 0;                  // bands delivered by ReadWindow
            virtual eRasterPixelType GetPixelType() const = 0;     // samples delivered by ReadWindow
            virtual int GetSourceBandCount() const = 0;            // as stored in the file
            virtual eRasterPixelType GetSourcePixelType() const = 0;
            virtual const SRasterGeoInfo& GetGeoInfo() const = 0;

            // col, row - upper-left source pixel; width x height output pixels; every step-th source pixel is taken
            virtual void ReadWindow(int col, int row, int width, int height, int step, void* pDst) = 0;
        };
    }
}
