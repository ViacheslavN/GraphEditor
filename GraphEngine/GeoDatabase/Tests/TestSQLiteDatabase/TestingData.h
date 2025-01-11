#pragma once
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"

class CTestingData
{
public:

    static void Create();

    static std::string m_sDatabaseName;
    static std::string m_sDatabasePath;
    static std::string m_sTestTableName;
    static GraphEngine::GeoDatabase::IFieldsPtr m_ptrFiles;
};
