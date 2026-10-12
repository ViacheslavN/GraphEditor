#pragma once

#include "../../../GeoDatabase/GeoDatabase.h"
#include "../../../DisplayLib/DisplayLib.h"
#include "../../../CartographyLib/Cartography.h"
#include "../../../../GeometryCompression/ShapeCompressor.h"
#include "LayerColumnExcludeList.h"

// OpenStreetMap (.osm XML, .osm.pbf) -> GraphEngine workspace and map.
//
// OSM has no layers or tables: only nodes, ways and relations with free key=value tags.
// The converter classifies them into datasets, the object model of a file (IOSMConvertor::Analyze,
// one pass over the file without the node coordinates):
//
//   IOSMMap               the file: format, bounds, numbers of nodes / ways / relations
//    +- IOSMLayer         thematic spatial layer (Roads, Buildings, Water ...): geometry type, table name
//    +- IOSMTable         non spatial table (Routes + members, Turn restrictions, Tags)
//        +- SOSMColumn        column of the table: the columns of the converter (osm_id, class, type ...),
//                             the fields of the schema and (Analyze) the OSM keys met on the features of the layer
//        +- IOSMTagKey        OSM key the dataset is made of (highway, landuse, route ...)
//            +- IOSMTagValue  value of the key (motorway, residential, bus ...) with the number of features
//
// The map is serializable. What isn't converted is CLayerColumnExcludeList: the excluded layers / tables and
// the excluded columns of a layer, an empty list - everything.
//
// Conversion: IOSMConvertor::Convert - all the datasets which aren't excluded in one set of passes over the file
// (nodes -> node store + points + relations, ways, multipolygons).
//
// Split data (IOSMConvertor::Split, OSMSplitFormat.h): the OSM file is split once into a folder - the nodes, the ways with
// their coordinates, the relations (data + index by id) and the metadata (the analysis, the symbols); Analyze / CreateMap
// open it (osm_split.xml) at once and Convert exports it without the OSM file and without node lookups.

namespace GraphEngine {
    namespace Convertors {

        typedef std::shared_ptr<class IProgressUpdater> IProgressUpdaterPtr;
        typedef std::shared_ptr<class IOSMConvertor> IOSMConvertorPtr;
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
            OSMFormatPBF     = 2,   // .pbf, .osm.pbf
            OSMFormatSplit   = 3    // the split data: the folder with osm_split.xml (the path of the map is that file)
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
            OSMStageAnalyze        = 0,   // analysis of the file: datasets, columns, counts
            OSMStageNodes          = 1,   // nodes and relations
            OSMStageSortNodes      = 2,   // node coordinates of an unsorted file
            OSMStageWays           = 3,
            OSMStageMultipolygons  = 4,
            OSMStageFinish         = 5,
            OSMStageSplit          = 6    // splitting the file into the split data (nodes, ways, relations, metadata)
        };

        // A part of the work of the current stage, to see where the time goes (the bottleneck):
        // the objects / rows / blocks, the data read or written and the time spent on it.
        struct SOSMProgressItem
        {
            std::string sName;              // "Reading the file", "Nodes", "Line features" ...
            std::string sInfo;              // what is done: the file, the tables, the threads
            uint64_t    nCount = 0;         // objects / rows / blocks, 0 - not counted
            uint64_t    nBytes = 0;         // the data read / written, 0 - none
            double      dSeconds = 0.;      // the time spent on it in the stage
            bool        bThreads = false;   // dSeconds is summed over several threads (CPU time of a pool, can exceed the stage time)
        };

        // a finished stage of the task
        struct SOSMStageSummary
        {
            eOSMStage   stage = OSMStageAnalyze;
            std::string sMessage;
            double      dSeconds = 0.;
            std::vector<SOSMProgressItem> vecItems;
        };

        struct SOSMProgress
        {
            eOSMStage   stage = OSMStageAnalyze;
            uint64_t    nNodes = 0;       // read in the current pass
            uint64_t    nWays = 0;
            uint64_t    nRelations = 0;
            uint64_t    nFeatures = 0;    // written features / records (conversion)
            std::string sMessage;

