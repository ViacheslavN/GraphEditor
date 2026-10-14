#include "BPtreeDBSelectCursor.h"
#include "BPtreeDBUtils.h"
#include "../../../CommonLib/str/StringEncoding.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"
#include "../Row.h"
#include "../Fields.h"
#include "../Field.h"
#include "../Utils.h"

namespace GraphEngine {
    namespace GeoDatabase {

        CBPtreeDBSelectCursor::CBPtreeDBSelectCursor(const std::string& sQuery, CBPtreeDBContextPtr ptrContext) :
                TBase(IFieldsPtr()),
                m_ptrContext(ptrContext)
        {
            try
            {
                m_ptrTran = m_ptrContext->GetReadTransaction(m_bOwnTran);
                m_ptrCursor = m_ptrTran->executeQuery(sQuery);
                InitFields(IFieldsPtr());
            }
            catch (std::exception& exc)
            {
                Close();
                CommonLib::CExcBase::RegenExc("CBPtreeDBSelectCursor: failed to execute {0}", sQuery, exc);
                throw;
            }
        }

        CBPtreeDBSelectCursor::CBPtreeDBSelectCursor(const std::string& sTableName, const std::string& sShapeFieldName, IFieldsPtr ptrSourceFields,
                                                     IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource, CBPtreeDBContextPtr ptrContext) :
                TBase(IFieldsPtr(), ptrFilter, ptrSpatRefSource),
                m_ptrContext(ptrContext),
                m_sShapeFieldName(sShapeFieldName),
                m_ptrSpatRefSource(ptrSpatRefSource)
        {
            try
            {
                CommonLib::bbox extent;
                bool bSpatial = !sShapeFieldName.empty() && GetQueryExtent(extent);

                bool bSQL = !ptrFilter->GetWhereClause().empty() || !ptrFilter->GetJoins().empty();
                // a field without the spatial index (f.e. dropped for a bulk load): SQL checks the boxes row by row
                if(bSpatial && !bSQL && !CBPtreeDBUtils::HasSpatialIndex(m_ptrContext->GetDBTable(sTableName), sShapeFieldName))
                    bSQL = true;

                m_ptrTran = m_ptrContext->GetReadTransaction(m_bOwnTran);
                if(bSQL)
                {
                    m_ptrCursor = m_ptrTran->executeQuery(CreateSQLQuery(ptrSourceFields, sTableName, bSpatial, extent));
                }
                else
                {
                    // the API of bptreedb: no SQL compilation. ROWID isn't a field there: the row id, a column after the
                    // fields of the cursor
                    IFieldSetPtr ptrFilterFields = ptrFilter->GetFieldSet();
                    std::vector<std::string> vecFields;
                    bool bRowId = false;
                    if(ptrFilterFields.get() && ptrFilterFields->GetCount() != 0)
                    {
                        for(int i = 0, sz = ptrFilterFields->GetCount(); i < sz; ++i)
                        {
                            if(CBPtreeDBUtils::IsRowIdName(ptrFilterFields->Get(i)) && (!ptrSourceFields.get() || ptrSourceFields->FindField(ptrFilterFields->Get(i)) < 0))
                                bRowId = true;
                            else
                                vecFields.push_back(ptrFilterFields->Get(i));
                        }
                    }
                    else if(ptrSourceFields.get())
                    {
                        for(int i = 0, sz = ptrSourceFields->GetFieldCount(); i < sz; ++i)
                            vecFields.push_back(ptrSourceFields->GetField(i)->GetName());
                    }
                    bptreedb::IFieldSetPtr ptrFieldSet = vecFields.empty() ? bptreedb::IFieldSetPtr() : bptreedb::IFieldSet::Create(vecFields);
                    if(bSpatial)
                        m_ptrCursor = m_ptrTran->executeSpatialQuery(extent, sTableName, sShapeFieldName,
                                                                     CBPtreeDBUtils::SpatialRel2QueryMode(m_spatialRel), ptrFieldSet);
                    else
                        m_ptrCursor = m_ptrTran->executeSelectQuery(sTableName, ptrFieldSet);
                    if(bRowId)
                        m_nRowIdCol = m_ptrCursor->ColumnCount();
                }

                InitFields(ptrSourceFields);
            }
            catch (std::exception& exc)
            {
                Close();
                CommonLib::CExcBase::RegenExc("CBPtreeDBSelectCursor: failed to search the table {0}", sTableName, exc);
                throw;
            }
        }

        CBPtreeDBSelectCursor::~CBPtreeDBSelectCursor()
        {
            Close();
        }

        void CBPtreeDBSelectCursor::Close()
        {
            m_ptrCursor.reset();
            if(m_bOwnTran && m_ptrTran.get())
            {
                try
                {
                    if(m_ptrTran->IsActive())
                        m_ptrTran->Commit();   // read only
                }
                catch (...)
                {
                }
            }
            m_ptrTran.reset();
            m_bOwnTran = false;
        }

