#include "TableCopier.h"
#include "QueryFilter.h"
#include "Field.h"

namespace GraphEngine {
    namespace GeoDatabase {

        namespace
        {
            bool IsIntegerType(eDataTypes type)
            {
                return type == dtInteger8 || type == dtInteger16 || type == dtInteger32 || type == dtInteger64 ||
                       type == dtUInteger8 || type == dtUInteger16 || type == dtUInteger32 || type == dtUInteger64;
            }
        }

        std::string CTableCopier::MakeValidTableName(const std::string& sName)
        {
            std::string sResult;
            for(size_t i = 0; i < sName.size(); ++i)
            {
                char ch = sName[i];
                bool bValid = (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '_';
                sResult += bValid ? ch : '_';
            }

            if(sResult.empty() || (sResult[0] >= '0' && sResult[0] <= '9'))
                sResult = "t_" + sResult;

            return sResult;
        }

        ITablePtr CTableCopier::CopySpatialTable(ITablePtr ptrSource, IDatabaseWorkspacePtr ptrTarget, const std::string& sTargetName,
                                                 TProgress progress, int64_t nProgressStep)
        {
            try
            {
                if(!ptrSource.get() || !ptrTarget.get())
                    throw CommonLib::CExcBase("Source table or target workspace is null");

                if(nProgressStep <= 0)
                    nProgressStep = 1000;

                // fields of the new table: the source fields + OID if the source has none
                IFieldsPtr ptrFields = ptrSource->GetFields()->Clone();
                std::string sOIDField = ptrSource->GetOIDFieldName();
                if(sOIDField.empty() || ptrFields->FindField(sOIDField) < 0)
                {
                    sOIDField = "OID";
                    int i = 0;
                    while(ptrFields->FieldExists(sOIDField))
                        sOIDField = "OID" + std::to_string(i++);

                    IFieldPtr ptrOid = std::make_shared<CField>();
                    ptrOid->SetName(sOIDField);
                    ptrOid->SetType(dtInteger64);
                    ptrOid->SetIsNullable(false);
                    ptrOid->SetIsPrimaryKey(true);
                    ptrFields->AddField(ptrOid);
                }

                std::string sShapeField = ptrSource->GetShapeFieldName();
                int nShapeField = ptrFields->FindField(sShapeField);
                if(nShapeField < 0)
                    throw CommonLib::CExcBase("Source table has no shape field");
                ptrFields->GetField(nShapeField)->SetType(dtGeometry);

                Geometry::IEnvelopePtr ptrExtent = ptrSource->GetExtent();
                Geometry::ISpatialReferencePtr ptrSpatRef = ptrSource->GetSpatialReference();

                ITablePtr ptrTargetTable = ptrTarget->CreateTableWithSpatialIndex(sTargetName, sTargetName, sTargetName + "_sidx", sShapeField, sOIDField, ptrFields,
                                                                                ptrSource->GetGeometryType(),
                                                                                ptrExtent.get() ? ptrExtent->Clone() : Geometry::IEnvelopePtr(),
                                                                                ptrSpatRef.get() ? ptrSpatRef->Clone() : Geometry::ISpatialReferencePtr());

                ITransactionPtr ptrTransaction = ptrTarget->StartTransaction(ttModify);
                IInsertCursorPtr ptrInsert = ptrTransaction->CreateInsertCusor(ptrTargetTable);

                // all rows of the source
                IQueryFilterPtr ptrFilter = std::make_shared<CQueryFilter>();
                ISelectCursorPtr ptrCursor = ptrSource->Search(ptrFilter);

                // target column -> source column
                std::vector<int32_t> vecSourceCol(ptrInsert->ColumnCount(), -1);
                for(int32_t col = 0; col < ptrInsert->ColumnCount(); ++col)
                    vecSourceCol[col] = ptrCursor->FindFieldByName(ptrInsert->ColumnName(col));

                int64_t nRows = 0;
                int64_t nAutoOid = 0;
                while(ptrCursor->Next())
                {
                    ++nAutoOid;
                    for(int32_t col = 0; col < ptrInsert->ColumnCount(); ++col)
                    {
                        eDataTypes type = ptrInsert->GetColumnType(col);
                        int32_t srcCol = vecSourceCol[col];

                        if(srcCol < 0 || ptrCursor->ColumnIsNull(srcCol))
                        {
                            if(ptrInsert->ColumnName(col) == sOIDField)
                                ptrInsert->BindInt64(col, nAutoOid); // generated OID
                            else
                                ptrInsert->BindBlob(col, nullptr, 0, true); // NULL
                            continue;
                        }

                        if(IsIntegerType(type))
                            ptrInsert->BindInt64(col, ptrCursor->ReadInt64(srcCol));
                        else if(type == dtFloat || type == dtDouble)
                            ptrInsert->BindDouble(col, ptrCursor->ReadDouble(srcCol));
                        else if(type == dtString)
                            ptrInsert->BindText(col, ptrCursor->ReadText(srcCol), true);
                        else if(type == dtGeometry)
                            ptrInsert->BindShape(col, ptrCursor->ReadShape(srcCol), true);
                        else if(type == dtBlob)
                        {
                            byte_t* pBuf = nullptr;
                            int32_t nSize = 0;
                            ptrCursor->ReadBlob(srcCol, &pBuf, nSize);
                            ptrInsert->BindBlob(col, pBuf, nSize, true);
                        }
                        else
                            ptrInsert->BindBlob(col, nullptr, 0, true);
                    }

                    ptrInsert->Next();
                    ++nRows;

                    if(progress && (nRows % nProgressStep) == 0 && !progress(nRows))
                    {
                        ptrTransaction->Rollback();
                        throw CommonLib::CExcBase("Copy is canceled");
                    }
                }

                ptrTransaction->Commit();
                if(progress)
                    progress(nRows);

                return ptrTargetTable;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to copy table to {0}", sTargetName, exc);
                throw;
            }
        }

    }
}
