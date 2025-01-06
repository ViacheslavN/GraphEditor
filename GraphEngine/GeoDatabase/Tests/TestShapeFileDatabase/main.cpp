
#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseShape/ShapefileSpatialTable.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "../../GeoDatabaseShape/ShapefileWorkspace.h"

int main()
{
    try
    {
        std::string path = "F:\\TestData";
        std::string pathExport = "F:\\TestData\\Export";

        try
        {
            CommonLib::CFileUtils::FileDelFolder(pathExport);
        }
        catch(...)
        {

        }

        CommonLib::CFileUtils::CreateDirectory(pathExport);

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrSrcWks =
            std::dynamic_pointer_cast<GraphEngine::GeoDatabase::IDatabaseWorkspace>(GraphEngine::GeoDatabase::CShapfileWorkspace::Open("Test", path.c_str(), 1));

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrExportWks =
                std::dynamic_pointer_cast<GraphEngine::GeoDatabase::IDatabaseWorkspace>(GraphEngine::GeoDatabase::CShapfileWorkspace::Open("TestExport", pathExport.c_str(), 2));

        GraphEngine::GeoDatabase::ISpatialTablePtr ptrShapeFile = ptrSrcWks->GetSpatialTable("building");


        GraphEngine::GeoDatabase::ISpatialTablePtr ptrExportShapeFile = ptrExportWks->CreateSpatialTable("building", "building", ptrShapeFile->GetFields(), "");

        GraphEngine::GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();

        GraphEngine::Geometry::IEnvelopePtr ptrExtent =  ptrShapeFile->GetExtent();

        ptrFilter->SetBB(ptrExtent->GetBoundingBox());
        ptrFilter->SetOutputSpatialReference(ptrShapeFile->GetSpatialReference());
        ptrFilter->SetSpatialRel(GraphEngine::GeoDatabase::srlIntersects);

        GraphEngine::GeoDatabase::ICursorPtr ptrCursor = ptrShapeFile->Search(ptrFilter);

        while (ptrCursor->NextRow())
        {
            GraphEngine::GeoDatabase::ISelectRowPtr ptrRow = ptrCursor->GetCurrentRow();

           for(int i = 0; i < ptrRow->ColumnCount(); ++i)
           {
               GraphEngine::GeoDatabase::eDataTypes fieldType =  ptrRow->GetColumnType(i);
               int intVal;
               double dVal;
               std::string  text;
               CommonLib::IGeoShapePtr  ptrShape;
               switch (fieldType)
               {
                   case  GraphEngine::GeoDatabase::dtInteger8:
                   case  GraphEngine::GeoDatabase::dtInteger16:
                   case  GraphEngine::GeoDatabase::dtInteger32:
                   case  GraphEngine::GeoDatabase::dtInteger64:
                   case  GraphEngine::GeoDatabase::dtUInteger8:
                   case  GraphEngine::GeoDatabase::dtUInteger16:
                   case  GraphEngine::GeoDatabase::dtUInteger32:
                   case  GraphEngine::GeoDatabase::dtUInteger64:
                   {
                       intVal = ptrRow->ReadInt32(i);
                   }
                   break;
                   case  GraphEngine::GeoDatabase::dtDouble:
                   case  GraphEngine::GeoDatabase::dtFloat:
                       dVal = ptrRow->ReadDouble(i);
                       break;

                   case GraphEngine::GeoDatabase::dtString:
                       text = ptrRow->ReadText(i);
                       break;
                   case GraphEngine::GeoDatabase::dtGeometry:
                       ptrShape = ptrRow->ReadShape(i);
                       break;
               }
           }
        }

    }
    catch(std::exception& exc)
    {
        std::cout <<"Error: " <<exc.what();
    }


    return 0;

}

