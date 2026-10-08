#pragma once

#ifndef NOMINMAX
#define NOMINMAX // windows.h min/max macros break std::min/max
#endif

#include <catch2/catch_test_macros.hpp>

#include "../../Cartography.h"
#include "../../Map.h"
#include "../../layers/Layers.h"
#include "../../layers/FeatureLayer.h"
#include "../../layers/LoaderLayers.h"
#include "../../renders/FeatureRenderer.h"
#include "../../renders/AnnotationRenderer.h"
#include "../../renders/RenderersLoader.h"
#include "../../selectors/SimpleSymbolSelector.h"
#include "../../selectors/SymbolSelectorsLoader.h"
#include "../../selectors/UniqueValueSymbolSelector.h"
#include "../../selectors/RangeSymbolSelector.h"
#include "../../selection/Selection.h"

#include "../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../CommonLib/xml/XMLNode.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../DisplayLib/Symbols/SimpleLineSymbol.h"
#include "../../../DisplayLib/Symbols/TextSymbol.h"
#include "../../../GeoDatabase/Field.h"
#include "../../../GeoDatabase/Fields.h"
#include "../../../GeoDatabase/FieldSet.h"
#include "../../../GeoDatabase/QueryFilter.h"
#include "../../../GeoDatabase/Row.h"

namespace cartography_test
{
    using namespace GraphEngine;

    // empty in-memory serialize object (XML backend)
    inline CommonLib::ISerializeObjPtr CreateSerializeRoot()
    {
        CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "root");
        return std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
    }

    inline std::shared_ptr<Cartography::CFeatureLayer> CreateFeatureLayer(const std::string& name)
    {
        std::shared_ptr<Cartography::CFeatureLayer> ptrLayer = std::make_shared<Cartography::CFeatureLayer>();
        ptrLayer->SetName(name);
        ptrLayer->SetVisible(true);
        return ptrLayer;
    }

    // symbol which only counts calls, works without a real display
    class CCountingSymbol : public Display::ISymbol
    {
    public:
        int nPrepare = 0;
        int nDraw = 0;
        int nReset = 0;
        int nFlush = 0;
        CommonLib::IGeoShapePtr ptrLastShape;

        virtual uint32_t GetSymbolID() const {return Display::UndefineSymbolID;}
        virtual void Init(Display::IDisplayPtr) {}
        virtual void Reset() {++nReset;}
        virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const {return ptrShape.get() != nullptr;}
        virtual void Draw(Display::IDisplayPtr, CommonLib::IGeoShapePtr ptrShape) {++nDraw; ptrLastShape = ptrShape;}
        virtual void FlushBuffers(Display::IDisplayPtr, Display::ITrackCancelPtr) {++nFlush;}
        virtual void GetBoundaryRect(CommonLib::IGeoShapePtr, Display::IDisplayPtr, Display::GRect&) const {}
        virtual bool GetScaleDependent() const {return false;}
        virtual void SetScaleDependent(bool) {}
        virtual bool GetDrawToBuffers() const {return false;}
        virtual void SetDrawToBuffers(bool) {}
        virtual void DrawDirectly(Display::IDisplayPtr, const Display::GPoint*, const int*, int) {}
        virtual void DrawGeometryEx(Display::IDisplayPtr, const Display::GPoint*, const int*, int) {}
        virtual void QueryBoundaryRectEx(Display::IDisplayPtr, const Display::GPoint*, const int*, int, Display::GRect&) const {}
        virtual void Prepare(Display::IDisplayPtr) {++nPrepare;}
        virtual void Save(CommonLib::ISerializeObjPtr) const {}
        virtual void Load(CommonLib::ISerializeObjPtr) {}
    };
    typedef std::shared_ptr<CCountingSymbol> CCountingSymbolPtr;

    inline GeoDatabase::IFieldPtr CreateField(const std::string& name, GeoDatabase::eDataTypes type)
    {
        std::shared_ptr<GeoDatabase::CField> ptrField = std::make_shared<GeoDatabase::CField>();
        ptrField->SetName(name);
        ptrField->SetType(type);
        return ptrField;
    }

    inline CommonLib::IGeoShapePtr CreatePoint(double x, double y)
    {
        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        ptrShape->Create(CommonLib::shape_type_point, 1);
        ptrShape->GetPoints()[0].x = x;
        ptrShape->GetPoints()[0].y = y;
        return ptrShape;
    }

    // minimal table: only fields and names are used by renderers/selectors
    class CTestTable : public GeoDatabase::ITable
    {
    public:
        CTestTable(GeoDatabase::IFieldsPtr ptrFields) : m_ptrFields(ptrFields), m_sOIDField("OID"), m_sShapeField("Shape")
        {}

        virtual void Save(CommonLib::ISerializeObjPtr) const {}
        virtual void Load(CommonLib::ISerializeObjPtr) {}
        virtual CommonLib::CGuid GetWorkspaceId() const {return CommonLib::CGuid();}
        virtual GeoDatabase::eDatasetType GetDatasetType() const {return GeoDatabase::dtSpatialTable;}
        virtual const std::string& GetDatasetName() const {return m_sName;}
        virtual const std::string& GetDatasetViewName() const {return m_sName;}

        virtual void SetOIDFieldName(const std::string& fieldName) {m_sOIDField = fieldName;}
        virtual const std::string& GetOIDFieldName() const {return m_sOIDField;}
        virtual void AddField(GeoDatabase::IFieldPtr field) {m_ptrFields->AddField(field);}
        virtual void DeleteField(const std::string&) {}
        virtual GeoDatabase::IFieldsPtr GetFields() const {return m_ptrFields;}
        virtual GeoDatabase::IFieldSetPtr GetFieldsSet() const {return GeoDatabase::IFieldSetPtr();}
        virtual void SetFields(GeoDatabase::IFieldsPtr ptrFields) {m_ptrFields = ptrFields;}
        virtual GeoDatabase::ISelectCursorPtr Search(GeoDatabase::IQueryFilterPtr) {return GeoDatabase::ISelectCursorPtr();}
        virtual GeoDatabase::ISelectCursorPtr Select(const std::string&) {return GeoDatabase::ISelectCursorPtr();}

        virtual CommonLib::eShapeType GetGeometryType() const {return CommonLib::shape_type_point;}
        virtual void SetGeometryType(CommonLib::eShapeType) {}
        virtual Geometry::IEnvelopePtr GetExtent() const {return Geometry::IEnvelopePtr();}
        virtual Geometry::ISpatialReferencePtr GetSpatialReference() const {return Geometry::ISpatialReferencePtr();}
        virtual void SetExtent(Geometry::IEnvelopePtr) {}
        virtual void SetSpatialReference(Geometry::ISpatialReferencePtr) {}
        virtual const std::string& GetShapeFieldName() const {return m_sShapeField;}
        virtual void SetShapeFieldName(const std::string& fieldName) {m_sShapeField = fieldName;}
        virtual void SetSpatialIndexName(const std::string&) {}
        virtual const std::string& GetSpatialIndexName() const {return m_sName;}

    private:
        GeoDatabase::IFieldsPtr m_ptrFields;
        std::string m_sName;
        std::string m_sOIDField;
        std::string m_sShapeField;
    };
}