        bool CBPtreeDBSelectCursor::GetQueryExtent(CommonLib::bbox& extent) const
        {
            // the filter extent in the coordinates of the table (ICursorBase projects it if the table has a system)
            if(m_ptrExtentSource.get())
            {
                extent = m_ptrExtentSource->GetBoundingBox();
                return IsNormalBox(extent);
            }

            // a table without a spatial reference: the filter extent as it is
            ISpatialFilter* pSpatialFilter = dynamic_cast<ISpatialFilter*>(m_ptrFilter.get());
            if(!pSpatialFilter || pSpatialFilter->GetSpatialRel() == srlUndefined)
                return false;

            const_cast<CBPtreeDBSelectCursor*>(this)->m_spatialRel = pSpatialFilter->GetSpatialRel();
            extent = pSpatialFilter->GetBB();
            if(!IsNormalBox(extent) && pSpatialFilter->GetShape().get())
                extent = pSpatialFilter->GetShape()->GetBB();

            return IsNormalBox(extent);
        }

        std::string CBPtreeDBSelectCursor::CreateSQLQuery(IFieldsPtr ptrSourceFields, const std::string& sTableName, bool bSpatial, const CommonLib::bbox& extent)
        {
            std::string sql = CreateSelectPartQuery(m_ptrFilter, ptrSourceFields);
            const std::vector<IJoinPtr>& vecJoins = m_ptrFilter->GetJoins();
            const std::string& sPrefix = m_ptrFilter->GetTablePrefix();
            const std::string& sWhere = m_ptrFilter->GetWhereClause();

            sql += " FROM " + sTableName;
            if(!sPrefix.empty())
                sql += " AS " + sPrefix;

            if(!vecJoins.empty())
                sql += CreateJoinPartQuery(vecJoins, sPrefix);

            std::string sConditions;
            if(bSpatial)
            {
                const char* pszFunc = "ST_Intersects";
                switch(CBPtreeDBUtils::SpatialRel2QueryMode(m_spatialRel))
                {
                    case bptreedb::sqmInside: pszFunc = "ST_Within"; break;
                    case bptreedb::sqmOver:   pszFunc = "ST_Contains"; break;
                    default: break;
                }

                std::string sGeom = (sPrefix.empty() ? sTableName : sPrefix) + "." + m_sShapeFieldName;
                sConditions = std::string(pszFunc) + "(" + sGeom + ", ST_MakeEnvelope(" +
                              CBPtreeDBUtils::DoubleToSQL(extent.xMin) + ", " + CBPtreeDBUtils::DoubleToSQL(extent.yMin) + ", " +
                              CBPtreeDBUtils::DoubleToSQL(extent.xMax) + ", " + CBPtreeDBUtils::DoubleToSQL(extent.yMax) + "))";
            }

            if(!sWhere.empty())
            {
                if(!sConditions.empty())
                    sConditions += " AND ";
                sConditions += "(" + sWhere + ")";
            }

            if(!sConditions.empty())
                sql += " WHERE " + sConditions;

            return sql;
        }

        void CBPtreeDBSelectCursor::InitFields(IFieldsPtr ptrSourceFields)
        {
            IFieldsPtr ptrFields = std::make_shared<CFields>();
            for(int32_t i = 0, sz = m_ptrCursor->ColumnCount(); i < sz; ++i)
            {
                std::string sName = m_ptrCursor->ColumnName(i);
                IFieldPtr ptrField;
                int nIndex = ptrSourceFields.get() ? ptrSourceFields->FindField(sName) : -1;
                if(nIndex >= 0)
                    ptrField = ptrSourceFields->GetField(nIndex)->Clone();
                else
                {
                    ptrField = std::make_shared<CField>();
                    ptrField->SetName(sName);
                    ptrField->SetType(CBPtreeDBUtils::DBType2FieldType(m_ptrCursor->GetColumnType(i)));
                }

                if(!m_sShapeFieldName.empty() && sName == m_sShapeFieldName)
                    ptrField->SetType(dtGeometry);
                // the row id of SQL (ROWID, _ROWID_, OID without such a field): the name the filter asks for
                if(nIndex < 0 && CBPtreeDBUtils::IsRowIdName(sName))
                {
                    ptrField->SetName(CBPtreeDBUtils::RowIdField());
                    ptrField->SetType(dtInteger64);
                }

                ptrFields->AddField(ptrField);
            }
            if(m_nRowIdCol >= 0)
            {
                IFieldPtr ptrField = std::make_shared<CField>();
                ptrField->SetName(CBPtreeDBUtils::RowIdField());
                ptrField->SetType(dtInteger64);
                ptrField->SetIsNullable(false);
                ptrFields->AddField(ptrField);
            }
            m_ptrFields = ptrFields;
        }

        const bptreedb::ISelectCursor* CBPtreeDBSelectCursor::Cursor() const
        {
            if(!m_ptrCursor.get())
                throw CommonLib::CExcBase("CBPtreeDBSelectCursor: no current row");

            return m_ptrCursor.get();
        }

