#pragma once
// Platform independent part of the test application: shapefile -> layer, project (xml) save / load.

#include "../../Cartography.h"
#include "../../../GeoDatabase/TableCopier.h"
#include "SymbologyModel.h"
#include <memory>

namespace TestMapDraw
{
    struct SFieldInfo
    {
        std::string sName;
        bool        bText = false;      // string field
        bool        bNumeric = false;   // integer / real field
    };

    struct SAnnotationParams
    {
        std::string sField;              // attribute field drawn as annotation, empty - no annotation
        double      dMinimumScale = 0.;  // drawn only when the map scale denominator <= this value, 0 - on all scales
    };

    // labels of a layer: placed by the label drawer of the map without overlapping
    struct SLabelParams
    {
        std::string sField;                 // label field, empty - no labels
        double      dMinimumScale = 0.;     // drawn only when the map scale denominator <= this value, 0 - on all scales
        double      dFontSize = 3.;         // mm
        GraphEngine::Display::Color color = GraphEngine::Display::Color(0, 0, 0, 255);
        double      dHaloSize = 0.3;        // mm, 0 - no halo
        GraphEngine::Cartography::SLabelingOptions options;
    };

    struct STableInfo
    {
        std::string             sName;
        std::vector<SFieldInfo> vecFields;   // attribute (non geometry) fields
        CommonLib::eShapeType   shapeType = CommonLib::shape_type_null;
    };

    // "Data" tab of the layer properties: the source table and the fields the layer uses
    struct SLayerDataInfo
    {
        struct SField
        {
            std::string sName;
            std::string sType;   // "Integer 64", "Geometry" ...
        };

        std::string sTable;
        std::string sWorkspace;          // name and path / database of the workspace
        std::string sSpatialReference;   // proj4 string, empty - unknown
        std::string sGeometryType;
        CommonLib::bbox extent;          // of the table, null - unknown
        std::vector<SField> vecFields;   // all the fields of the table
        std::vector<std::string> vecOIDFields;     // integer fields - candidates for the OID field
        std::vector<std::string> vecShapeFields;   // geometry fields
        std::string sTableOIDField;      // the defaults of the table
        std::string sTableShapeField;
        std::string sOIDField;           // of the layer, empty - the table default
        std::string sShapeField;
    };

    // map properties dialog
    struct SMapParams
    {
        std::string sName;
        std::string sSpatialReference;   // proj4, empty - none (the coordinates of the layers are used as they are)
        CommonLib::Units units = CommonLib::UnitsUnknown;
        bool        bReferenceScale = false;   // symbols have their size at the reference scale, scaled on other scales
        double      dReferenceScale = 0.;
        GraphEngine::Display::Color background = GraphEngine::Display::Color(255, 255, 255, 255);   // alpha 0 - none
    };

    struct SCoordinateSystemPreset
    {
        std::string sName;
        std::string sProj4;
    };

    // layer settings from the Add layer dialogs
    struct SLayerParams
    {
        SAnnotationParams           annotation;
        SLabelParams                labels;
        std::shared_ptr<SSymbology> ptrSymbology;   // null - default symbol
    };

    // table to query values from (dialogs): shapefile (ANSI path to .shp) or SQLite (UTF-8 path) + table
    struct SDataSource
    {
        bool        bSQLite = false;
        std::string sPath;
        std::string sTable;
    };

    class CMapProject
    {
    public:
        CMapProject();
        ~CMapProject();

        GraphEngine::Cartography::IMapPtr GetMap() const;

        void New();
        // opens the shapefile (full path to .shp) and adds it as a feature layer, returns the layer
        GraphEngine::Cartography::ILayerPtr AddShapefile(const std::string& sFilePathUtf8, const SLayerParams& params = SLayerParams());
        void AddTable(GraphEngine::GeoDatabase::ITablePtr ptrTable, const std::string& sLayerName, const SLayerParams& params = SLayerParams());
        // attribute (non geometry) fields and the geometry type of the shapefile
        static STableInfo GetShapefileInfo(const std::string& sFilePath);

        // distinct values of the field (sorted), at most nMaxCount; pbTruncated - there are more of them
        static std::vector<CommonLib::CVariant> GetUniqueValues(const SDataSource& source, const std::string& sField, size_t nMaxCount, bool* pbTruncated = nullptr);
        static std::vector<CommonLib::CVariant> GetUniqueValues(GraphEngine::GeoDatabase::ITablePtr ptrTable, const std::string& sField, size_t nMaxCount, bool* pbTruncated = nullptr);
        // min / max of the numeric values of the field, false - no numeric values
        static bool GetValueRange(const SDataSource& source, const std::string& sField, double& dMin, double& dMax);
        static bool GetValueRange(GraphEngine::GeoDatabase::ITablePtr ptrTable, const std::string& sField, double& dMin, double& dMax);
        // color of the default symbol of the layer with this index
        static GraphEngine::Display::Color GetLayerColor(int nIndex);
        // opens the raster file (TIFF / GeoTIFF, UTF-8 path) and adds it as a raster layer with the default renderer, returns the layer
        GraphEngine::Cartography::ILayerPtr AddRaster(const std::string& sFilePathUtf8);

        // adds the spatial tables of an SQLite database as layers (sTableName empty - all of them), returns the number of added layers
        // annotation and symbology are set only for the tables which have their fields
        int AddSQLiteDatabase(const std::string& sDatabasePath, const std::string& sTableName = std::string(), const SLayerParams& params = SLayerParams());
        // the layers made by a converter (OSM ...) in a separate map: moved into the project map (on the top),
        // the workspace of their tables is registered for the project save / load; returns the number of layers
        int AddConvertedLayers(GraphEngine::GeoDatabase::IWorkspacePtr ptrWorkspace, GraphEngine::Cartography::IMapPtr ptrSourceMap);
        // spatial tables of an SQLite database with their attribute fields (UTF-8 path)
        static std::vector<STableInfo> GetSQLiteTables(const std::string& sDatabasePath);

