
#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "TestingData.h"
#include "../../GeoDatabaseShape/ShapefileWorkspace.h"


void TestSpatialData()
{
    try {

        std::string path = "F:\\TestData";
        std::string tableName = "building";

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrShapeWks =
                std::dynamic_pointer_cast<GraphEngine::GeoDatabase::IDatabaseWorkspace>(GraphEngine::GeoDatabase::CShapfileWorkspace::Open("Test", path.c_str(), 1));

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrSqliteWks = GraphEngine::GeoDatabase::CSQLiteWorkspace::Open(CTestingData::m_sDatabaseName.c_str(), CTestingData::m_sDatabasePath.c_str(), 2);


        GraphEngine::GeoDatabase::ITablePtr ptrShapeFile = ptrShapeWks->GetTable("building");




        ptrSqliteWks->CreateTableWithSpatialIndex(tableName, tableName, "building_shape_index", ptrShapeFile->GetShapeFieldName(), ptrShapeFile->GetOIDFieldName(), ptrShapeFile->GetFields(),
                                         ptrShapeFile->GetGeometryType(), ptrShapeFile->GetExtent()->Clone(), ptrShapeFile->GetSpatialReference()->Clone() );

        GraphEngine::GeoDatabase::ITablePtr ptrSqliteTable =ptrSqliteWks->GetTable(tableName);

        ptrSqliteTable->SetShapeFieldName(ptrShapeFile->GetShapeFieldName());
        ptrSqliteTable->SetOIDFieldName(ptrShapeFile->GetOIDFieldName());
        ptrSqliteTable->SetSpatialReference(ptrShapeFile->GetSpatialReference());
        ptrSqliteTable->SetSpatialIndexName("building_shape_index");
        ptrSqliteTable->SetExtent(ptrShapeFile->GetExtent());

        GraphEngine::GeoDatabase::ISpatialFilterPtr ptrFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
        GraphEngine::Geometry::IEnvelopePtr ptrExtent =  ptrShapeFile->GetExtent();


        ptrFilter->SetBB(ptrExtent->GetBoundingBox());
        ptrFilter->SetOutputSpatialReference(ptrShapeFile->GetSpatialReference());
        ptrFilter->SetSpatialRel(GraphEngine::GeoDatabase::srlIntersects);

        GraphEngine::GeoDatabase::ISelectCursorPtr ptrCursor = ptrShapeFile->Search(ptrFilter);


        GraphEngine::GeoDatabase::ITransactionPtr ptrTransaction = ptrSqliteWks->StartTransaction(GraphEngine::GeoDatabase::eTransactionType::ttModify);

        GraphEngine::GeoDatabase::IInsertCursorPtr ptrInsertCursor = ptrTransaction->CreateInsertCusor(ptrSqliteTable);

        while (ptrCursor->Next()) {

            for(int i = 0; i < ptrCursor->ColumnCount(); ++i)
            {
                GraphEngine::GeoDatabase::eDataTypes fieldType = ptrCursor->GetColumnType(i);


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
                        ptrInsertCursor->BindInt64(i, ptrCursor->ReadInt64(i));
                    }
                        break;
                    case  GraphEngine::GeoDatabase::dtDouble:
                    case  GraphEngine::GeoDatabase::dtFloat:
                        ptrInsertCursor->BindDouble(i, ptrCursor->ReadDouble(i));
                        break;

                    case GraphEngine::GeoDatabase::dtString:
                        text = ptrCursor->ReadText(i);
                        ptrInsertCursor->BindText(i, text, true);
                        break;
                    case GraphEngine::GeoDatabase::dtBlob:
                        ptrShape = ptrCursor->ReadShape(i);
                        ptrInsertCursor->BindShape(i, ptrShape, true);
                        break;
                }
            }

            ptrInsertCursor->Next();


        }

        ptrTransaction->Commit();


        GraphEngine::GeoDatabase::ISpatialFilterPtr ptrSearchSqliteFilter = std::make_shared<GraphEngine::GeoDatabase::CQueryFilter>();
        GraphEngine::Geometry::IEnvelopePtr ptrSqliteExtent =  ptrSqliteTable->GetExtent();


        ptrSearchSqliteFilter->SetBB(ptrExtent->GetBoundingBox());
        ptrSearchSqliteFilter->SetOutputSpatialReference(ptrSqliteExtent->GetSpatialReference());
        ptrSearchSqliteFilter->SetSpatialRel(GraphEngine::GeoDatabase::srlIntersects);

        GraphEngine::GeoDatabase::ISelectCursorPtr ptrSqliteSeachCursor = ptrSqliteTable->Search(ptrFilter);



        while (ptrSqliteSeachCursor->Next()) {


            for (int i = 0; i < ptrSqliteSeachCursor->ColumnCount(); ++i) {
                GraphEngine::GeoDatabase::eDataTypes fieldType = ptrSqliteSeachCursor->GetColumnType(i);
                int intVal;
                double dVal;
                std::string text;
                CommonLib::IGeoShapePtr ptrShape;
                switch (fieldType) {
                    case GraphEngine::GeoDatabase::dtInteger8:
                    case GraphEngine::GeoDatabase::dtInteger16:
                    case GraphEngine::GeoDatabase::dtInteger32:
                    case GraphEngine::GeoDatabase::dtInteger64:
                    case GraphEngine::GeoDatabase::dtUInteger8:
                    case GraphEngine::GeoDatabase::dtUInteger16:
                    case GraphEngine::GeoDatabase::dtUInteger32:
                    case GraphEngine::GeoDatabase::dtUInteger64: {
                        intVal = ptrSqliteSeachCursor->ReadInt32(i);
                    }
                        break;
                    case GraphEngine::GeoDatabase::dtDouble:
                    case GraphEngine::GeoDatabase::dtFloat:
                        dVal = ptrSqliteSeachCursor->ReadDouble(i);
                        break;

                    case GraphEngine::GeoDatabase::dtString:
                        text = ptrSqliteSeachCursor->ReadText(i);
                        break;
                    case GraphEngine::GeoDatabase::dtBlob:
                        ptrShape = ptrSqliteSeachCursor->ReadShape(i);
                        break;
                }
            }
        }

    }
    catch(std::exception& exc)
    {
        CommonLib::CExcBase::RegenExc("Failed to test spatial data", exc);
        throw ;
    }

}