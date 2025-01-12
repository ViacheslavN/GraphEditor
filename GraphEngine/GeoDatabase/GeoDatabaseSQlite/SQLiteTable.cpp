#include "SQLiteTable.h"
#include "SQLiteSelectCursor.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CSQLiteTable::CSQLiteTable(const std::string& tableName,  const std::string& viewName,
                                   CommonLib::database::IDatabasePtr ptrDatabase) :
                     TBase(eDatasetType::dtTypeTable, tableName, viewName, ptrDatabase)
        {

        }

        CSQLiteTable::~CSQLiteTable()
        {

        }

        void CSQLiteTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {

        }

        void CSQLiteTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }




    }
}
