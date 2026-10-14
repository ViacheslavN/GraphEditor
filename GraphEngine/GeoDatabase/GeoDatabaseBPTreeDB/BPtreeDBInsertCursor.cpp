#include "BPtreeDBInsertCursor.h"
#include "BPtreeDBUtils.h"
#include "../../../CommonLib/str/StringEncoding.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CBPtreeDBInsertCursor::CBPtreeDBInsertCursor(const std::string&  sqlInsertQuery, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext) :
                m_ptrContext(ptrContext),
                m_ptrTran(ptrTran)
        {
            try
            {
                m_ptrStatement = m_ptrTran->prepare(sqlInsertQuery);
                m_ptrContext->InvalidatePendingBoxes();   // rows the boxes don't know
                m_vecNull.assign(m_ptrStatement->GetParameterCount(), false);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBInsertCursor: failed to prepare {0}", sqlInsertQuery, exc);
                throw;
            }
        }

        CBPtreeDBInsertCursor::CBPtreeDBInsertCursor(ITablePtr pTable, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContextPtr ptrContext) :
                m_ptrContext(ptrContext),
                m_ptrTran(ptrTran)
        {
            try
            {
                m_sTableName = pTable->GetDatasetName();
                m_ptrFields = pTable->GetFields();

                std::vector<std::string> vecFields;
                for(int i = 0, sz = m_ptrFields->GetFieldCount(); i < sz; ++i)
                    vecFields.push_back(m_ptrFields->GetField(i)->GetName());

                m_ptrCursor = m_ptrTran->createInsertCursor(m_sTableName, bptreedb::IFieldSet::Create(vecFields));

                if(!pTable->GetShapeFieldName().empty())
                    m_nShapeFieldIndex = m_ptrFields->FindField(pTable->GetShapeFieldName());

                // a table created in the bulk load mode: its indexes are built at the end
                m_ptrPending = m_ptrContext->GetPendingIndexes(m_sTableName);
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBInsertCursor: failed to create insert cursor for {0}", pTable->GetDatasetName(), exc);
                throw;
            }
        }

        CBPtreeDBInsertCursor::~CBPtreeDBInsertCursor()
        {

        }

        void CBPtreeDBInsertCursor::SetBulkSpatialIndex(const std::string& /*sTempDirUtf8*/, uint64_t /*nMemoryBytes*/)
        {
            try
            {
                if(!m_ptrCursor.get() || m_nShapeFieldIndex < 0 || m_ptrPending.get())
                    return;

                // dropping an index needs a DDL transaction: in a modify one the index is updated row by row
                if(m_ptrTran->GetTransactionDataType() != bptreedb::eTT_DDL)
                    return;

                std::string sShapeField = m_ptrFields->GetField(m_nShapeFieldIndex)->GetName();
                bptreedb::ITablePtr ptrTable = m_ptrContext->GetDBTableThrow(m_sTableName);
                bptreedb::IIndexPtr ptrIndex = ptrTable->GetIndex(sShapeField);
                if(!ptrIndex.get())
                    return;

                SBPtreeDBSpatialIndexParams params;
                CommonLib::bbox extent;
                if(ptrIndex->GetType() == bptreedb::itSpatialZOrder)
                {
                    // the grid of the new index has to cover the rows the table has already: only an empty table
                    if(ptrTable->GetRowCount() != 0)
                        return;

                    params.type = bsitZOrder;
                    CBPtreeDBUtils::SSpatialTableInfo info;
                    if(CBPtreeDBUtils::ReadSpatialTableInfo(m_sTableName, info, m_ptrTran, m_ptrContext.get()))
                        extent = info.extent;
                }
                else if(ptrIndex->GetType() != bptreedb::itSpatialRTree)
                    return;

                ptrTable->DropIndex(sShapeField, m_ptrTran);

                m_ptrPending = m_ptrContext->AddPendingIndexes(m_sTableName);
                m_ptrPending->sSpatialField = sShapeField;
                m_ptrPending->spatialParams = params;
                m_ptrPending->AddExtent(extent);
                m_ptrPending->bCompleteBoxes = false;   // the rows inserted before aren't known: the index reads them
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBInsertCursor: failed to set the bulk spatial index of {0}", m_sTableName, exc);
                throw;
            }
        }

        void CBPtreeDBInsertCursor::Finish()
        {
            if(m_bFinished)
                return;

            try
            {
                m_bFinished = true;
                if(m_ptrCursor.get())
                    m_ptrCursor->Finish();

                if(m_ptrPending.get())
                {
                    m_ptrPending.reset();
                    m_ptrContext->BuildPendingIndexes(m_sTableName, m_ptrTran);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBInsertCursor: failed to build the indexes of {0}", m_sTableName, exc);
                throw;
            }
        }

        void CBPtreeDBInsertCursor::CheckColumn(int32_t col) const
        {
            if(col < 0 || col >= ColumnCount())
                throw CommonLib::CExcBase("CBPtreeDBInsertCursor: column {0} is out of range", col);
        }

        int32_t  CBPtreeDBInsertCursor::ColumnCount() const
        {
            if(m_ptrStatement.get())
                return (int32_t)m_ptrStatement->GetParameterCount();

            return m_ptrFields->GetFieldCount();
        }

        std::string CBPtreeDBInsertCursor::ColumnName(int32_t col) const
        {
            CheckColumn(col);
            if(m_ptrStatement.get())
                return "?" + std::to_string(col);

            return m_ptrFields->GetField(col)->GetName();
        }

        bool CBPtreeDBInsertCursor::ColumnIsNull(int32_t col) const
        {
            CheckColumn(col);
            if(m_ptrStatement.get())
                return m_vecNull[col];

            return m_ptrCursor->ColumnIsNull(col);
        }

        eDataTypes CBPtreeDBInsertCursor::GetColumnType(int32_t col) const
        {
            CheckColumn(col);
            if(m_ptrStatement.get())
                return dtUnknown;

            return m_ptrFields->GetField(col)->GetType();
        }

        int32_t CBPtreeDBInsertCursor::GetColumnBytes(int32_t col) const
        {
            CheckColumn(col);
            if(m_ptrStatement.get())
                return 0;

            return m_ptrFields->GetField(col)->GetLength();
        }

        void CBPtreeDBInsertCursor::Next()
        {
            try
            {
                if(m_bFinished)
                    throw CommonLib::CExcBase("the cursor is finished");

                if(m_ptrStatement.get())
                {
                    m_ptrStatement->execute();
                    return;
                }

                m_nLastRowID = m_ptrCursor->Next();

                if(m_ptrPending.get() && IsNormalBox(m_bbox))
                {
                    m_ptrPending->AddExtent(m_bbox);
                    m_ptrPending->AddBox(m_bbox, m_nLastRowID);   // the spatial index is built from the boxes
                }
                m_bbox.type = CommonLib::bbox_type_invalid;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("CBPtreeDBInsertCursor: failed to insert a row into {0}", m_sTableName, exc);
                throw;
            }
        }

        void CBPtreeDBInsertCursor::Bind(int32_t col, const CommonLib::CVariant& value)
        {
            CheckColumn(col);
            if(m_ptrStatement.get())
            {
                m_ptrStatement->BindParameter((uint32_t)col, value);
                m_vecNull[col] = value.IsNull();
            }
            else
                m_ptrCursor->BindValue(col, value);
        }

        void CBPtreeDBInsertCursor::BindInt8(int32_t col, int8_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindInt8(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindUInt8(int32_t col, uint8_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindUInt8(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindInt16(int32_t col, int16_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindInt16(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindUInt16(int32_t col, uint16_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindUInt16(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindInt32(int32_t col, int32_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindInt32(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindUInt32(int32_t col, uint32_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindUInt32(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindInt64(int32_t col, int64_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindInt64(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindUInt64(int32_t col, uint64_t val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindUInt64(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindFloat(int32_t col, float val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindFloat(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindDouble(int32_t col, double val)
        {
            if(m_ptrCursor.get())
                m_ptrCursor->BindDouble(col, val);
            else
                Bind(col, CommonLib::CVariant(val));
        }

        void CBPtreeDBInsertCursor::BindText(int32_t col, const std::string& text, bool copy)
        {
            // bptreedb always copies the values
            if(m_ptrCursor.get())
                m_ptrCursor->BindText(col, text, true);
            else
                Bind(col, CommonLib::CVariant(text));
        }

        void CBPtreeDBInsertCursor::BindTextW(int32_t col, const std::wstring& text, bool copy)
        {
            BindText(col, CommonLib::StringEncoding::str_w2utf8_safe(text), true);
        }

        void CBPtreeDBInsertCursor::BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy)
        {
            if(m_ptrCursor.get())
            {
                if(pBuf == nullptr)
                    m_ptrCursor->BindNull(col);
                else
                    m_ptrCursor->BindBlob(col, pBuf, size, true);
            }
            else
            {
                if(pBuf == nullptr)
                    Bind(col, CommonLib::CVariant());
                else
                    Bind(col, CommonLib::CVariant(std::string((const char*)pBuf, (size_t)size)));   // a blob as bytes
            }
        }

        void CBPtreeDBInsertCursor::BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape, bool copy)
        {
            if(!m_ptrCursor.get())
            {
                Bind(col, ptrShape.get() ? CommonLib::CVariant(ptrShape) : CommonLib::CVariant());
                return;
            }

            if(!ptrShape.get())
            {
                m_ptrCursor->BindNull(col);  // NULL: not in the spatial index
                return;
            }

            if(col == m_nShapeFieldIndex && m_ptrPending.get())
                m_bbox = ptrShape->GetBB();

            m_ptrCursor->BindShape(col, ptrShape);
        }

        void CBPtreeDBInsertCursor::BindGuid(int32_t col, const CommonLib::CGuid& guid)
        {
            BindText(col, guid.ToAstr(false), true);
        }

    }
}
