#include "../GeoDatabase.h"
#include "../../CommonLib/sqlitelib/Database.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteUtils
        {
        public:
            static void FieldType2SQLiteType(eDataTypes type, std::string& sSQLiteType);
            static eDataTypes SQLiteType2FieldType(const std::string& sSQLiteType);
            static eDataTypes SQLiteType2FieldType(int nSQLiteFieldType);
            static IFieldsPtr ReadFields(const std::string& sTable, CommonLib::database::IDatabasePtr ptrDatabase);
        };

    }
}