#pragma once

#include "BPtreeDBTableBase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CBPtreeDBTable : public CBPtreeDBTableBase<ITable>
        {
        public:

            typedef CBPtreeDBTableBase<ITable> TBase;

            CBPtreeDBTable(CommonLib::CGuid workspaceId, const std::string& tableName,  const std::string& viewName,
                           CBPtreeDBContextPtr ptrContext);
            virtual  ~CBPtreeDBTable();

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);
        };

    }
}
