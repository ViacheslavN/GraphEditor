#pragma once
#include "GeoDatabase.h"
#include <functional>

namespace GraphEngine {
    namespace GeoDatabase {

        // Copies tables between workspaces, e.g. shapefile -> SQLite
        class CTableCopier
        {
        public:
            // nCopied - rows copied so far, return false to cancel the copy
            typedef std::function<bool(int64_t nCopied)> TProgress;

            // Creates a spatial table sTargetName (fields, spatial index, metadata) in ptrTarget and copies all rows
            // of ptrSource into it. Returns the new table. Throws on error or cancel.
            static ITablePtr CopySpatialTable(ITablePtr ptrSource, IDatabaseWorkspacePtr ptrTarget, const std::string& sTargetName,
                                              TProgress progress = TProgress(), int64_t nProgressStep = 1000);

            // a name usable as an SQL table name: letters, digits and '_', doesn't start with a digit
            static std::string MakeValidTableName(const std::string& sName);
        };

    }
}
