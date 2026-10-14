#pragma once
#include "GeoDatabase.h"
#include <functional>

namespace GraphEngine {
    namespace GeoDatabase {

        // the options of CTableCopier::CopySpatialTable
        struct STableCopyOptions
        {
            static const int AutoScale = -1000;

            // the shapes are written compressed (GeometryCompression blobs: integer coordinates, about 4 times
            // smaller), the coordinates rounded to 10^-nScaleExponent; AutoScale - by the units of the coordinate
            // system of the source (degrees 1e-7, meters 1 cm...). A shape that is compressed already is copied as
            // it is; one that can't be compressed (Z, M, curves, out of the integer range) - raw.
            bool bCompressGeometry = false;
            int  nScaleExponent = AutoScale;
        };

        // Copies tables between workspaces, e.g. shapefile -> SQLite
        class CTableCopier
        {
        public:
            // nCopied - rows copied so far, return false to cancel the copy
            typedef std::function<bool(int64_t nCopied)> TProgress;

            typedef STableCopyOptions SCopyOptions;

            // Creates a spatial table sTargetName (fields, spatial index, metadata) in ptrTarget and copies all rows
            // of ptrSource into it. Returns the new table. Throws on error or cancel.
            static ITablePtr CopySpatialTable(ITablePtr ptrSource, IDatabaseWorkspacePtr ptrTarget, const std::string& sTargetName,
                                              TProgress progress = TProgress(), int64_t nProgressStep = 1000,
                                              const SCopyOptions& options = SCopyOptions());

            // a name usable as an SQL table name: letters, digits and '_', doesn't start with a digit
            static std::string MakeValidTableName(const std::string& sName);
        };

    }
}
