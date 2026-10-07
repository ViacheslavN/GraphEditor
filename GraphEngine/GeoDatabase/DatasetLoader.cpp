#include "DatasetLoader.h"
#include "WorkspaceHolder.h"
#include "GeoDatabaseShape/ShapefileWorkspace.h"
#include "GeoDatabaseSQlite/SQLiteWorkspace.h"

namespace GraphEngine {
    namespace GeoDatabase {

    ITablePtr CDatasetLoader::LoadTable(CommonLib::ISerializeObjPtr ptrObj)
    {
        try
        {
            CommonLib::CGuid wksId = ptrObj->GetPropertyGuid("WorkspaceId");
            std::string sDatasetName =  ptrObj->GetPropertyString("DatasetName");

            IWorkspacePtr ptrWorkspace = CWorkspaceHolder::GetWorkspace(wksId);
            if(!ptrWorkspace.get())
                throw CommonLib::CExcBase("Workspace {0} isn't opened", wksId.ToAstr(true));

            IDatabaseWorkspace* pDatabaseWorkspace = dynamic_cast<IDatabaseWorkspace*>(ptrWorkspace.get());
            if(!pDatabaseWorkspace)
                throw CommonLib::CExcBase("Workspace {0} doesn't contain tables", wksId.ToAstr(true));

            ITablePtr ptrTable = pDatabaseWorkspace->GetTable(sDatasetName);
            if(!ptrTable.get())
                throw CommonLib::CExcBase("Table {0} not found", sDatasetName);

            return ptrTable;
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to LoadTable", exc);
            throw;
        }
    }

    IWorkspacePtr CDatasetLoader::LoadWorkspace(CommonLib::ISerializeObjPtr ptrObj)
    {
        try
        {
            eWorkspaceType type = (eWorkspaceType)ptrObj->GetPropertyInt32U("WksType", wtUndefined);
            switch (type)
            {
                case wtShapeFile:
                    return CShapfileWorkspace::Open(ptrObj);
                case wtSqlLite:
                    return CSQLiteWorkspace::Open(ptrObj);
                default:
                    throw CommonLib::CExcBase("Unsupported workspace type: {0}", (int)type);
            }
        }
        catch (std::exception& exc)
        {
            CommonLib::CExcBase::RegenExc("Failed to load workspace", exc);
            throw;
        }
    }

    }
}
