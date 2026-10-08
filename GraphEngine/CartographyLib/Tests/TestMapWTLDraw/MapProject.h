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

    struct STableInfo
    {
        std::string             sName;
        std::vector<SFieldInfo> vecFields;   // attribute (non geometry) fields
        CommonLib::eShapeType   shapeType = CommonLib::shape_type_null;
    };

    // layer settings from the Add layer dialogs
    struct SLayerParams
    {
        SAnnotationParams           annotation;
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
        // min / max of the numeric values of the field, false - no numeric values
        static bool GetValueRange(const SDataSource& source, const std::string& sField, double& dMin, double& dMax);
        // color of the default symbol of the layer with this index
        static GraphEngine::Display::Color GetLayerColor(int nIndex);
        // opens the raster file (TIFF / GeoTIFF, UTF-8 path) and adds it as a raster layer with the default renderer, returns the layer
        GraphEngine::Cartography::ILayerPtr AddRaster(const std::string& sFilePathUtf8);

        // adds the spatial tables of an SQLite database as layers (sTableName empty - all of them), returns the number of added layers
        // annotation and symbology are set only for the tables which have their fields
        int AddSQLiteDatabase(const std::string& sDatabasePath, const std::string& sTableName = std::string(), const SLayerParams& params = SLayerParams());
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
