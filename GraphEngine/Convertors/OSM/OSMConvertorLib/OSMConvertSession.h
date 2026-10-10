#pragma once
#include "OSMConvertorLib.h"
#include "OSMSchema.h"
#include "OSMGeometry.h"
#include "OSMNodeStore.h"
#include <memory>
#include <set>

namespace GraphEngine {
    namespace Convertors {

        // Converts the datasets of an OSM map into the workspace. Passes over the file for a set of datasets:
        //  1. nodes + relations: the node coordinates go to the node store (once per session), point features,
        //     routes / restrictions, the multipolygon relations of the polygon layers are collected;
        //  2. ways: line / polygon features, the geometry of the multipolygon members;
        //  3. multipolygons are built from their member ways.
        // All the tables of one Convert / ConvertDataset call are written in one transaction.
        class COSMConvertSession : public IOSMConvertSession
        {
        public:
            COSMConvertSession(const COSMSchema& schema, const SOSMConvertSettings& settings, IOSMMapPtr ptrOSMMap,
                               Cartography::IMapPtr ptrMap, GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace);
            virtual ~COSMConvertSession();

            virtual IOSMMapPtr                         GetOSMMap() const { return m_ptrOSMMap; }
            virtual Cartography::IMapPtr               GetMap() const { return m_ptrMap; }
            virtual GeoDatabase::IDatabaseWorkspacePtr GetWorkspace() const { return m_ptrWorkspace; }

            virtual void Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);
            virtual void ConvertDataset(IOSMDatasetPtr ptrDataset, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);
            virtual bool IsConverted(IOSMDatasetPtr ptrDataset) const;
            virtual void Close();

            // the node store is built (the next conversions don't store the nodes again)
            bool HasNodeStore() const { return m_ptrNodes.get() && m_ptrNodes->IsFinished(); }

        private:
            void ConvertDatasets(const std::vector<IOSMDatasetPtr>& vecDatasets, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel);
            std::string DatasetKey(const IOSMDatasetPtr& ptrDataset) const;
            void AddMapLayer(const IOSMLayer& osmLayer, GeoDatabase::ITablePtr ptrTable);

        private:
            COSMSchema                          m_schema;
            SOSMConvertSettings                 m_settings;
            IOSMMapPtr                          m_ptrOSMMap;
            Cartography::IMapPtr                m_ptrMap;
            GeoDatabase::IDatabaseWorkspacePtr  m_ptrWorkspace;
            COSMProjection                      m_projection;
            std::unique_ptr<COSMNodeStore>      m_ptrNodes;
            std::set<std::string>               m_setConverted;
            std::vector<std::pair<int, Cartography::ILayerPtr> > m_vecMapLayers;   // added layers with their rank
        };
    }
}
