#include "MapProject.h"
#include "../../Map.h"
#include "../../layers/FeatureLayer.h"
#include "../../layers/RasterLayer.h"
#include "../../renders/FeatureRenderer.h"
#include "../../selectors/SimpleSymbolSelector.h"
#include "../../../GeoDatabase/GeoDatabaseShape/ShapefileWorkspace.h"
#include "../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterWorkspace.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "../../../GeoDatabase/WorkspaceHolder.h"
#include "../../../GeoDatabase/DatasetLoader.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleLineSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleMarketSymbol.h"
#include "../../../CommonLib/xml/XMLDoc.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../CommonLib/Serialize/SerializeXML.h"

#include <cmath>
#include <filesystem>

using namespace GraphEngine;

namespace TestMapDraw
{
    namespace
    {
        void SplitShapefilePath(const std::string& sFilePath, std::string& sDir, std::string& sName)
        {
            sDir = "./";
            sName = sFilePath;
            size_t nPos = sFilePath.find_last_of("\\/");
            if(nPos != std::string::npos)
            {
                sDir = sFilePath.substr(0, nPos + 1);
                sName = sFilePath.substr(nPos + 1);
            }

            size_t nExt = sName.find_last_of('.');
            if(nExt != std::string::npos)
                sName = sName.substr(0, nExt);
        }

        Display::Color LayerColor(int nIndex)
        {
            static const Display::Color colors[] =
            {
                Display::Color(255, 204, 0, 255),
                Display::Color(102, 194, 165, 255),
                Display::Color(141, 160, 203, 255),
                Display::Color(231, 138, 195, 255),
                Display::Color(166, 216, 84, 255),
                Display::Color(229, 196, 148, 255)
            };
            const int nCount = (int)(sizeof(colors) / sizeof(colors[0]));
            return colors[(nIndex < 0 ? 0 : nIndex) % nCount];
        }
    }

    CMapProject::CMapProject()
    {
        New();
    }

    CMapProject::~CMapProject()
    {
        Clear();
    }

    Cartography::IMapPtr CMapProject::GetMap() const
    {
        return m_ptrMap;
    }

    void CMapProject::Clear()
    {
        for(size_t i = 0; i < m_vecWorkspaces.size(); ++i)
            GeoDatabase::CWorkspaceHolder::RemoveWorkspace(m_vecWorkspaces[i]->GetWorkspaceId());

        m_vecWorkspaces.clear();
    }

    void CMapProject::InitMap(Cartography::IMapPtr ptrMap)
    {
        ptrMap->GetSelection()->SetSymbol(CreateSelectionSymbol());
        m_ptrMap = ptrMap;
    }

    void CMapProject::New()
    {
        Clear();
        InitMap(std::make_shared<Cartography::CMap>());
    }

    Display::ISymbolPtr CMapProject::CreateSelectionSymbol()
    {
        return std::make_shared<Display::CSimpleLineSymbol>(Display::Color(0, 255, 255, 255), 2., Display::SimpleLineStyleSolid);
    }

    Display::ISymbolPtr CMapProject::CreateDefaultSymbol(CommonLib::eShapeType shapeType, int nColorIndex)
    {
        Display::Color color = LayerColor(nColorIndex);
        switch(CommonLib::CGeoShape::GetGeneralType(shapeType))
        {
            case CommonLib::shape_type_general_point:
            case CommonLib::shape_type_general_multipoint:
            {
                std::shared_ptr<Display::CSimpleMarketSymbol> ptrMarker = std::make_shared<Display::CSimpleMarketSymbol>();
                ptrMarker->SetStyle(Display::SimpleMarkerStyleCircle);
                ptrMarker->SetColor(color);
                ptrMarker->SetSize(2.);
                ptrMarker->SetOutline(true);
                ptrMarker->SetOutlineColor(Display::Color(64, 64, 64, 255));
                ptrMarker->SetOutlineSize(0.2);
                return ptrMarker;
            }
            case CommonLib::shape_type_general_polyline:
                return std::make_shared<Display::CSimpleLineSymbol>(color, 0.5, Display::SimpleLineStyleSolid);
            default:
            {
                std::shared_ptr<Display::CSimpleFillSymbol> ptrFill = std::make_shared<Display::CSimpleFillSymbol>();
                ptrFill->SetColor(color);
                ptrFill->SetOutlineSymbol(std::make_shared<Display::CSimpleLineSymbol>(Display::Color(64, 64, 64, 255), 0.2, Display::SimpleLineStyleSolid));
                return ptrFill;
            }
        }
    }