        bool CBPtreeDBSelectCursor::Next()
        {
            if(!m_ptrCursor.get())
                return false;

            try
            {
                if(m_ptrCursor->Next())
                    return true;
            }
            catch (std::exception& exc)
            {
                Close();
                CommonLib::CExcBase::RegenExc("CBPtreeDBSelectCursor: failed to read the next row", exc);
                throw;
            }

            Close();   // all rows are read: the read transaction ends
            return false;
        }

        int64_t CBPtreeDBSelectCursor::GetRowID() const
        {
            return Cursor()->GetRowID();
        }

        bool CBPtreeDBSelectCursor::ColumnIsNull(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return false;
            return Cursor()->ColumnIsNull(col);
        }

        int32_t CBPtreeDBSelectCursor::GetColumnBytes(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return (int32_t)sizeof(int64_t);
            return Cursor()->GetColumnBytes(col);
        }

        // bptreedb converts a number to the type asked for if the value fits (range checked)
        int8_t CBPtreeDBSelectCursor::ReadInt8(int32_t col) const
        {
            return Cursor()->ReadInt8(col);
        }

        uint8_t CBPtreeDBSelectCursor::ReadUInt8(int32_t col) const
        {
            return Cursor()->ReadUInt8(col);
        }

        int16_t CBPtreeDBSelectCursor::ReadInt16(int32_t col) const
        {
            return Cursor()->ReadInt16(col);
        }

        uint16_t CBPtreeDBSelectCursor::ReadUInt16(int32_t col) const
        {
            return Cursor()->ReadUInt16(col);
        }

        int32_t CBPtreeDBSelectCursor::ReadInt32(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return (int32_t)GetRowID();
            return Cursor()->ReadInt32(col);
        }

        uint32_t CBPtreeDBSelectCursor::ReadUInt32(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return (uint32_t)GetRowID();
            return Cursor()->ReadUInt32(col);
        }

        int64_t CBPtreeDBSelectCursor::ReadInt64(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return GetRowID();
            return Cursor()->ReadInt64(col);
        }

        uint64_t CBPtreeDBSelectCursor::ReadUInt64(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return (uint64_t)GetRowID();
            return Cursor()->ReadUInt64(col);
        }

        float CBPtreeDBSelectCursor::ReadFloat(int32_t col) const
        {
            return Cursor()->ReadFloat(col);
        }

        double CBPtreeDBSelectCursor::ReadDouble(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return (double)GetRowID();
            return Cursor()->ReadDouble(col);
        }

        void CBPtreeDBSelectCursor::ReadText(int32_t col, std::string& text) const
        {
            if(col == m_nRowIdCol)
            {
                text = std::to_string(GetRowID());
                return;
            }
            Cursor()->ReadText(col, text);
        }

        std::string CBPtreeDBSelectCursor::ReadText(int32_t col) const
        {
            if(col == m_nRowIdCol)
                return std::to_string(GetRowID());
            return Cursor()->ReadText(col);
        }

        void CBPtreeDBSelectCursor::ReadTextW(int32_t col, std::wstring& text) const
        {
            Cursor()->ReadTextW(col, text);
        }

        std::wstring CBPtreeDBSelectCursor::ReadTextW(int32_t col) const
        {
            return Cursor()->ReadTextW(col);
        }

        void CBPtreeDBSelectCursor::ReadBlob(int col, byte_t **pBuf, int32_t& size) const
        {
            const byte_t* pData = nullptr;
            Cursor()->ReadBlob(col, &pData, size);
            *pBuf = const_cast<byte_t*>(pData);   // the buffer of the cursor, valid until the next Next
        }

        CommonLib::CGuid CBPtreeDBSelectCursor::ReadGuid(int32_t col) const
        {
            return CommonLib::CGuid(ReadText(col));
        }

        CommonLib::IGeoShapePtr CBPtreeDBSelectCursor::ReadShape(int32_t col) const
        {
            CommonLib::IGeoShapePtr ptrShape = Cursor()->ReadShape(col);

            // the table and the output (map) coordinate systems differ: the shape is given in the output one.
            // The shape of the bptreedb cursor is its value: a copy is projected.
            if(m_bNeedTransform && m_ptrSpatRefSource.get() && m_ptrFilter.get() && ptrShape.get() && ptrShape->GetPointCnt() > 0)
            {
                if(m_ptrCacheShape.get() == nullptr)
                    m_ptrCacheShape = std::make_shared<CommonLib::CGeoShape>();

                m_ptrCacheShape->Import(ptrShape->Data(), ptrShape->Size());
                m_ptrSpatRefSource->Project(m_ptrFilter->GetOutputSpatialReference(), m_ptrCacheShape);
                return m_ptrCacheShape;
            }

            return ptrShape;
        }

        IRowPtr CBPtreeDBSelectCursor::CreateRow() const
        {
            return std::make_shared<CRow>(m_ptrFields);
        }

        void CBPtreeDBSelectCursor::FillRow(IRowPtr ptrRow) const
        {
            CGeoDatabaseUtils::FillRow((ISelectCursor*)this, ptrRow);
        }

    }
}
