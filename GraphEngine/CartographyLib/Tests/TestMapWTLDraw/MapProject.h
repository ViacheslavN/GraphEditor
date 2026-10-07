#pragma once
// Platform independent part of the test application: shapefile -> layer, project (xml) save / load.

#include "../../Cartography.h"
#include "../../../GeoDatabase/TableCopier.h"

namespace TestMapDraw
{
    class CMapProject
    {
    public:
        CMapProject();
        ~CMapProject();

        GraphEngine::Cartography::IMapPtr GetMap() const;

        void New();
        // opens the shapefile (full path to .shp) and adds it as a feature layer, returns the layer
        GraphEngine::Cartography::ILayerPtr AddShapefile(const std::string& sFilePathUtf8);
        void AddTable(GraphEngine::GeoDatabase::ITablePtr ptrTable, const std::string& sLayerName);

        // adds the spatial tables of an SQLite database as layers (sTableName empty - all of them), returns the number of added layers
        int AddSQLiteDatabase(const std::string& sDatabasePath, const std::string& sTableName = std::string());

        // copies the shapefile into an SQLite database (created if it doesn't exist) as a new spatial table,
        // returns the table name; progress gets the number of copied features and can cancel
        static std::string ConvertShapefileToSQLite(const std::string& sShapefilePath, const std::string& sDatabasePath,
                                                    GraphEngine::GeoDatabase::CTableCopier::TProgress progress = GraphEngine::GeoDatabase::CTableCopier::TProgress(),
                                                    int64_t* pnCopied = nullptr);

        void Save(const std::string& sFilePathUtf8) const;
        void Load(const std::string& sFilePathUtf8);

        static GraphEngine::Display::ISymbolPtr CreateDefaultSymbol(CommonLib::eShapeType shapeType, int nColorIndex);
        static GraphEngine::Display::ISymbolPtr CreateSelectionSymbol();

    private:
        void Clear();
        void InitMap(GraphEngine::Cartography::IMapPtr ptrMap);

    private:
        GraphEngine::Cartography::IMapPtr m_ptrMap;
        std::vector<GraphEngine::GeoDatabase::IWorkspacePtr> m_vecWorkspaces;
    };
}
