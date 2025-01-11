
#include <iostream>
#include "../../../CommonLib/CommonLib.h"
#include "../../GeoDatabase.h"
#include "../../GeoDatabaseSQlite/SQLiteWorkspace.h"
#include "../../QueryFilter.h"
#include "../../../CommonLib/filesystem/filesystem.h"
#include "TestingData.h"

void CreateDatabase();



int main()
{
    try {

        CTestingData::Create();

        CreateDatabase();


    }
    catch(std::exception& exc)
    {
        std::cout <<"Error: " <<exc.what();
    }

    return 0;
}