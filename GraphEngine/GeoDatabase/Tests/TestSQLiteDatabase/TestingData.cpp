#include "TestingData.h"
#include "../GeoDatabase/Field.h"
#include "../GeoDatabase/Fields.h"
#include "../GeoDatabase/FieldSet.h"


std::string CTestingData::m_sDatabaseName = "TestDatabase";
std::string CTestingData::m_sDatabasePath = "F:\\TestDatabase.sqlitedb";
std::string CTestingData::m_sTestTableName_1 = "TestTable_1";
std::string CTestingData::m_sTestTableName_2 = "TestTable_2";
std::string CTestingData::m_sTestTableName_3 = "TestTable_3";
GraphEngine::GeoDatabase::IFieldsPtr CTestingData::m_ptrFiles_1;
GraphEngine::GeoDatabase::IFieldsPtr CTestingData::m_ptrFiles_2;
GraphEngine::GeoDatabase::IFieldsPtr CTestingData::m_ptrFiles_3;


void CTestingData::Create()
{
    m_ptrFiles_1 = std::make_shared< GraphEngine::GeoDatabase::CFields>();
    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("OID");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtInteger64);
        ptrField->SetIsPrimaryKey(true);

        m_ptrFiles_1->AddField(ptrField);
    }


    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("FieldInt");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtInteger32);
        ptrField->SetIsDefault(100);

        m_ptrFiles_1->AddField(ptrField);
    }

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("FieldIntNullable");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtInteger32);
        ptrField->SetIsNullable(true);

        m_ptrFiles_1->AddField(ptrField);
    }

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("StringField");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtString);

        m_ptrFiles_1->AddField(ptrField);
    }

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("StringField1");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtString);
        ptrField->SetIsDefault("TestText");

        m_ptrFiles_1->AddField(ptrField);
    }


    m_ptrFiles_2 = std::make_shared< GraphEngine::GeoDatabase::CFields>();

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("OID_2");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtInteger64);
        ptrField->SetIsPrimaryKey(true);

        m_ptrFiles_2->AddField(ptrField);
    }

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("StringText");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtString);


        m_ptrFiles_2->AddField(ptrField);
    }


    m_ptrFiles_3 = std::make_shared< GraphEngine::GeoDatabase::CFields>();

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("OID_3");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtInteger64);
        ptrField->SetIsPrimaryKey(true);

        m_ptrFiles_3->AddField(ptrField);
    }

    {
        GraphEngine::GeoDatabase::IFieldPtr ptrField = std::make_shared<GraphEngine::GeoDatabase::CField>();
        ptrField->SetName("StringText");
        ptrField->SetType(GraphEngine::GeoDatabase::eDataTypes::dtString);


        m_ptrFiles_3->AddField(ptrField);
    }

}