            // reading of the file (.pbf; an .osm file has no byte counts)
            int         nPass = 0;        // the pass over the file, from 1 (0 - the file isn't read now)
            int         nPassCount = 0;   // the passes of this task
            uint64_t    nBytesRead = 0;   // the data of the file processed in this pass
            uint64_t    nBytesTotal = 0;  // the data this pass reads: the whole file or (the ways pass) only the blocks with ways,
                                          // 0 - unknown
            uint64_t    nFileSize = 0;

            // the stages after the reading: multipolygons built / layers with the spatial index built, of all
            uint64_t    nStepDone = 0;
            uint64_t    nStepTotal = 0;

            // the details of the current stage (what is read / written where, how long it takes) and the finished stages
            double      dStageSeconds = 0.;
            std::vector<SOSMProgressItem> vecItems;
            std::vector<SOSMStageSummary> vecStages;
        };

        class IProgressUpdater {
        public:
            IProgressUpdater() = default;
            virtual ~IProgressUpdater() = default;
            // called from the converting thread from time to time
            virtual void UpdateStatusInfo(const SOSMProgress& progress) = 0;
        };

        // a column of the table of a dataset
        struct SOSMColumn
        {
            std::string sName;              // the column in the destination table
            std::string sKey;               // the OSM key of the value, empty - made by the converter (osm_id, osm_type, name, class, type,
                                            // the columns of the tables)
            bool        bInteger = false;   // int64, otherwise text
            bool        bRequired = false;  // always written, can't be excluded (osm_id; type - the map styles draw by it)
            uint64_t    nCount = 0;         // features / records having the value (Analyze), 0 - unknown
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
            virtual uint64_t            GetFeatureCount() const = 0;  // features / records found by Analyze

            // the columns of the table (the converter columns first, the OSM keys sorted by the number of features)
            virtual int                 GetColumnCount() const = 0;
            virtual const SOSMColumn&   GetColumn(int nIndex) const = 0;
            virtual int                 FindColumn(const std::string& sName) const = 0;   // -1 - no such column

            virtual int                 GetKeyCount() const = 0;
            virtual IOSMTagKeyPtr       GetKey(int nIndex) const = 0;
            virtual IOSMTagKeyPtr       FindKey(const std::string& sKey) const = 0;

            // the key and the value are enabled (a value which wasn't met by Analyze is enabled)
            virtual bool                IsValueEnabled(const std::string& sKey, const std::string& sValue) const = 0;
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
            // the file was read by Analyze: the counts / bounds / OSM key columns are known; false - a map by CreateMap,
            // the counts are 0, only the converter / schema columns, every dataset is converted (the empty ones are not added to the map)
            virtual bool                IsScanned() const = 0;

            virtual int                 GetLayerCount() const = 0;
            virtual IOSMLayerPtr        GetLayer(int nIndex) const = 0;
            virtual IOSMLayerPtr        FindLayer(const std::string& sName) const = 0;
            virtual void                RemoveLayer(const std::string& sName) = 0;

            virtual int                 GetTableCount() const = 0;
            virtual IOSMTablePtr        GetTable(int nIndex) const = 0;
            virtual IOSMTablePtr        FindTable(const std::string& sName) const = 0;
            virtual void                RemoveTable(const std::string& sName) = 0;
        };

        // how the floating point coordinates are turned into the integers of the compressed geometry
        // (they are stored as integers of 10^-k, k - the scale exponent)
        enum eOSMCompressScale
        {
            OSMCompressScaleAuto    = 0,   // by the units of the output coordinate system: 1 cm (Web Mercator), 1e-7 degree (CEnvelope::GetCompressParams)
            OSMCompressScaleManual  = 1,   // nManualScaleExponent
            OSMCompressScaleMaximum = 2    // the largest k which keeps all the digits of the doubles through int64 (|x| * 10^k <= 2^53)
        };

        struct SOSMCompressSettings
        {
            bool              bEnabled = true;
            eOSMCompressScale scale = OSMCompressScaleAuto;
            int               nManualScaleExponent = 2;   // -22..22
        };

