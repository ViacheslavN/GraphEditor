#include "BPtreeDBTable.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CBPtreeDBTable::CBPtreeDBTable(CommonLib::CGuid workspaceId, const std::string& tableName,  const std::string& viewName,
                                       CBPtreeDBContextPtr ptrContext) :
                TBase(workspaceId, eDatasetType::dtTypeTable, tableName, viewName, ptrContext)
        {
            // the OID is the bptreedb row id, read as the column ROWID (the select cursor gives it, IDeleteCursor deletes
            // by it); an OID column of the source is an ordinary field
            m_sOIDFieldName = CBPtreeDBUtils::RowIdField();
        }

        CBPtreeDBTable::~CBPtreeDBTable()
        {

        }

        void CBPtreeDBTable::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            // workspace id + dataset name: CDatasetLoader::LoadTable reopens the table through its workspace
            TBase::Save(pObj);
        }

        void CBPtreeDBTable::Load(CommonLib::ISerializeObjPtr pObj)
        {

        }

    }
}
