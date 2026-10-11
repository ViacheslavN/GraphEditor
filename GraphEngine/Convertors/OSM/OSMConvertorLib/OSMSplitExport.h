#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"
#include "OSMGeometry.h"

namespace GraphEngine {
    namespace Convertors {

        // Export of the split data (OSMSplitFormat.h) into the workspace: the layers / tables of the map which aren't excluded,
        // without the OSM file and without node lookups (the ways keep their coordinates):
        //  1. nodes.dat      -> point features
        //  2. ways.dat       -> line / polygon features
        //  3. relations.dat  -> routes, restrictions, multipolygons (their member ways are read by ways.idx)
        // All the tables are written in one transaction; the layers are added to the map with the symbols of osm_split.xml.
        class COSMSplitExport
        {
        public:
            COSMSplitExport(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                            const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                            GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace);

            void Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

        private:
            COSMSchema                          m_schema;
            SOSMConvertSettings                 m_settings;
            IOSMMapPtr                          m_ptrOSMMap;
            CLayerColumnExcludeList             m_excludeList;
            Cartography::IMapPtr                m_ptrMap;
            GeoDatabase::IDatabaseWorkspacePtr  m_ptrWorkspace;
            COSMProjection                      m_projection;
        };
    }
}
