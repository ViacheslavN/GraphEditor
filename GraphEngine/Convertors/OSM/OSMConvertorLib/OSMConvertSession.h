#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"
#include "OSMGeometry.h"
#include "OSMNodeStore.h"
#include <memory>

namespace GraphEngine {
    namespace Convertors {

        // One conversion of an OSM map into the workspace: all the layers / tables which aren't excluded
        // (CLayerColumnExcludeList) with their columns, in one set of passes over the file:
        //  1. nodes + relations: the node coordinates go to the node store (a flat or a sorted temporary file),
        //     point features, routes / restrictions, the multipolygon relations of the polygon layers are collected;
        //  2. ways: line / polygon features, the geometry of the multipolygon members;
        //  3. multipolygons are built from their member ways.
        // All the tables are written in one transaction, the temporary files are deleted at the end.
        class COSMConvertSession
        {
        public:
            COSMConvertSession(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                               const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                               GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace);
            ~COSMConvertSession();

            void Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);

            // the dataset is converted: not excluded and has features (a map without counts - always)
            bool IsConverted(const IOSMDataset& dataset) const;

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
