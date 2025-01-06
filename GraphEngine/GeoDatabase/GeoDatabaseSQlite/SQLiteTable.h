#pragma once

#include "../TableBase.h"
#include "../../CommonLib/sqlitelib/Database.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CSQLiteTable : public ITableBase<ITable>
        {
        public:
            CSQLiteTable();
            virtual  ~CSQLiteTable();
        private:
            CommonLib::database::IDatabasePtr m_ptrDatabase;
        };

    }
    }