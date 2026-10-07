#pragma once

#ifndef NOMINMAX
#define NOMINMAX // windows.h min/max macros break std::min/max
#endif

#include <catch2/catch_test_macros.hpp>

#include "../../GeoDatabase.h"
#include "../../Field.h"
#include "../../Fields.h"
#include "../../FieldSet.h"
#include "../../QueryFilter.h"
#include "../../Row.h"
#include "../../WorkspaceHolder.h"
#include "../../DatasetLoader.h"
#include "../../TableCopier.h"
#include "../../GeoDatabaseShape/ShapefileWorkspace.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../GeoDatabaseSQlite/SQLiteUtils.h"
#include "../../../CommonLib/Serialize/SerializeXML.h"
#include "../../../CommonLib/xml/XMLNode.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../../../ThirdParty/ShapeLib/shapefil.h"

#include <filesystem>
#include <cmath>

namespace geodatabase_test
{
    using namespace GraphEngine;

    inline std::filesystem::path TestDir()
    {
        std::filesystem::path dir = std::filesystem::temp_directory_path() / "GraphEngineGeoDatabaseTests";
        std::filesystem::create_directories(dir);
        return dir;
    }

    inline void RemoveDatabase(const std::string& sPath)
    {
        for(const char* ext : {"", "-wal", "-shm", "-journal"})
            std::filesystem::remove(sPath + ext);
    }

    inline CommonLib::ISerializeObjPtr CreateSerializeRoot()
    {
        CommonLib::xml::IXMLNodePtr ptrNode = std::make_shared<CommonLib::xml::CXMLNode>(CommonLib::xml::IXMLNodePtr(), "root");
        return std::make_shared<CommonLib::CSerializeObjXML>(ptrNode);
    }

    inline GeoDatabase::IFieldPtr CreateField(const std::string& name, GeoDatabase::eDataTypes type, bool bPrimaryKey = false)
    {
        std::shared_ptr<GeoDatabase::CField> ptrField = std::make_shared<GeoDatabase::CField>();
        ptrField->SetName(name);
        ptrField->SetType(type);
        ptrField->SetIsNullable(!bPrimaryKey);
        ptrField->SetIsPrimaryKey(bPrimaryKey);
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

    // Polygons: 5 squares 10x10 at x = 0, 20, 40, 60, 80 (y 0..10)
    // NAME: "P0".."P4", CODE: 100 + i (NULL for record 3), VALUE: i * 1.5
    // with a .prj (WGS 84 lat/long)
    inline std::string CreatePolygonsShapefile()
    {
        static std::string sDir;
        if(!sDir.empty())
            return sDir;

        std::filesystem::path dir = TestDir() / "shp";
        std::filesystem::create_directories(dir);
        std::string base = (dir / "squares").string();

        SHPHandle hShp = SHPCreate(base.c_str(), SHPT_POLYGON);
        DBFHandle hDbf = DBFCreate(base.c_str());
        REQUIRE(hShp != nullptr);
        REQUIRE(hDbf != nullptr);
        DBFAddField(hDbf, "NAME", FTString, 10, 0);
        DBFAddField(hDbf, "CODE", FTInteger, 6, 0);
        DBFAddField(hDbf, "VALUE", FTDouble, 12, 3);

        for(int i = 0; i < 5; ++i)
        {
            double x0 = 20. * i;
            double x[5] = {x0, x0, x0 + 10., x0 + 10., x0};
            double y[5] = {0., 10., 10., 0., 0.};
            SHPObject* pObj = SHPCreateSimpleObject(SHPT_POLYGON, 5, x, y, nullptr);
            SHPWriteObject(hShp, -1, pObj);
            SHPDestroyObject(pObj);

            std::string name = "P" + std::to_string(i);
            DBFWriteStringAttribute(hDbf, i, 0, name.c_str());
            if(i == 3)
                DBFWriteNULLAttribute(hDbf, i, 1);
            else
                DBFWriteIntegerAttribute(hDbf, i, 1, 100 + i);
            DBFWriteDoubleAttribute(hDbf, i, 2, i * 1.5);
        }
        SHPClose(hShp);
        DBFClose(hDbf);

        FILE* pPrj = fopen((base + ".prj").c_str(), "w");
        REQUIRE(pPrj != nullptr);
        fputs("GEOGCS[\"GCS_WGS_1984\",DATUM[\"D_WGS_1984\",SPHEROID[\"WGS_1984\",6378137,298.257223563]],PRIMEM[\"Greenwich\",0],UNIT[\"Degree\",0.017453292519943295]]", pPrj);
        fclose(pPrj);

        sDir = dir.string();
        return sDir;
    }

    // Points without .prj: (1,1), (2,2), (3,3)
    inline std::string CreatePointsShapefile()
    {
        static std::string sDir;
        if(!sDir.empty())
            return sDir;

        std::filesystem::path dir = TestDir() / "shp";
        std::filesystem::create_directories(dir);
        std::string base = (dir / "points").string();

        SHPHandle hShp = SHPCreate(base.c_str(), SHPT_POINT);
        DBFHandle hDbf = DBFCreate(base.c_str());
        DBFAddField(hDbf, "ID", FTInteger, 6, 0);
        for(int i = 0; i < 3; ++i)
        {
            double x = i + 1.;
            double y = i + 1.;
            SHPObject* pObj = SHPCreateSimpleObject(SHPT_POINT, 1, &x, &y, nullptr);
            SHPWriteObject(hShp, -1, pObj);
            SHPDestroyObject(pObj);
            DBFWriteIntegerAttribute(hDbf, i, 0, i + 1);
        }
        SHPClose(hShp);
        DBFClose(hDbf);

        sDir = dir.string();
        return sDir;
    }

    inline GeoDatabase::IDatabaseWorkspacePtr OpenShapeWorkspace(const std::string& sDir)
    {
        return std::dynamic_pointer_cast<GeoDatabase::IDatabaseWorkspace>(GeoDatabase::CShapfileWorkspace::Open("shp", sDir.c_str(), CommonLib::CGuid::CreateNew()));
    }

    inline std::shared_ptr<GeoDatabase::CQueryFilter> CreateBBoxFilter(double xMin, double yMin, double xMax, double yMax, Geometry::ISpatialReferencePtr ptrSpatRef)
    {
        std::shared_ptr<GeoDatabase::CQueryFilter> ptrFilter = std::make_shared<GeoDatabase::CQueryFilter>();
        CommonLib::bbox bb;
        bb.type = CommonLib::bbox_type_normal;
        bb.xMin = xMin; bb.yMin = yMin; bb.xMax = xMax; bb.yMax = yMax;
        ptrFilter->SetBB(bb);
        ptrFilter->SetSpatialRel(GeoDatabase::srlIntersects);
        ptrFilter->SetOutputSpatialReference(ptrSpatRef);
        return ptrFilter;
    }

    inline std::vector<int64_t> ReadOids(GeoDatabase::ISelectCursorPtr ptrCursor, const std::string& sOidField)
    {
        std::vector<int64_t> oids;
        int32_t col = ptrCursor->FindFieldByName(sOidField);
        REQUIRE(col >= 0);
        while(ptrCursor->Next())
            oids.push_back(ptrCursor->ReadInt64(col));
        std::sort(oids.begin(), oids.end());
        return oids;
    }
}