    Cartography::ILayerPtr CMapProject::AddShapefile(const std::string& sFilePathUtf8)
    {
        try
        {
            std::string sPath;
            std::string sName;
            SplitShapefilePath(sFilePathUtf8, sPath, sName);

            GeoDatabase::IWorkspacePtr ptrWorkspace = GeoDatabase::CShapfileWorkspace::Open(sName.c_str(), sPath.c_str(), CommonLib::CGuid::CreateNew());
            GeoDatabase::IDatabaseWorkspace* pDbWorkspace = dynamic_cast<GeoDatabase::IDatabaseWorkspace*>(ptrWorkspace.get());
            GeoDatabase::ITablePtr ptrTable = pDbWorkspace->GetTable(sName);

            GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
            m_vecWorkspaces.push_back(ptrWorkspace);

            AddTable(ptrTable, sName);
            return m_ptrMap->GetLayers()->GetLayer(m_ptrMap->GetLayers()->GetLayerCount() - 1);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to add shapefile {0}", sFilePathUtf8, exc);
            throw;
        }
    }

    void CMapProject::AddTable(GeoDatabase::ITablePtr ptrTable, const std::string& sLayerName)
    {
        int nLayerCount = m_ptrMap->GetLayers()->GetLayerCount();

        std::shared_ptr<Cartography::CFeatureRenderer> ptrRenderer = std::make_shared<Cartography::CFeatureRenderer>();
        ptrRenderer->SetSymbolSelector(std::make_shared<Cartography::CSimpleSymbolSelector>(CreateDefaultSymbol(ptrTable->GetGeometryType(), nLayerCount)));

        std::shared_ptr<Cartography::CFeatureLayer> ptrLayer = std::make_shared<Cartography::CFeatureLayer>();
        ptrLayer->SetName(sLayerName);
        ptrLayer->SetLayerTable(ptrTable);
        ptrLayer->AddRenderer(ptrRenderer);
        ptrLayer->SetVisible(true);
        ptrLayer->SetSelectable(true);

        AddLayer(ptrLayer, ptrTable->GetSpatialReference());
    }

    void CMapProject::AddLayer(Cartography::ILayerPtr ptrLayer, Geometry::ISpatialReferencePtr ptrSpatRef)
    {
        if(m_ptrMap->GetLayers()->GetLayerCount() == 0)
        {
            // the first layer defines the map coordinate system
            m_ptrMap->SetSpatialReference(ptrSpatRef);
            m_ptrMap->SetMapUnits(ptrSpatRef.get() ? ptrSpatRef->GetUnits() : CommonLib::UnitsUnknown);
        }

        m_ptrMap->GetLayers()->AddLayer(ptrLayer);
    }

    bool CMapProject::GetLayerExtent(int nLayerIndex, CommonLib::bbox& bb) const
    {
        Cartography::ILayersPtr ptrLayers = m_ptrMap->GetLayers();
        if(nLayerIndex < 0 || nLayerIndex >= ptrLayers->GetLayerCount())
            throw CommonLib::CExcBase("Layer index out of range: {0}", nLayerIndex);

        Geometry::IEnvelopePtr ptrExtent = ptrLayers->GetLayer(nLayerIndex)->GetExtent();
        if(!ptrExtent.get() || !(ptrExtent->GetBoundingBox().type & CommonLib::bbox_type_normal))
            return false;

        CommonLib::bbox box = ptrExtent->GetBoundingBox();
        Geometry::ISpatialReferencePtr ptrLayerSpatRef = ptrExtent->GetSpatialReference();
        Geometry::ISpatialReferencePtr ptrMapSpatRef = m_ptrMap->GetSpatialReference();
        if(ptrLayerSpatRef.get() && ptrMapSpatRef.get() && !ptrLayerSpatRef->IsEqual(ptrMapSpatRef))
        {
            if(!ptrLayerSpatRef->Project(ptrMapSpatRef, box))
                return false;
        }

        if(!std::isfinite(box.xMin) || !std::isfinite(box.xMax) || !std::isfinite(box.yMin) || !std::isfinite(box.yMax))
            return false;

        // a single point (or a line along an axis): show some area around it
        double dx = box.xMax - box.xMin;
        double dy = box.yMax - box.yMin;
        double size = (std::max)(dx, dy);
        if(size <= 0.)
            size = (std::max)((std::max)(std::fabs(box.xMin), std::fabs(box.yMin)) * 1e-4, 1e-6);
        if(dx < size * 0.01)
        {
            box.xMin -= size * 0.05;
            box.xMax += size * 0.05;
        }
        if(dy < size * 0.01)
        {
            box.yMin -= size * 0.05;
            box.yMax += size * 0.05;
        }

        box.type = CommonLib::bbox_type_normal;
        bb = box;
        return true;
    }

