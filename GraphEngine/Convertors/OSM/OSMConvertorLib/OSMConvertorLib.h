#pragma once

#include "../../../GeoDatabase/GeoDatabase.h"
#include "../../../DisplayLib/DisplayLib.h"
#include "../../../CartographyLib/Cartography.h"

// OpenStreetMap (.osm XML, .osm.pbf) -> GraphEngine workspace and map.
//
// OSM has no layers or tables: only nodes, ways and relations with free key=value tags.
// The converter classifies them into datasets, the object model of a file (IOSMConvertor::ReadMap,
// a fast pass over the tags only):
//
//   IOSMMap               the file: format, bounds, numbers of nodes / ways / relations
//    +- IOSMLayer         thematic spatial layer (Roads, Buildings, Water ...): geometry type, table name
//    +- IOSMTable         non spatial table (Routes + members, Turn restrictions, Tags)
//        +- IOSMTagKey        OSM key the dataset is made of (highway, landuse, route ...)
//            +- IOSMTagValue  value of the key (motorway, residential, bus ...) with the number of features
//
// Every level has the Enabled flag: a feature is converted when its dataset, key and value are enabled.
// The datasets can be removed from the map, the map is serializable (the selection can be saved).
//
// Conversion: IOSMConvertor::Convert - everything enabled at once, or IOSMConvertSession -
// the datasets one by one (the node coordinates are read once and kept in a temporary file for the session).

namespace GraphEngine {
    namespace Convertors {

        typedef std::shared_ptr<class IProgressUpdater> IProgressUpdaterPtr;
        typedef std::shared_ptr<class IOSMConvertor> IOSMConvertorPtr;
        typedef std::shared_ptr<class IOSMConvertSession> IOSMConvertSessionPtr;
        typedef std::shared_ptr<class IOSMMap> IOSMMapPtr;
        typedef std::shared_ptr<class IOSMDataset> IOSMDatasetPtr;
        typedef std::shared_ptr<class IOSMLayer> IOSMLayerPtr;
        typedef std::shared_ptr<class IOSMTable> IOSMTablePtr;
        typedef std::shared_ptr<class IOSMTagKey> IOSMTagKeyPtr;
        typedef std::shared_ptr<class IOSMTagValue> IOSMTagValuePtr;

        enum eOSMFileFormat
        {
            OSMFormatUnknown = 0,
            OSMFormatXML     = 1,   // .osm
            OSMFormatPBF     = 2    // .pbf, .osm.pbf
        };

        enum eOSMGeometryType
        {
            OSMGeometryPoint   = 0,   // nodes
            OSMGeometryLine    = 1,   // ways
            OSMGeometryPolygon = 2    // closed ways, multipolygon / boundary relations
        };

        enum eOSMTableType
        {
            OSMTableRoutes       = 0,   // route relations (bus, tram, hiking ...) + the table of their members
            OSMTableRestrictions = 1,   // turn restrictions: from way, via node / way, to way
            OSMTableTags         = 2    // all the tags (key / value) of the converted objects
        };

        enum eOSMStage
        {
            OSMStageReadMap        = 0,   // analysis of the tags
            OSMStageNodes          = 1,   // nodes and relations
            OSMStageSortNodes      = 2,   // node coordinates of an unsorted file
            OSMStageWays           = 3,
            OSMStageMultipolygons  = 4,
            OSMStageFinish         = 5
        };

        struct SOSMProgress
        {
            eOSMStage   stage = OSMStageReadMap;
            uint64_t    nNodes = 0;       // read in the current pass
            uint64_t    nWays = 0;
            uint64_t    nRelations = 0;
            uint64_t    nFeatures = 0;    // written features / records (conversion)
            std::string sMessage;
        };

        class IProgressUpdater {
        public:
            IProgressUpdater() = default;
            virtual ~IProgressUpdater() = default;
            // called from the converting thread from time to time
            virtual void UpdateStatusInfo(const SOSMProgress& progress) = 0;
        };

        class IOSMTagValue
        {
        public:
            IOSMTagValue(){}
            virtual ~IOSMTagValue(){}

            virtual const std::string&  GetValue() const = 0;
            virtual uint64_t            GetFeatureCount() const = 0;
            virtual bool                GetEnabled() const = 0;
            virtual void                SetEnabled(bool bEnabled) = 0;
        };

        class IOSMTagKey
        {
        public:
            IOSMTagKey(){}
            virtual ~IOSMTagKey(){}

            virtual const std::string&  GetKey() const = 0;
            virtual uint64_t            GetFeatureCount() const = 0;   // all the values
            virtual bool                GetEnabled() const = 0;
            virtual void                SetEnabled(bool bEnabled) = 0;

            // values sorted by the number of features (the biggest first)
            virtual int                 GetValueCount() const = 0;
            virtual IOSMTagValuePtr     GetValue(int nIndex) const = 0;
            virtual IOSMTagValuePtr     FindValue(const std::string& sValue) const = 0;
        };

        // a layer or a table of the conversion result
        class IOSMDataset : public CommonLib::ISerialize
        {
        public:
            IOSMDataset(){}
            virtual ~IOSMDataset(){}

