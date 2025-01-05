
#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseShape/ShapefileSpatialTable.h"
#include "../../QueryFilter.h"

int main()
{
    try
    {
        std::string path = "F:\\TestData";
        GraphEngine::GeoDatabase::CShapefileSpatialTable shapeFile(path, "building", "building");
        GraphEngine::GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();

        GraphEngine::Geometry::IEnvelopePtr ptrExtent =  shapeFile.GetExtent();

        ptrFilter->SetBB(ptrExtent->GetBoundingBox());
        ptrFilter->SetOutputSpatialReference(shapeFile.GetSpatialReference());
        ptrFilter->SetSpatialRel(GraphEngine::GeoDatabase::srlIntersects);

        GraphEngine::GeoDatabase::ICursorPtr ptrCursor = shapeFile.Search(ptrFilter);



        while (ptrCursor->NextRow())
        {
            GraphEngine::GeoDatabase::IRowPtr ptrRow = ptrCursor->GetCurrentRow();

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