    Cartography::ILayerPtr CMapProject::AddRaster(const std::string& sFilePathUtf8)
    {
        try
        {
            // one raster workspace per folder, the dataset name is the file name
            std::filesystem::path path = std::filesystem::u8path(sFilePathUtf8);
            std::string sDir = path.parent_path().u8string();
            std::string sName = path.filename().u8string();

            GeoDatabase::IRasterWorkspacePtr ptrWorkspace;
            for(size_t i = 0; i < m_vecWorkspaces.size() && !ptrWorkspace.get(); ++i)
            {
                GeoDatabase::CRasterWorkspace* pRasterWks = dynamic_cast<GeoDatabase::CRasterWorkspace*>(m_vecWorkspaces[i].get());
                if(pRasterWks && pRasterWks->GetPath() == sDir)
                    ptrWorkspace = std::dynamic_pointer_cast<GeoDatabase::IRasterWorkspace>(m_vecWorkspaces[i]);
            }

            bool bNewWorkspace = !ptrWorkspace.get();
            if(bNewWorkspace)
                ptrWorkspace = std::dynamic_pointer_cast<GeoDatabase::IRasterWorkspace>(
                        GeoDatabase::CRasterWorkspace::Open(path.parent_path().filename().u8string().c_str(), sDir.c_str(), CommonLib::CGuid::CreateNew()));

            GeoDatabase::IRasterDatasetPtr ptrDataset = ptrWorkspace->OpenRasterDataset(sName);

            if(bNewWorkspace)
            {
                GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
                m_vecWorkspaces.push_back(ptrWorkspace);
            }

            std::shared_ptr<Cartography::CRasterLayer> ptrLayer = std::make_shared<Cartography::CRasterLayer>(ptrDataset);
            ptrLayer->SetName(path.stem().u8string());
            ptrLayer->SetVisible(true);

            AddLayer(ptrLayer, ptrDataset->GetSpatialReference());
            return ptrLayer;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to add raster {0}", sFilePathUtf8, exc);
            throw;
        }
    }

    int CMapProject::AddSQLiteDatabase(const std::string& sDatabasePath, const std::string& sTableName)
    {
        try
        {
            std::string sDir;
            std::string sName;
            SplitShapefilePath(sDatabasePath, sDir, sName);

            GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace = GeoDatabase::CSQLiteWorkspace::Open(sName.c_str(), sDatabasePath.c_str(), CommonLib::CGuid::CreateNew());
            GeoDatabase::CSQLiteWorkspace* pSQLiteWorkspace = dynamic_cast<GeoDatabase::CSQLiteWorkspace*>(ptrWorkspace.get());

            std::vector<std::string> vecTables = pSQLiteWorkspace->GetSpatialTableNames();
            if(!sTableName.empty())
            {
                if(std::find(vecTables.begin(), vecTables.end(), sTableName) == vecTables.end())
                    throw CommonLib::CExcBase("Spatial table {0} not found", sTableName);

                vecTables.assign(1, sTableName);
            }

            if(vecTables.empty())
                throw CommonLib::CExcBase("The database has no spatial tables");

            GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
            m_vecWorkspaces.push_back(ptrWorkspace);

            for(size_t i = 0; i < vecTables.size(); ++i)
                AddTable(ptrWorkspace->GetTable(vecTables[i]), vecTables[i]);

            return (int)vecTables.size();
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to add SQLite database {0}", sDatabasePath, exc);
            throw;
        }
    }

