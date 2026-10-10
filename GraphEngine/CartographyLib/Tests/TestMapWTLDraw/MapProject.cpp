#include "MapProject.h"
#include "../../Map.h"
#include "../../layers/FeatureLayer.h"
#include "../../layers/RasterLayer.h"
#include "../../layers/GroupLayer.h"
#include "../../renders/FeatureRenderer.h"
#include "../../renders/AnnotationRenderer.h"
#include "../../renders/LabelRenderer.h"
#include "../../selectors/SimpleSymbolSelector.h"
#include "../../../GeoDatabase/GeoDatabaseShape/ShapefileWorkspace.h"
#include "../../../GeoDatabase/GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../../GeoDatabase/GeoDatabaseRaster/RasterWorkspace.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "../../../GeoDatabase/WorkspaceHolder.h"
#include "../../../GeoDatabase/DatasetLoader.h"
#include "../../../GeoDatabase/QueryFilter.h"
#include "../../selectors/SymbolSelectorUtils.h"
#include "../../../DisplayLib/Symbols/SimpleFillSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleLineSymbol.h"
#include "../../../DisplayLib/Symbols/SimpleMarketSymbol.h"
#include "../../../DisplayLib/Symbols/TextSymbol.h"
#include "../../../CommonLib/xml/XMLDoc.h"
#include "../../../CommonLib/xml/XMLNode.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../GisGeometry/SpatialReferenceProj4/SpatialReferenceProj4.h"