        // the storage of the node coordinates (in the temporary folder) while the ways are built
        enum eOSMNodeStore
        {
            OSMNodeStoreAuto   = 0,   // flat for the files of a continent / the planet (64-bit), sorted otherwise
            OSMNodeStoreSorted = 1,   // the nodes sorted by id, 16 bytes per node: extracts
            OSMNodeStoreFlat   = 2    // a file indexed by the node id, 8 bytes per id up to the biggest one (~100 GB for the
                                      // current ids, sparse: the gaps take no disk space), the fastest lookup: the planet
        };

        struct SOSMConvertSettings
        {
            std::string sTempDir;              // UTF-8, the temporary files (node coordinates ~115 GB for the planet, multipolygon ways,
                                               // spatial index runs), created if it doesn't exist; empty - the system temporary folder
            uint32_t    nNodeCacheMB = 512;    // memory for the node coordinates (the sorted node store, the caches of the lookups of the flat one)
                                               // and the multipolygon ways, the rest is in temporary files
            // the splitting with the flat node store (the planet): the node coordinates are loaded into memory by parts of this size,
            // the ways get them without random reads of the disk (a part per pass over the ways); 0 - 60% of the physical memory
            uint32_t    nWaysNodeMemoryMB = 0;
            uint32_t    nNodeWriteBufferMB = 1024;   // the flat node store: the nodes are collected in memory and written by big blocks
                                               // in the background (two halves: one is filled while the other one is written)
            bool        bWebMercator = true;   // geometry in Web Mercator (EPSG:3857), false - longitude / latitude (EPSG:4326)
            std::string sNameLanguage;         // "en", "de" ...: name:<language> is used for the names when it exists
            bool        bAddLayersToMap = true;
            // the geometry compression (GeometryCompression, NumLen coded deltas, about 4 times smaller)
            SOSMCompressSettings compression;
            // the threads decoding the blocks of a .pbf file, 0 - by the number of the processors
            int         nThreads = 0;
            eOSMNodeStore nodeStore = OSMNodeStoreAuto;
        };

        class IOSMConvertor {
            public:
            IOSMConvertor(){}
            virtual ~IOSMConvertor(){}

            // analyzes the file (.osm or .pbf): metadata, the layers / tables with the numbers of features,
            // their columns (the OSM keys met on the features of every layer) and the key / value hierarchy
            virtual IOSMMapPtr Analyze(const std::string& path, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            // a folder of split data (osm_split.xml) is opened at once: its map is analyzed already

            // all the datasets of the schema without reading the file (no counts, only the converter / schema columns):
            // the conversion makes one pass less; the split data: its analyzed map
            virtual IOSMMapPtr CreateMap(const std::string& path) = 0;

            // the parameters of the geometry compression for the map (by the settings.compression): the extent is the bounds
            // of the map (Analyze) or the world (a map which isn't read) in the output coordinate system;
            // the result is used when the compression is enabled
            virtual GeometryCompression::SShapeCompressParams GetCompressParams(IOSMMapPtr ptrOSMMap) const = 0;

            // the datasets of the OSM map without the excluded ones / their excluded columns: the tables are created in the workspace
            // (SQLite ...) and filled in one set of passes over the file; ptrMap can be null
            virtual void Convert(IOSMMapPtr ptrOSMMap, const CLayerColumnExcludeList& excludeList, Cartography::IMapPtr ptrMap,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            // Splits an OSM file (.osm / .pbf) into a folder (created; the files there are replaced): the nodes, the ways with
            // their coordinates, the relations with the indexes by id, the metadata (the analysis of the file, the symbols of
            // the layers), see OSMSplitFormat.h. One pass over a sorted file (planet / extracts), two passes otherwise.
            // bKeepNodeFile - nodes.flat (the coordinates of all the nodes, not needed by the export) stays in the folder.
            // Returns the map of the split data (OSMFormatSplit): Convert exports it without the OSM file.
            virtual IOSMMapPtr Split(const std::string& path, const std::string& sFolder, bool bKeepNodeFile,
                                     IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            // the whole file: CreateMap + Convert with nothing excluded
            virtual void ConvertFromXML(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

            virtual void ConvertFromPBF(const std::string& path, Cartography::IMapPtr ptrMap ,
                GeoDatabase::IDatabaseWorkspacePtr ptrDstWorkspace, IProgressUpdaterPtr ptrProgress, Display::ITrackCancelPtr ptrCancel) = 0;

        };


    }
}