    std::string CMapProject::ConvertShapefileToSQLite(const std::string& sShapefilePath, const std::string& sDatabasePath,
                                                      GeoDatabase::CTableCopier::TProgress progress, int64_t* pnCopied)
    {
        try
        {
            std::string sDir;
            std::string sName;
            SplitShapefilePath(sShapefilePath, sDir, sName);

            GeoDatabase::IDatabaseWorkspacePtr ptrShapeWorkspace = std::dynamic_pointer_cast<GeoDatabase::IDatabaseWorkspace>(
                    GeoDatabase::CShapfileWorkspace::Open(sName.c_str(), sDir.c_str(), CommonLib::CGuid::CreateNew()));
            GeoDatabase::ITablePtr ptrShapeTable = ptrShapeWorkspace->GetTable(sName);

            std::string sDbDir;
            std::string sDbName;
            SplitShapefilePath(sDatabasePath, sDbDir, sDbName);
            GeoDatabase::IDatabaseWorkspacePtr ptrDbWorkspace = CommonLib::CFileUtils::IsFileExist(sDatabasePath) ?
                    GeoDatabase::CSQLiteWorkspace::Open(sDbName.c_str(), sDatabasePath.c_str(), CommonLib::CGuid::CreateNew()) :
                    GeoDatabase::CSQLiteWorkspace::Create(sDbName.c_str(), sDatabasePath.c_str(), CommonLib::CGuid::CreateNew());

            // unique table name in the database
            std::vector<std::string> vecTables = dynamic_cast<GeoDatabase::CSQLiteWorkspace*>(ptrDbWorkspace.get())->GetSpatialTableNames();
            std::string sBaseName = GeoDatabase::CTableCopier::MakeValidTableName(sName);
            std::string sTableName = sBaseName;
            for(int i = 1; std::find(vecTables.begin(), vecTables.end(), sTableName) != vecTables.end(); ++i)
                sTableName = sBaseName + "_" + std::to_string(i);

            int64_t nCopied = 0;
            GeoDatabase::CTableCopier::CopySpatialTable(ptrShapeTable, ptrDbWorkspace, sTableName,
                [&](int64_t nRows) { nCopied = nRows; return progress ? progress(nRows) : true; });

            if(pnCopied)
                *pnCopied = nCopied;
            return sTableName;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to convert {0} to SQLite", sShapefilePath, exc);
            throw;
        }
    }

    // <Project>
    //   <Workspaces> <Workspace .../> ... </Workspaces>
    //   <Map> ... </Map>
    // </Project>
    void CMapProject::Save(const std::string& sFilePathUtf8) const
    {
        try
        {
            CommonLib::xml::CXMLDoc xmlDoc;
            CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(xmlDoc.GetNodes());
            CommonLib::ISerializeObjPtr ptrProject = ptrRoot->CreateChildNode("Project");

            CommonLib::ISerializeObjPtr ptrWorkspaces = ptrProject->CreateChildNode("Workspaces");
            for(size_t i = 0; i < m_vecWorkspaces.size(); ++i)
                m_vecWorkspaces[i]->Save(ptrWorkspaces->CreateChildNode("Workspace"));

            m_ptrMap->Save(ptrProject);
            xmlDoc.Save(sFilePathUtf8);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to save project {0}", sFilePathUtf8, exc);
            throw;
        }
    }

    void CMapProject::Load(const std::string& sFilePathUtf8)
    {
        try
        {
            CommonLib::xml::CXMLDoc xmlDoc;
            xmlDoc.Open(sFilePathUtf8);
            CommonLib::ISerializeObjPtr ptrRoot = std::make_shared<CommonLib::CSerializeObjXML>(xmlDoc.GetNodes());
            if(!ptrRoot->IsChildExists("Project"))
                throw CommonLib::CExcBase("Not a map project");

            CommonLib::ISerializeObjPtr ptrProject = ptrRoot->GetChild("Project");

            Clear();
            if(ptrProject->IsChildExists("Workspaces"))
            {
                CommonLib::ISerializeObjPtr ptrWorkspaces = ptrProject->GetChild("Workspaces");
                for(uint32_t i = 0, sz = ptrWorkspaces->GetChildCnt(); i < sz; ++i)
                {
                    GeoDatabase::IWorkspacePtr ptrWorkspace = GeoDatabase::CDatasetLoader::LoadWorkspace(ptrWorkspaces->GetChild(i));
                    GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
                    m_vecWorkspaces.push_back(ptrWorkspace);
                }
            }

            std::shared_ptr<Cartography::CMap> ptrMap = std::make_shared<Cartography::CMap>();
            ptrMap->Load(ptrProject);
            InitMap(ptrMap);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to open project {0}", sFilePathUtf8, exc);
            throw;
        }
    }
}
