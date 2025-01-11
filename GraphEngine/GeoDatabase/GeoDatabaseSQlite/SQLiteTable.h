#pragma once

#include "SQLiteTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteTable : public CSQLiteTableBase<ITable>
        {
        public:

            typedef CSQLiteTableBase<ITable> TBase;

            CSQLiteTable(const std::string& tableName,  const std::string& viewName,
                         CommonLib::database::IDatabasePtr ptrDatabase);
            virtual  ~CSQLiteTable();

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:

        };

    }
    }