#include <cmath>
#include <filesystem>
#include <functional>
#include <map>

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

        std::vector<SFieldInfo> GetTableFields(GeoDatabase::ITablePtr ptrTable)
        {
            std::vector<SFieldInfo> vecFields;
            GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
            for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
            {
                GeoDatabase::IFieldPtr ptrField = ptrFields->GetField(i);
                if(ptrField->GetType() == GeoDatabase::dtGeometry)
                    continue;

                GeoDatabase::eDataTypes type = ptrField->GetType();
                SFieldInfo info;
                info.sName = ptrField->GetName();
                info.bText = type == GeoDatabase::dtString;
                info.bNumeric = (type >= GeoDatabase::dtInteger8 && type <= GeoDatabase::dtDouble);
                vecFields.push_back(info);
            }
            return vecFields;
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

    Display::Color CMapProject::GetLayerColor(int nIndex)
    {
        return LayerColor(nIndex);
    }

    STableInfo CMapProject::GetShapefileInfo(const std::string& sFilePath)
    {
        try
        {
            std::string sPath;
            std::string sName;
            SplitShapefilePath(sFilePath, sPath, sName);

            // temporary workspace, not registered in CWorkspaceHolder
            GeoDatabase::IWorkspacePtr ptrWorkspace = GeoDatabase::CShapfileWorkspace::Open(sName.c_str(), sPath.c_str(), CommonLib::CGuid::CreateNew());
            GeoDatabase::IDatabaseWorkspace* pDbWorkspace = dynamic_cast<GeoDatabase::IDatabaseWorkspace*>(ptrWorkspace.get());
            if(!pDbWorkspace)
                throw CommonLib::CExcBase("not a database workspace");

            GeoDatabase::ITablePtr ptrTable = pDbWorkspace->GetTable(sName);
            STableInfo info;
            info.sName = sName;
            info.vecFields = GetTableFields(ptrTable);
            info.shapeType = ptrTable->GetGeometryType();
            return info;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read fields of shapefile {0}", sFilePath, exc);
            throw;
        }
    }

    namespace
    {
        // the table of the source, the workspace must live while the table is used
        GeoDatabase::ITablePtr OpenSourceTable(const SDataSource& source, GeoDatabase::IWorkspacePtr& ptrWorkspace)
        {
            std::string sDir;
            std::string sName;
            SplitShapefilePath(source.sPath, sDir, sName);

            if(source.bSQLite)
            {
                GeoDatabase::IDatabaseWorkspacePtr ptrDb = GeoDatabase::CSQLiteWorkspace::Open(sName.c_str(), source.sPath.c_str(), CommonLib::CGuid::CreateNew());
                ptrWorkspace = ptrDb;
                return ptrDb->GetTable(source.sTable);
            }

            ptrWorkspace = GeoDatabase::CShapfileWorkspace::Open(sName.c_str(), sDir.c_str(), CommonLib::CGuid::CreateNew());
            GeoDatabase::IDatabaseWorkspace* pDb = dynamic_cast<GeoDatabase::IDatabaseWorkspace*>(ptrWorkspace.get());
            if(!pDb)
                throw CommonLib::CExcBase("not a database workspace");
            return pDb->GetTable(sName);
        }

        // reads the one field of all the rows
        template<class F>
        void ForEachTableValue(GeoDatabase::ITablePtr ptrTable, const std::string& sField, F func)
        {
            if(!ptrTable.get())
                throw CommonLib::CExcBase("No table");
            if(!ptrTable->GetFields()->FieldExists(sField))
                throw CommonLib::CExcBase("Field {0} not found", sField);

            GeoDatabase::IQueryFilterPtr ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
            ptrFilter->GetFieldSet()->Add(sField);
            GeoDatabase::ISelectCursorPtr ptrCursor = ptrTable->Search(ptrFilter);
            if(!ptrCursor.get())
                return;

            GeoDatabase::IRowPtr ptrRow = ptrCursor->CreateRow();
            int32_t nColumn = -1;
            while(ptrCursor->Next())
            {
                ptrCursor->FillRow(ptrRow);
                nColumn = Cartography::CSymbolSelectorUtils::FindColumn(ptrRow, sField, nColumn);
                if(!func(ptrRow, nColumn))
                    break;
            }
        }

        // text symbol of the annotation / labels: the size and the halo are in mm
        std::shared_ptr<Display::CTextSymbol> CreateTextSymbol(double dSize, const Display::Color& color, double dHaloSize)
        {
            std::shared_ptr<Display::CTextSymbol> ptrTextSymbol = std::make_shared<Display::CTextSymbol>();
            ptrTextSymbol->SetSize(dSize);
            ptrTextSymbol->SetColor(color);
            if(dHaloSize > 0.)
            {
                ptrTextSymbol->GetFont()->SetHaloSize(dHaloSize);
                ptrTextSymbol->GetFont()->SetBgColor(Display::Color(255, 255, 255, 255));
            }
            return ptrTextSymbol;
        }

        // annotation: the field value is drawn with a simple text symbol, an empty field - no annotation;
        // the symbol of the current annotation is kept
        void SetLayerAnnotation(Cartography::IFeatureLayerPtr ptrLayer, const SAnnotationParams& anno)
        {
            if(anno.sField.empty())
            {
                ptrLayer->SetAnnotationRenderer(Cartography::IAnnotationRenderPtr());
                ptrLayer->SetAnnoFieldName(std::string());
                return;
            }

            Cartography::IAnnotationRenderPtr ptrAnnoRenderer = ptrLayer->GetAnnotationRenderer();
            if(!ptrAnnoRenderer.get())
                ptrAnnoRenderer = std::make_shared<Cartography::CAnnotationRenderer>(std::make_shared<Cartography::CSimpleSymbolSelector>(
                        CreateTextSymbol(3., Display::Color(0, 0, 0, 255), 0.)));
            ptrAnnoRenderer->SetMinimumScale(anno.dMinimumScale > 0. ? anno.dMinimumScale : 0.); // layer skips it when scale > minimum scale
            ptrLayer->SetAnnotationRenderer(ptrAnnoRenderer);
            ptrLayer->SetAnnoFieldName(anno.sField);
        }

        // labels: the field value with a text symbol (with a halo), the label drawer of the map places them without overlapping
        void SetLayerLabels(Cartography::IFeatureLayerPtr ptrLayer, const SLabelParams& labels, int nClassIndex)
        {
            if(labels.sField.empty())
            {
                ptrLayer->SetLabelRenderer(Cartography::ILabelRendererPtr());
                ptrLayer->SetLabelFieldName(std::string());
                return;
            }

            std::shared_ptr<Cartography::CLabelRenderer> ptrLabelRenderer = std::make_shared<Cartography::CLabelRenderer>(
                    std::make_shared<Cartography::CSimpleSymbolSelector>(CreateTextSymbol(labels.dFontSize, labels.color, labels.dHaloSize)));
            ptrLabelRenderer->SetMinimumScale(labels.dMinimumScale > 0. ? labels.dMinimumScale : 0.); // layer skips it when scale > minimum scale
            ptrLabelRenderer->SetClassIndex(nClassIndex);   // the same priority: labels of the upper layers are placed first
            ptrLayer->SetLabelRenderer(ptrLabelRenderer);
            ptrLayer->SetLabelFieldName(labels.sField);
            ptrLayer->SetLabelingOptions(labels.options);
        }

        bool HasDataLayersIn(Cartography::ILayersPtr ptrLayers)
        {
            for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
            {
                Cartography::ILayerPtr ptrLayer = ptrLayers->GetLayer(i);
                Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer);
                if(ptrGroup.get() ? HasDataLayersIn(ptrGroup->GetChildren()) : true)
                    return true;
            }
            return false;
        }

        Cartography::ILayersPtr FindParentListIn(Cartography::ILayersPtr ptrLayers, Cartography::ILayerPtr ptrLayer)
        {
            for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
            {
                Cartography::ILayerPtr ptrItem = ptrLayers->GetLayer(i);
                if(ptrItem == ptrLayer)
                    return ptrLayers;

                Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrItem);
                if(ptrGroup.get())
                {
                    Cartography::ILayersPtr ptrFound = FindParentListIn(ptrGroup->GetChildren(), ptrLayer);
                    if(ptrFound.get())
                        return ptrFound;
                }
            }
            return Cartography::ILayersPtr();
        }

        int IndexOf(Cartography::ILayersPtr ptrLayers, Cartography::ILayerPtr ptrLayer)
        {
            for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
                if(ptrLayers->GetLayer(i) == ptrLayer)
                    return i;
            return -1;
        }
    }

    std::vector<CommonLib::CVariant> CMapProject::GetUniqueValues(GeoDatabase::ITablePtr ptrTable, const std::string& sField, size_t nMaxCount, bool* pbTruncated)
    {
        try
        {
            if(pbTruncated)
                *pbTruncated = false;

            std::map<Cartography::SValueKey, CommonLib::CVariant> mapValues;
            ForEachTableValue(ptrTable, sField, [&](GeoDatabase::IRowPtr ptrRow, int32_t nColumn) -> bool
            {
                Cartography::SValueKey key = Cartography::CSymbolSelectorUtils::MakeKey(ptrRow, nColumn);
                if(key.kind == Cartography::SValueKey::KindOther || mapValues.count(key))
                    return true;

                if(mapValues.size() >= nMaxCount)
                {
                    if(pbTruncated)
                        *pbTruncated = true;
                    return false;
                }

                CommonLib::CVariantPtr ptrValue = ptrRow->ColumnIsNull(nColumn) ? CommonLib::CVariantPtr() : ptrRow->GetValue(nColumn);
                mapValues[key] = ptrValue.get() ? *ptrValue : CommonLib::CVariant();
                return true;
            });

            std::vector<CommonLib::CVariant> vecValues;
            for(std::map<Cartography::SValueKey, CommonLib::CVariant>::const_iterator it = mapValues.begin(); it != mapValues.end(); ++it)
                vecValues.push_back(it->second);
            return vecValues;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read values of field {0}", sField, exc);
            throw;
        }
    }

    bool CMapProject::GetValueRange(GeoDatabase::ITablePtr ptrTable, const std::string& sField, double& dMin, double& dMax)
    {
        try
        {
            bool bFound = false;
            ForEachTableValue(ptrTable, sField, [&](GeoDatabase::IRowPtr ptrRow, int32_t nColumn) -> bool
            {
                double dValue = 0.;
                if(!Cartography::CSymbolSelectorUtils::ToDouble(ptrRow, nColumn, dValue))
                    return true;

                if(!bFound || dValue < dMin) dMin = dValue;
                if(!bFound || dValue > dMax) dMax = dValue;
                bFound = true;
                return true;
            });
            return bFound;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read values of field {0}", sField, exc);
            throw;
        }
    }

    std::vector<CommonLib::CVariant> CMapProject::GetUniqueValues(const SDataSource& source, const std::string& sField, size_t nMaxCount, bool* pbTruncated)
    {
        GeoDatabase::IWorkspacePtr ptrWorkspace;   // lives while the table is read
        GeoDatabase::ITablePtr ptrTable;
        try
        {
            ptrTable = OpenSourceTable(source, ptrWorkspace);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read values of field {0}", sField, exc);
            throw;
        }
        return GetUniqueValues(ptrTable, sField, nMaxCount, pbTruncated);
    }

    bool CMapProject::GetValueRange(const SDataSource& source, const std::string& sField, double& dMin, double& dMax)
    {
        GeoDatabase::IWorkspacePtr ptrWorkspace;   // lives while the table is read
        GeoDatabase::ITablePtr ptrTable;
        try
        {
            ptrTable = OpenSourceTable(source, ptrWorkspace);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read values of field {0}", sField, exc);
            throw;
        }
        return GetValueRange(ptrTable, sField, dMin, dMax);
    }

    Cartography::ILayerPtr CMapProject::AddShapefile(const std::string& sFilePathUtf8, const SLayerParams& params)
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

            AddTable(ptrTable, sName, params);
            return m_ptrMap->GetLayers()->GetLayer(m_ptrMap->GetLayers()->GetLayerCount() - 1);
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to add shapefile {0}", sFilePathUtf8, exc);
            throw;
        }
    }

    void CMapProject::AddTable(GeoDatabase::ITablePtr ptrTable, const std::string& sLayerName, const SLayerParams& params)
    {
        int nLayerCount = m_ptrMap->GetLayers()->GetLayerCount();
        const SAnnotationParams& anno = params.annotation;

        // symbology from the dialog (made first: a wrong image file fails before the layer is added)
        Cartography::ISymbolSelectorPtr ptrSelector;
        if(params.ptrSymbology.get())
            ptrSelector = CSymbologyBuilder::CreateSelector(*params.ptrSymbology);
        else
            ptrSelector = std::make_shared<Cartography::CSimpleSymbolSelector>(CreateDefaultSymbol(ptrTable->GetGeometryType(), nLayerCount));

        std::shared_ptr<Cartography::CFeatureRenderer> ptrRenderer = std::make_shared<Cartography::CFeatureRenderer>();
        ptrRenderer->SetSymbolSelector(ptrSelector);

        std::shared_ptr<Cartography::CFeatureLayer> ptrLayer = std::make_shared<Cartography::CFeatureLayer>();
        ptrLayer->SetName(sLayerName);
        ptrLayer->SetLayerTable(ptrTable);
        ptrLayer->AddRenderer(ptrRenderer);
        ptrLayer->SetVisible(true);
        ptrLayer->SetSelectable(true);

        SetLayerLabels(ptrLayer, params.labels, nLayerCount);
        SetLayerAnnotation(ptrLayer, anno);

        AddLayer(ptrLayer, ptrTable->GetSpatialReference());
    }

    void CMapProject::AddLayer(Cartography::ILayerPtr ptrLayer, Geometry::ISpatialReferencePtr ptrSpatRef)
    {
        if(!HasDataLayers())
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

        return GetLayerExtent(ptrLayers->GetLayer(nLayerIndex), bb);
    }

    bool CMapProject::GetLayerExtent(Cartography::ILayerPtr ptrLayer, CommonLib::bbox& bb) const
    {
        if(!ptrLayer.get())
            return false;

        Geometry::IEnvelopePtr ptrExtent = ptrLayer->GetExtent();
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

    bool CMapProject::HasDataLayers() const
    {
        return HasDataLayersIn(m_ptrMap->GetLayers());
    }

    Cartography::IGroupLayerPtr CMapProject::AddGroupLayer(const std::string& sName, Cartography::IGroupLayerPtr ptrParent)
    {
        Cartography::IGroupLayerPtr ptrGroup = std::make_shared<Cartography::CGroupLayer>(sName);
        if(ptrParent.get())
            ptrParent->GetChildren()->AddLayer(ptrGroup);
        else
            m_ptrMap->GetLayers()->AddLayer(ptrGroup);
        return ptrGroup;
    }

    Cartography::ILayersPtr CMapProject::FindParentList(Cartography::ILayerPtr ptrLayer) const
    {
        if(!ptrLayer.get())
            return Cartography::ILayersPtr();
        return FindParentListIn(m_ptrMap->GetLayers(), ptrLayer);
    }

    void CMapProject::RemoveLayer(Cartography::ILayerPtr ptrLayer)
    {
        Cartography::ILayersPtr ptrList = FindParentList(ptrLayer);
        if(!ptrList.get())
            throw CommonLib::CExcBase("The layer {0} isn't in the map", ptrLayer.get() ? ptrLayer->GetName() : std::string());

        m_ptrMap->GetSelection()->ClearForLayer(ptrLayer->GetLayerId());
        ptrList->RemoveLayer(ptrLayer);
    }

    bool CMapProject::IsInGroup(Cartography::ILayerPtr ptrLayer, Cartography::ILayerPtr ptrGroup)
    {
        if(!ptrLayer.get() || !ptrGroup.get())
            return false;
        if(ptrLayer == ptrGroup)
            return true;

        Cartography::IGroupLayerPtr ptrGroupLayer = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrGroup);
        return ptrGroupLayer.get() && FindParentListIn(ptrGroupLayer->GetChildren(), ptrLayer).get() != nullptr;
    }

    bool CMapProject::MoveLayer(Cartography::ILayerPtr ptrLayer, Cartography::ILayersPtr ptrTargetList, Cartography::ILayerPtr ptrNeighbour, bool bAboveNeighbour)
    {
        Cartography::ILayersPtr ptrSourceList = FindParentList(ptrLayer);
        if(!ptrSourceList.get() || !ptrTargetList.get() || ptrNeighbour == ptrLayer)
            return false;
        if(ptrNeighbour.get() && IndexOf(ptrTargetList, ptrNeighbour) < 0)
            return false;

        // a group can't go into itself or into its child groups
        Cartography::IGroupLayerPtr ptrMovedGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer);
        if(ptrMovedGroup.get())
        {
            std::function<bool(Cartography::ILayersPtr)> containsList = [&](Cartography::ILayersPtr ptrLayers) -> bool
            {
                if(ptrLayers == ptrTargetList)
                    return true;
                for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
                {
                    Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayers->GetLayer(i));
                    if(ptrGroup.get() && containsList(ptrGroup->GetChildren()))
                        return true;
                }
                return false;
            };
            if(containsList(ptrMovedGroup->GetChildren()))
                return false;
        }

        // the index of the neighbour is taken after the layer has left its list (the same list too)
        ptrSourceList->RemoveLayer(ptrLayer);
        int nIndex = -1;   // the top (the end of the list)
        if(ptrNeighbour.get())
        {
            int nNeighbour = IndexOf(ptrTargetList, ptrNeighbour);
            nIndex = bAboveNeighbour ? nNeighbour + 1 : nNeighbour;
        }
        ptrTargetList->InsertLayer(ptrLayer, nIndex);
        return true;
    }

    STableInfo CMapProject::GetTableInfo(GeoDatabase::ITablePtr ptrTable, const std::string& sName)
    {
        STableInfo info;
        info.sName = sName;
        if(!ptrTable.get())
            return info;

        info.vecFields = GetTableFields(ptrTable);
        info.shapeType = ptrTable->GetGeometryType();
        return info;
    }

    void CMapProject::GetLayerParams(Cartography::IFeatureLayerPtr ptrLayer, SLayerParams& params, bool* pbSymbologyExact)
    {
        params = SLayerParams();
        if(pbSymbologyExact)
            *pbSymbologyExact = false;
        if(!ptrLayer.get())
            return;

        // annotation
        Cartography::IAnnotationRenderPtr ptrAnno = ptrLayer->GetAnnotationRenderer();
        if(ptrLayer->HasAnnoField() && ptrAnno.get())
        {
            params.annotation.sField = ptrLayer->GetAnnoFieldName();
            params.annotation.dMinimumScale = ptrAnno->GetMinimumScale();
        }

        // labels: the text symbol of the simple selector of the label renderer
        Cartography::ILabelRendererPtr ptrLabels = ptrLayer->GetLabelRenderer();
        if(ptrLayer->HasLabelField() && ptrLabels.get())
        {
            params.labels.sField = ptrLayer->GetLabelFieldName();
            params.labels.dMinimumScale = ptrLabels->GetMinimumScale();
            params.labels.options = ptrLayer->GetLabelingOptions();

            Cartography::ISimpleSymbolSelectorPtr ptrSelector = std::dynamic_pointer_cast<Cartography::ISimpleSymbolSelector>(ptrLabels->GetSymbolSelector());
            Display::ITextSymbolPtr ptrText = ptrSelector.get() ? std::dynamic_pointer_cast<Display::ITextSymbol>(ptrSelector->GetSymbol()) : Display::ITextSymbolPtr();
            if(ptrText.get())
            {
                params.labels.dFontSize = ptrText->GetSize();
                params.labels.color = ptrText->GetColor();
                params.labels.dHaloSize = ptrText->GetFont().get() ? ptrText->GetFont()->GetHaloSize() : 0.;
            }
        }

        // symbology: the selector of the first feature renderer
        GeoDatabase::ITablePtr ptrTable = ptrLayer->GetLayerTable();
        eGeometryKind geometry = GeometryKindOf(ptrTable.get() ? ptrTable->GetGeometryType() : CommonLib::shape_type_polygon);
        Cartography::ISymbolSelectorPtr ptrSelector = ptrLayer->GetRendererCount() > 0 && ptrLayer->GetRenderer(0).get() ?
                                                      ptrLayer->GetRenderer(0)->GetSymbolSelector() : Cartography::ISymbolSelectorPtr();
        std::shared_ptr<SSymbology> ptrSymbology = std::make_shared<SSymbology>();
        bool bExact = CSymbologyBuilder::FromSelector(ptrSelector, geometry, *ptrSymbology);
        params.ptrSymbology = ptrSymbology;
        if(pbSymbologyExact)
            *pbSymbologyExact = bExact;
    }

    void CMapProject::ApplyLayerParams(Cartography::IFeatureLayerPtr ptrLayer, const SLayerParams& params)
    {
        if(!ptrLayer.get())
            return;

        // the selector is made first: a wrong image file throws before the layer is changed
        Cartography::ISymbolSelectorPtr ptrSelector;
        if(params.ptrSymbology.get())
            ptrSelector = CSymbologyBuilder::CreateSelector(*params.ptrSymbology);

        if(ptrSelector.get())
        {
            Cartography::IFeatureRendererPtr ptrRenderer = ptrLayer->GetRendererCount() > 0 ? ptrLayer->GetRenderer(0) : Cartography::IFeatureRendererPtr();
            if(ptrRenderer.get())
                ptrRenderer->SetSymbolSelector(ptrSelector);
            else
            {
                std::shared_ptr<Cartography::CFeatureRenderer> ptrNewRenderer = std::make_shared<Cartography::CFeatureRenderer>();
                ptrNewRenderer->SetSymbolSelector(ptrSelector);
                ptrLayer->AddRenderer(ptrNewRenderer);
            }
        }

        // the labels keep their place in the order of the label classes
        Cartography::ILabelRendererPtr ptrOldLabels = ptrLayer->GetLabelRenderer();
        SetLayerLabels(ptrLayer, params.labels, ptrOldLabels.get() ? ptrOldLabels->GetClassIndex() : 0);
        SetLayerAnnotation(ptrLayer, params.annotation);
    }

    namespace
    {
        const char* DataTypeName(GeoDatabase::eDataTypes type)
        {
            switch(type)
            {
                case GeoDatabase::dtInteger8:   return "Integer 8";
                case GeoDatabase::dtInteger16:  return "Integer 16";
                case GeoDatabase::dtInteger32:  return "Integer 32";
                case GeoDatabase::dtInteger64:  return "Integer 64";
                case GeoDatabase::dtUInteger8:  return "Unsigned 8";
                case GeoDatabase::dtUInteger16: return "Unsigned 16";
                case GeoDatabase::dtUInteger32: return "Unsigned 32";
                case GeoDatabase::dtUInteger64: return "Unsigned 64";
                case GeoDatabase::dtFloat:      return "Float";
                case GeoDatabase::dtDouble:     return "Double";
                case GeoDatabase::dtString:     return "Text";
                case GeoDatabase::dtBlob:       return "Blob";
                case GeoDatabase::dtDate:       return "Date";
                case GeoDatabase::dtGuid:       return "Guid";
                case GeoDatabase::dtGeometry:   return "Geometry";
                default:                        return "Unknown";
            }
        }

        bool IsIntegerType(GeoDatabase::eDataTypes type)
        {
            return type >= GeoDatabase::dtInteger8 && type <= GeoDatabase::dtUInteger64;
        }

        const char* GeometryTypeName(CommonLib::eShapeType shapeType)
        {
            switch(CommonLib::CGeoShape::GetGeneralType(shapeType))
            {
                case CommonLib::shape_type_general_point:      return "Point";
                case CommonLib::shape_type_general_multipoint: return "Multipoint";
                case CommonLib::shape_type_general_polyline:   return "Polyline";
                case CommonLib::shape_type_general_polygon:    return "Polygon";
                default:                                       return "Unknown";
            }
        }

        // name and path of the workspace: from its saved properties (the workspaces have no common getters for them)
        std::string DescribeWorkspace(GeoDatabase::IWorkspacePtr ptrWorkspace)
        {
            if(!ptrWorkspace.get())
                return std::string();

            try
            {
                CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "Workspace");
                CommonLib::ISerializeObjPtr ptrObj = std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
                ptrWorkspace->Save(ptrObj);

                std::string sKind;
                switch(ptrWorkspace->GetWorkspaceType())
                {
                    case GeoDatabase::wtShapeFile: sKind = "Shape files"; break;
                    case GeoDatabase::wtSqlLite:   sKind = "SQLite"; break;
                    case GeoDatabase::wtRaster:    sKind = "Raster"; break;
                    default:                       sKind = "Workspace"; break;
                }

                std::string sPath = ptrObj->GetPropertyString("DatabasePath", ptrObj->GetPropertyString("Path", std::string()));
                return sKind + " " + ptrObj->GetPropertyString("Name", std::string()) + (sPath.empty() ? std::string() : ": " + sPath);
            }
            catch (std::exception&)
            {
                return std::string();
            }
        }

        // the shape field the renderers draw (they resolve an empty one to the table default when drawing)
        std::string CurrentShapeField(Cartography::IFeatureLayerPtr ptrLayer)
        {
            if(ptrLayer->GetRendererCount() > 0 && ptrLayer->GetRenderer(0).get() && !ptrLayer->GetRenderer(0)->GetShapeField().empty())
                return ptrLayer->GetRenderer(0)->GetShapeField();
            return ptrLayer->GetShapeField();
        }
    }

    // ---------------- map properties

    const char* CMapProject::WebMercatorProj4()
    {
        return "+proj=merc +a=6378137 +b=6378137 +lat_ts=0 +lon_0=0 +x_0=0 +y_0=0 +k=1 +units=m +no_defs";
    }

    const char* CMapProject::UnitsName(CommonLib::Units units)
    {
        switch(units)
        {
            case CommonLib::UnitsInches:         return "Inches";
            case CommonLib::UnitsPoints:         return "Points";
            case CommonLib::UnitsFeet:           return "Feet";
            case CommonLib::UnitsYards:          return "Yards";
            case CommonLib::UnitsMiles:          return "Miles";
            case CommonLib::UnitsNauticalMiles:  return "Nautical miles";
            case CommonLib::UnitsMillimeters:    return "Millimeters";
            case CommonLib::UnitsCentimeters:    return "Centimeters";
            case CommonLib::UnitsMeters:         return "Meters";
            case CommonLib::UnitsKilometers:     return "Kilometers";
            case CommonLib::UnitsDecimalDegrees: return "Decimal degrees";
            case CommonLib::UnitsDecimeters:     return "Decimeters";
            default:                             return "Unknown";
        }
    }

    std::string CMapProject::CoordinateSystemFromEpsg(int nCode)
    {
        // Web Mercator isn't in the code list of the projection library
        if(nCode == 3857 || nCode == 900913 || nCode == 3785)
            return WebMercatorProj4();

        Geometry::CSpatialReferenceProj4 spatRef(nCode);
        if(!spatRef.IsValid() || spatRef.GetProjectionString().empty())
            throw CommonLib::CExcBase("Unknown EPSG code {0}", nCode);
        return spatRef.GetProjectionString();
    }

    std::string CMapProject::DescribeCoordinateSystem(const std::string& sProj4, CommonLib::Units* pUnits)
    {
        if(pUnits)
            *pUnits = CommonLib::UnitsUnknown;
        if(sProj4.empty())
            return "None: the coordinates of the layers are used as they are";

        Geometry::CSpatialReferenceProj4 spatRef(sProj4);
        if(!spatRef.IsValid())
            throw CommonLib::CExcBase("Wrong coordinate system: {0}", sProj4);

        CommonLib::Units units = spatRef.GetUnits();
        if(pUnits)
            *pUnits = units;
        std::string sKind = units == CommonLib::UnitsDecimalDegrees ? "Geographic" : "Projected";
        return sKind + ", " + UnitsName(units);
    }

    SMapParams CMapProject::GetMapParams() const
    {
        SMapParams params;
        params.sName = m_ptrMap->GetName();
        Geometry::ISpatialReferencePtr ptrSpatRef = m_ptrMap->GetSpatialReference();
        if(ptrSpatRef.get())
            params.sSpatialReference = ptrSpatRef->GetProjectionString();
        params.units = m_ptrMap->GetMapUnits();
        params.bReferenceScale = m_ptrMap->GetHasReferenceScale();
        params.dReferenceScale = m_ptrMap->GetReferenceScale();

        Display::IFillSymbolPtr ptrBackground = m_ptrMap->GetBackgroundSymbol();
        params.background = ptrBackground.get() ? ptrBackground->GetColor() : Display::Color(255, 255, 255, Display::Color::Transparent);
        return params;
    }

    void CMapProject::ApplyMapParams(const SMapParams& params)
    {
        // checked first: a wrong system doesn't change the map
        Geometry::ISpatialReferencePtr ptrSpatRef;
        if(!params.sSpatialReference.empty())
        {
            ptrSpatRef = std::make_shared<Geometry::CSpatialReferenceProj4>(params.sSpatialReference);
            if(!ptrSpatRef->IsValid())
                throw CommonLib::CExcBase("Wrong coordinate system: {0}", params.sSpatialReference);
        }
        if(params.bReferenceScale && params.dReferenceScale <= 0.)
            throw CommonLib::CExcBase("Wrong reference scale");

        m_ptrMap->SetName(params.sName);

        Geometry::ISpatialReferencePtr ptrOld = m_ptrMap->GetSpatialReference();
        bool bChanged = ptrSpatRef.get() ? !(ptrOld.get() && ptrOld->IsEqual(ptrSpatRef)) : ptrOld.get() != nullptr;
        if(bChanged)
            m_ptrMap->SetSpatialReference(ptrSpatRef);   // the full extent is calculated again
        m_ptrMap->SetMapUnits(params.units);

        m_ptrMap->SetHasReferenceScale(params.bReferenceScale);
        m_ptrMap->SetReferenceScale(params.bReferenceScale ? params.dReferenceScale : 0.);

        if(params.background.GetA() == Display::Color::Transparent)
            m_ptrMap->SetBackgroundSymbol(Display::IFillSymbolPtr());
        else
        {
            std::shared_ptr<Display::CSimpleFillSymbol> ptrBackground = std::make_shared<Display::CSimpleFillSymbol>();
            ptrBackground->SetColor(params.background);
            m_ptrMap->SetBackgroundSymbol(ptrBackground);
        }
    }

    namespace
    {
        void CollectLayerSystems(Cartography::ILayersPtr ptrLayers, std::vector<SCoordinateSystemPreset>& vecPresets)
        {
            for(int i = 0, sz = ptrLayers->GetLayerCount(); i < sz; ++i)
            {
                Cartography::ILayerPtr ptrLayer = ptrLayers->GetLayer(i);
                if(Cartography::IGroupLayerPtr ptrGroup = std::dynamic_pointer_cast<Cartography::IGroupLayer>(ptrLayer))
                {
                    CollectLayerSystems(ptrGroup->GetChildren(), vecPresets);
                    continue;
                }

                Geometry::ISpatialReferencePtr ptrSpatRef;
                if(Cartography::IFeatureLayerPtr ptrFeature = std::dynamic_pointer_cast<Cartography::IFeatureLayer>(ptrLayer))
                    ptrSpatRef = ptrFeature->GetLayerTable().get() ? ptrFeature->GetLayerTable()->GetSpatialReference() : Geometry::ISpatialReferencePtr();
                else if(Cartography::IRasterLayerPtr ptrRaster = std::dynamic_pointer_cast<Cartography::IRasterLayer>(ptrLayer))
                    ptrSpatRef = ptrRaster->GetRasterDataset().get() ? ptrRaster->GetRasterDataset()->GetSpatialReference() : Geometry::ISpatialReferencePtr();
                if(!ptrSpatRef.get() || ptrSpatRef->GetProjectionString().empty())
                    continue;

                bool bKnown = false;
                for(size_t p = 0; p < vecPresets.size() && !bKnown; ++p)
                    bKnown = vecPresets[p].sProj4 == ptrSpatRef->GetProjectionString();
                if(!bKnown)
                    vecPresets.push_back({"Layer \"" + ptrLayer->GetName() + "\"", ptrSpatRef->GetProjectionString()});
            }
        }
    }

    std::vector<SCoordinateSystemPreset> CMapProject::GetCoordinateSystemPresets() const
    {
        std::vector<SCoordinateSystemPreset> vecPresets;
        vecPresets.push_back({"WGS 84, longitude / latitude (EPSG:4326)", "+proj=longlat +datum=WGS84 +no_defs"});
        vecPresets.push_back({"Web Mercator (EPSG:3857)", WebMercatorProj4()});

        // UTM zone of the center of the map
        Geometry::ISpatialReferencePtr ptrMapSpatRef = m_ptrMap->GetSpatialReference();
        if(ptrMapSpatRef.get() && HasDataLayers())
        {
            try
            {
                CommonLib::bbox bb = m_ptrMap->GetFullExtent(ptrMapSpatRef)->GetBoundingBox();
                CommonLib::GisXYPoint center;
                center.x = (bb.xMin + bb.xMax) / 2;
                center.y = (bb.yMin + bb.yMax) / 2;
                Geometry::ISpatialReferencePtr ptrWgs = std::make_shared<Geometry::CSpatialReferenceProj4>(std::string("+proj=longlat +datum=WGS84 +no_defs"));
                if(ptrMapSpatRef->Project(ptrWgs, &center) && std::isfinite(center.x) && std::isfinite(center.y) &&
                   center.x >= -180. && center.x <= 180. && center.y > -80. && center.y < 84.)
                {
                    int nZone = (std::min)(60, (int)std::floor((center.x + 180.) / 6.) + 1);
                    bool bSouth = center.y < 0.;
                    vecPresets.push_back({"UTM zone " + std::to_string(nZone) + (bSouth ? "S" : "N") + " of the map center, WGS 84 (EPSG:" +
                                          std::to_string((bSouth ? 32700 : 32600) + nZone) + ")",
                                          "+proj=utm +zone=" + std::to_string(nZone) + (bSouth ? " +south" : "") + " +datum=WGS84 +units=m +no_defs"});
                }
            }
            catch (std::exception&)
            {
                // no UTM preset
            }
        }

        CollectLayerSystems(m_ptrMap->GetLayers(), vecPresets);
        return vecPresets;
    }

    SLayerDataInfo CMapProject::GetLayerDataInfo(Cartography::IFeatureLayerPtr ptrLayer)
    {
        SLayerDataInfo info;
        GeoDatabase::ITablePtr ptrTable = ptrLayer.get() ? ptrLayer->GetLayerTable() : GeoDatabase::ITablePtr();
        if(!ptrTable.get())
            return info;

        info.sTable = ptrTable->GetDatasetName();
        if(ptrTable->GetDatasetViewName() != info.sTable && !ptrTable->GetDatasetViewName().empty())
            info.sTable += " (" + ptrTable->GetDatasetViewName() + ")";
        info.sWorkspace = DescribeWorkspace(GeoDatabase::CWorkspaceHolder::GetWorkspace(ptrTable->GetWorkspaceId()));
        info.sGeometryType = GeometryTypeName(ptrTable->GetGeometryType());

        Geometry::ISpatialReferencePtr ptrSpatRef = ptrTable->GetSpatialReference();
        if(ptrSpatRef.get())
            info.sSpatialReference = ptrSpatRef->GetProjectionString();
        Geometry::IEnvelopePtr ptrExtent = ptrTable->GetExtent();
        if(ptrExtent.get())
            info.extent = ptrExtent->GetBoundingBox();

        GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
        for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
        {
            GeoDatabase::IFieldPtr ptrField = ptrFields->GetField(i);
            SLayerDataInfo::SField field;
            field.sName = ptrField->GetName();
            field.sType = DataTypeName(ptrField->GetType());
            info.vecFields.push_back(field);

            if(IsIntegerType(ptrField->GetType()))
                info.vecOIDFields.push_back(field.sName);
            else if(ptrField->GetType() == GeoDatabase::dtGeometry)
                info.vecShapeFields.push_back(field.sName);
        }

        info.sTableOIDField = ptrTable->GetOIDFieldName();
        info.sTableShapeField = ptrTable->GetShapeFieldName();
        info.sOIDField = ptrLayer->GetOIDField();
        info.sShapeField = CurrentShapeField(ptrLayer);
        // the table default is shown as the default, not as a choice of the layer
        if(info.sOIDField == info.sTableOIDField)
            info.sOIDField.clear();
        if(info.sShapeField == info.sTableShapeField)
            info.sShapeField.clear();
        return info;
    }

    void CMapProject::SetLayerDataFields(Cartography::IFeatureLayerPtr ptrLayer, const std::string& sOIDField, const std::string& sShapeField)
    {
        GeoDatabase::ITablePtr ptrTable = ptrLayer.get() ? ptrLayer->GetLayerTable() : GeoDatabase::ITablePtr();
        if(!ptrTable.get())
            throw CommonLib::CExcBase("The layer has no table");

        GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
        if(!sOIDField.empty())
        {
            if(!ptrFields->FieldExists(sOIDField))
                throw CommonLib::CExcBase("OID field {0} isn't in the table", sOIDField);
            if(!IsIntegerType(ptrFields->GetField(sOIDField)->GetType()))
                throw CommonLib::CExcBase("OID field {0} isn't an integer field", sOIDField);
        }
        if(!sShapeField.empty())
        {
            if(!ptrFields->FieldExists(sShapeField))
                throw CommonLib::CExcBase("Shape field {0} isn't in the table", sShapeField);
            if(ptrFields->GetField(sShapeField)->GetType() != GeoDatabase::dtGeometry)
                throw CommonLib::CExcBase("Shape field {0} isn't a geometry field", sShapeField);
        }

        ptrLayer->SetOIDField(sOIDField);

        ptrLayer->SetShapeField(sShapeField);
        for(int i = 0, sz = ptrLayer->GetRendererCount(); i < sz; ++i)
            if(ptrLayer->GetRenderer(i).get())
                ptrLayer->GetRenderer(i)->SetShapeField(sShapeField);   // empty - the table default
        if(ptrLayer->GetAnnotationRenderer().get())
            ptrLayer->GetAnnotationRenderer()->SetShapeField(sShapeField);
        if(ptrLayer->GetLabelRenderer().get())
            ptrLayer->GetLabelRenderer()->SetShapeField(sShapeField);
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

    std::vector<STableInfo> CMapProject::GetSQLiteTables(const std::string& sDatabasePath)
    {
        try
        {
            std::string sDir;
            std::string sName;
            SplitShapefilePath(sDatabasePath, sDir, sName);

            // temporary workspace, not registered in CWorkspaceHolder
            GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace = GeoDatabase::CSQLiteWorkspace::Open(sName.c_str(), sDatabasePath.c_str(), CommonLib::CGuid::CreateNew());
            GeoDatabase::CSQLiteWorkspace* pSQLiteWorkspace = dynamic_cast<GeoDatabase::CSQLiteWorkspace*>(ptrWorkspace.get());

            std::vector<std::string> vecNames = pSQLiteWorkspace->GetSpatialTableNames();
            std::vector<STableInfo> vecTables;
            for(size_t i = 0; i < vecNames.size(); ++i)
            {
                STableInfo info;
                info.sName = vecNames[i];
                GeoDatabase::ITablePtr ptrTable = ptrWorkspace->GetTable(vecNames[i]);
                info.vecFields = GetTableFields(ptrTable);
                info.shapeType = ptrTable->GetGeometryType();
                vecTables.push_back(info);
            }
            return vecTables;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to read tables of SQLite database {0}", sDatabasePath, exc);
            throw;
        }
    }

    int CMapProject::AddConvertedLayers(GeoDatabase::IWorkspacePtr ptrWorkspace, Cartography::IMapPtr ptrSourceMap)
    {
        if(!ptrWorkspace.get() || !ptrSourceMap.get())
            return 0;

        Cartography::ILayersPtr ptrSourceLayers = ptrSourceMap->GetLayers();
        std::vector<Cartography::ILayerPtr> vecLayers;
        for(int i = 0; i < ptrSourceLayers->GetLayerCount(); ++i)
            vecLayers.push_back(ptrSourceLayers->GetLayer(i));
        if(vecLayers.empty())
            return 0;
        ptrSourceLayers->RemoveAllLayers();

        GeoDatabase::CWorkspaceHolder::AddWorkspace(ptrWorkspace);
        m_vecWorkspaces.push_back(ptrWorkspace);

        // the background of the converted map (OSM land color) when the project map has none
        if(!m_ptrMap->GetBackgroundSymbol().get() && ptrSourceMap->GetBackgroundSymbol().get())
            m_ptrMap->SetBackgroundSymbol(ptrSourceMap->GetBackgroundSymbol());

        // the source map keeps the drawing order (the bottom layer first)
        for(size_t i = 0; i < vecLayers.size(); ++i)
            AddLayer(vecLayers[i], ptrSourceMap->GetSpatialReference());
        return (int)vecLayers.size();
    }

    int CMapProject::AddSQLiteDatabase(const std::string& sDatabasePath, const std::string& sTableName, const SLayerParams& params)
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
            {
                GeoDatabase::ITablePtr ptrTable = ptrWorkspace->GetTable(vecTables[i]);
                GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
                SLayerParams tableParams;
                if(!params.annotation.sField.empty() && ptrFields->FieldExists(params.annotation.sField))
                    tableParams.annotation = params.annotation;
                if(!params.labels.sField.empty() && ptrFields->FieldExists(params.labels.sField))
                    tableParams.labels = params.labels;

                // symbology is made for one table: its field and its geometry type
                const SSymbology* pSymbology = params.ptrSymbology.get();
                if(pSymbology && (pSymbology->selector == SelectorSimple || ptrFields->FieldExists(pSymbology->sField)) &&
                   CSymbolFactory::GetGeometryKind(pSymbology->simpleSymbol.kind) == GeometryKindOf(ptrTable->GetGeometryType()))
                    tableParams.ptrSymbology = params.ptrSymbology;

                AddTable(ptrTable, vecTables[i], tableParams);
            }

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

    namespace
    {
        // symbols of the symbol selectors of the feature renderers (the label renderers are skipped)
        void ForEachLayerSymbol(Cartography::IFeatureLayerPtr ptrLayer, const std::function<void(Display::ISymbolPtr)>& func)
        {
            if(!ptrLayer.get())
                return;
            for(int r = 0; r < ptrLayer->GetRendererCount(); ++r)
            {
                Cartography::IFeatureRendererPtr ptrRenderer = ptrLayer->GetRenderer(r);
                if(!ptrRenderer.get() || std::dynamic_pointer_cast<Cartography::ILabelRenderer>(ptrRenderer).get())
                    continue;
                Cartography::ISymbolSelectorPtr ptrSelector = ptrRenderer->GetSymbolSelector();
                if(Cartography::ISimpleSymbolSelector* pSimple = dynamic_cast<Cartography::ISimpleSymbolSelector*>(ptrSelector.get()))
                    func(pSimple->GetSymbol());
                else if(Cartography::IUniqueValueSymbolSelector* pUnique = dynamic_cast<Cartography::IUniqueValueSymbolSelector*>(ptrSelector.get()))
                {
                    for(int i = 0; i < pUnique->GetValueCount(); ++i)
                        func(pUnique->GetSymbol(i));
                    func(pUnique->GetDefaultSymbol());
                }
                else if(Cartography::IRangeSymbolSelector* pRange = dynamic_cast<Cartography::IRangeSymbolSelector*>(ptrSelector.get()))
                {
                    for(int i = 0; i < pRange->GetRangeCount(); ++i)
                        func(pRange->GetSymbol(i));
                    func(pRange->GetDefaultSymbol());
                }
            }
        }
    }

    int CMapProject::GetLayerScaleDependent(Cartography::IFeatureLayerPtr ptrLayer)
    {
        bool bYes = false, bNo = false;
        ForEachLayerSymbol(ptrLayer, [&](Display::ISymbolPtr ptrSymbol)
        {
            if(!ptrSymbol.get())
                return;
            int n = CSymbolFactory::GetScaleDependent(ptrSymbol);
            if(n != CSymbolFactory::ScaleDependentNo)
                bYes = true;
            if(n != CSymbolFactory::ScaleDependentYes)
                bNo = true;
        });
        return bYes && bNo ? CSymbolFactory::ScaleDependentMixed : (bYes ? CSymbolFactory::ScaleDependentYes : CSymbolFactory::ScaleDependentNo);
    }

    void CMapProject::SetLayerScaleDependent(Cartography::IFeatureLayerPtr ptrLayer, bool bScaleDependent)
    {
        ForEachLayerSymbol(ptrLayer, [bScaleDependent](Display::ISymbolPtr ptrSymbol) { CSymbolFactory::SetScaleDependent(ptrSymbol, bScaleDependent); });
    }
}
