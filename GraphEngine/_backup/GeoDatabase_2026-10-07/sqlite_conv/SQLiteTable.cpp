#include "SQLiteTable.h"
#include "SQLiteSelectCursor.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteTable::CSQLiteTable(CommonLib::CGuid workspaceId, const std::string& tableName,  const std::string& viewName,
                                   CommonLib::database::IDatabasePtr ptrDatabase) :
                     TBase(workspaceId, eDatasetType::dtTypeTable, tableName, viewName, ptrDatabase)
        {

        }


        CSQLiteTable::~CSQLiteTable()
        {

        }

        void CSQLiteTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            // workspace id + dataset name: CDatasetLoader::LoadTable reopens the table through its workspace
            TBase::Save(pObj);
        }

        void CSQLiteTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }




    }
}
