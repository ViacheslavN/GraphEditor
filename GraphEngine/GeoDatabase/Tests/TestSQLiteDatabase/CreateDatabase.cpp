#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "TestingData.h"


void CreateDatabase()
{
    try {

        if(CommonLib::CFileUtils::IsFileExist(CTestingData::m_sDatabasePath))
            CommonLib::CFileUtils::DelFile(CTestingData::m_sDatabasePath);

        GraphEngine::GeoDatabase::IDatabaseWorkspacePtr ptrWks = GraphEngine::GeoDatabase::CSQLiteWorkspace::Create(CTestingData::m_sDatabaseName.c_str(), CTestingData::m_sDatabasePath.c_str(), 1);
        ptrWks->CreateTable(CTestingData::m_sTestTableName_1, CTestingData::m_sTestTableName_1, CTestingData::m_ptrFiles_1);
        ptrWks->CreateTable(CTestingData::m_sTestTableName_2, CTestingData::m_sTestTableName_2, CTestingData::m_ptrFiles_2);
        ptrWks->CreateTable(CTestingData::m_sTestTableName_3, CTestingData::m_sTestTableName_3, CTestingData::m_ptrFiles_3);

    }
    catch(std::exception& exc)
    {
        CommonLib::CExcBase::RegenExc("Failed to create database", exc);
        throw ;
    }
}