        // copies the shapefile into an SQLite database (created if it doesn't exist) as a new spatial table,
        // returns the table name; progress gets the number of copied features and can cancel
        static std::string ConvertShapefileToSQLite(const std::string& sShapefilePath, const std::string& sDatabasePath,
                                                    GraphEngine::GeoDatabase::CTableCopier::TProgress progress = GraphEngine::GeoDatabase::CTableCopier::TProgress(),
                                                    int64_t* pnCopied = nullptr);

        // extent of the layer in the map coordinate system (a point / zero-size extent is expanded a bit),
        // false - the layer has no extent or it can't be projected
        bool GetLayerExtent(int nLayerIndex, CommonLib::bbox& bb) const;
        bool GetLayerExtent(GraphEngine::Cartography::ILayerPtr ptrLayer, CommonLib::bbox& bb) const;

        // ---- layer tree (group layers); change the layers when the map isn't drawn (CMapDrawer::StopDraw)

        // the map has a layer with data (feature / raster) at any level: the first one sets the map coordinate system
        bool HasDataLayers() const;
        // empty group on the top of the map (ptrParent null) or of the group
        GraphEngine::Cartography::IGroupLayerPtr AddGroupLayer(const std::string& sName, GraphEngine::Cartography::IGroupLayerPtr ptrParent = GraphEngine::Cartography::IGroupLayerPtr());
        // the list which has the layer: the map layers or the children of a group; null - not in the map
        GraphEngine::Cartography::ILayersPtr FindParentList(GraphEngine::Cartography::ILayerPtr ptrLayer) const;
        void RemoveLayer(GraphEngine::Cartography::ILayerPtr ptrLayer);
        // moves the layer into the list (the map layers or the children of a group): above or below the neighbour
        // (the drawing order: above - drawn later), neighbour null - on the top of the list;
        // false - impossible (a group into itself or into its child group, the neighbour isn't in the list)
        bool MoveLayer(GraphEngine::Cartography::ILayerPtr ptrLayer, GraphEngine::Cartography::ILayersPtr ptrTargetList,
                       GraphEngine::Cartography::ILayerPtr ptrNeighbour, bool bAboveNeighbour);
        // the layer is the group or one of its children at any level
        static bool IsInGroup(GraphEngine::Cartography::ILayerPtr ptrLayer, GraphEngine::Cartography::ILayerPtr ptrGroup);

        // ---- map properties: the coordinate system of the map is set by the first layer with data,
        // the other layers are projected into it while drawn

        SMapParams GetMapParams() const;
        // throws on a wrong coordinate system (the map isn't changed then); the full extent is calculated again
        void ApplyMapParams(const SMapParams& params);
        // WGS 84, Web Mercator, UTM zone of the center of the map, the coordinate systems of the layers
        std::vector<SCoordinateSystemPreset> GetCoordinateSystemPresets() const;
        // proj4 string of an EPSG code, throws when the code is unknown
        static std::string CoordinateSystemFromEpsg(int nCode);
        // "Projected, meters" / "Geographic, degrees", empty proj4 - "None"; throws on a wrong string; pUnits - the units of it
        static std::string DescribeCoordinateSystem(const std::string& sProj4, CommonLib::Units* pUnits = nullptr);
        static const char* UnitsName(CommonLib::Units units);
        static const char* WebMercatorProj4();

        // ---- settings of a feature layer (layer properties dialog)

        static STableInfo GetTableInfo(GraphEngine::GeoDatabase::ITablePtr ptrTable, const std::string& sName);
        // the current settings; pbSymbologyExact - false: the symbology can't be shown exactly by the dialog
        // (it is kept when the user doesn't change it)
        static void GetLayerParams(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer, SLayerParams& params, bool* pbSymbologyExact = nullptr);
        // annotation and labels are set as in params, the symbology - only when params.ptrSymbology is set;
        // throws when a symbol can't be made (an image file), the layer isn't changed then
        static void ApplyLayerParams(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer, const SLayerParams& params);
        // scale dependence of all the symbols of the feature renderers (not labels):
        // CSymbolFactory::ScaleDependentNo / Yes / Mixed, No when the layer has no symbols
        static int  GetLayerScaleDependent(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer);
        static void SetLayerScaleDependent(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer, bool bScaleDependent);

        static SLayerDataInfo GetLayerDataInfo(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer);
        // the OID and the shape field of the layer (empty - the table default); the shape field is set to all the renderers
        // of the layer (features, annotation, labels); throws when a field isn't in the table or has a wrong type
        static void SetLayerDataFields(GraphEngine::Cartography::IFeatureLayerPtr ptrLayer, const std::string& sOIDField, const std::string& sShapeField);

        void Save(const std::string& sFilePathUtf8) const;
        void Load(const std::string& sFilePathUtf8);

        static GraphEngine::Display::ISymbolPtr CreateDefaultSymbol(CommonLib::eShapeType shapeType, int nColorIndex);
        static GraphEngine::Display::ISymbolPtr CreateSelectionSymbol();

    private:
        void Clear();
        void InitMap(GraphEngine::Cartography::IMapPtr ptrMap);
        void AddLayer(GraphEngine::Cartography::ILayerPtr ptrLayer, GraphEngine::Geometry::ISpatialReferencePtr ptrSpatRef);

    private:
        GraphEngine::Cartography::IMapPtr m_ptrMap;
        std::vector<GraphEngine::GeoDatabase::IWorkspacePtr> m_vecWorkspaces;
    };
}
