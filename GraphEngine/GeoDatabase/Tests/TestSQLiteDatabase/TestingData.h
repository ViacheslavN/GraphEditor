#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"

class CTestingData
{
public:

    static void Create();

    static std::string m_sDatabaseName;
    static std::string m_sDatabasePath;
    static std::string m_sTestTableName_1;
    static GraphEngine::GeoDatabase::IFieldsPtr m_ptrFiles_1;

    static std::string m_sTestTableName_2;
    static GraphEngine::GeoDatabase::IFieldsPtr m_ptrFiles_2;

    static std::string m_sTestTableName_3;
    static GraphEngine::GeoDatabase::IFieldsPtr m_ptrFiles_3;
};