            virtual const std::string&  GetName() const = 0;          // id: "roads", "routes" ...
            virtual const std::string&  GetDisplayName() const = 0;   // "Roads", "Routes" ...
            virtual const std::string&  GetTableName() const = 0;     // table in the destination workspace
            virtual void                SetTableName(const std::string& sTableName) = 0;
            virtual uint64_t            GetFeatureCount() const = 0;  // features / records found by ReadMap
            virtual bool                GetEnabled() const = 0;
            virtual void                SetEnabled(bool bEnabled) = 0;

            virtual int                 GetKeyCount() const = 0;
            virtual IOSMTagKeyPtr       GetKey(int nIndex) const = 0;
            virtual IOSMTagKeyPtr       FindKey(const std::string& sKey) const = 0;

            // the key and the value are enabled (a value which wasn't met by ReadMap is enabled),
            // the Enabled flag of the dataset itself isn't checked
            virtual bool                IsValueEnabled(const std::string& sKey, const std::string& sValue) const = 0;
            // GetEnabled() && IsValueEnabled()
            virtual bool                IsEnabled(const std::string& sKey, const std::string& sValue) const = 0;
        };

        class IOSMLayer : public IOSMDataset
        {
        public:
            IOSMLayer(){}
            virtual ~IOSMLayer(){}
            virtual eOSMGeometryType    GetGeometryType() const = 0;
        };

        // Routes: the keys "route" (bus, tram ...); the members go to the table GetTableName() + "_members".
        // Restrictions: the key "restriction" (no_left_turn ...).
        // Tags: the key "key" lists the OSM keys (name, addr:street ...), the records of the disabled keys aren't written;
        // the tags of the objects converted into the other datasets are written.
        class IOSMTable : public IOSMDataset
        {
        public:
            IOSMTable(){}
            virtual ~IOSMTable(){}
            virtual eOSMTableType       GetTableType() const = 0;
        };

        class IOSMMap : public CommonLib::ISerialize
        {
        public:
            IOSMMap(){}
            virtual ~IOSMMap(){}

            virtual const std::string&  GetPath() const = 0;
            virtual eOSMFileFormat      GetFormat() const = 0;
            virtual const CommonLib::bbox& GetBounds() const = 0;     // longitude / latitude of the nodes
            virtual uint64_t            GetNodeCount() const = 0;
            virtual uint64_t            GetWayCount() const = 0;
            virtual uint64_t            GetRelationCount() const = 0;
            // the objects are sorted by id: nodes, then ways, then relations (planet / extracts are)
            virtual bool                IsSorted() const = 0;

            virtual int                 GetLayerCount() const = 0;
            virtual IOSMLayerPtr        GetLayer(int nIndex) const = 0;
            virtual IOSMLayerPtr        FindLayer(const std::string& sName) const = 0;
            virtual void                RemoveLayer(const std::string& sName) = 0;

            virtual int                 GetTableCount() const = 0;
            virtual IOSMTablePtr        GetTable(int nIndex) const = 0;
            virtual IOSMTablePtr        FindTable(const std::string& sName) const = 0;
            virtual void                RemoveTable(const std::string& sName) = 0;
        };

        // conversion of the datasets of an OSM map one by one into a workspace (and the map)
        class IOSMConvertSession
        {
        public:
            IOSMConvertSession(){}
            virtual ~IOSMConvertSession(){}

            virtual IOSMMapPtr                 GetOSMMap() const = 0;
            virtual Cartography::IMapPtr       GetMap() const = 0;
            virtual GeoDatabase::IDatabaseWorkspacePtr GetWorkspace() const = 0;

            // all the enabled datasets which aren't converted yet (one set of passes over the file)
            virtual void Convert(IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;
            // one dataset of the map (also a disabled one, its keys / values selection is used)
            virtual void ConvertDataset(IOSMDatasetPtr ptrDataset, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;
            virtual bool IsConverted(IOSMDatasetPtr ptrDataset) const = 0;
            // deletes the temporary files (also done by the destructor)
            virtual void Close() = 0;
        };

        struct SOSMConvertSettings
        {
            std::string sTempDir;              // temporary files (node coordinates), empty - the system temporary folder
            uint32_t    nNodeCacheMB = 512;    // memory for the node coordinates, the rest is in a temporary file
            bool        bWebMercator = true;   // geometry in Web Mercator (EPSG:3857), false - longitude / latitude (EPSG:4326)
            std::string sNameLanguage;         // "en", "de" ...: name:<language> is used for the names when it exists
            bool        bAddLayersToMap = true;
        };

        class IOSMConvertor {
            public:
            IOSMConvertor(){}
            virtual ~IOSMConvertor(){}

            // reads the file (.osm or .pbf): metadata and the dataset hierarchy with the numbers of features,
            // everything is enabled
            virtual IOSMMapPtr ReadMap(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            // tables of the datasets are created in the workspace (SQLite ...); ptrMap can be null
            virtual IOSMConvertSessionPtr CreateSession(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap, GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace) = 0;

            // the enabled datasets of the OSM map: CreateSession + Convert
            virtual void Convert(IOSMMapPtr ptrOSMMap, Cartography::IMapPtr ptrMap,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            // the whole file: ReadMap + Convert
            virtual void ConvertFromXML(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            virtual void ConvertFromPBF(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

        };


    }
}
