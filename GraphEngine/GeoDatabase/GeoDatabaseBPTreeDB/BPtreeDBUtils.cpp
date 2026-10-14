#include "BPtreeDBUtils.h"
#include "../Fields.h"
#include "../Field.h"
#include <limits>
#include <sstream>
#include <algorithm>

namespace GraphEngine {
    namespace GeoDatabase {

        namespace
        {
            // a transaction for reading that is committed when it goes out of scope (if it is its own)
            class CReadTransactionScope
            {
            public:
                explicit CReadTransactionScope(CBPtreeDBContext* pContext)
                {
                    m_ptrTran = pContext->GetReadTransaction(m_bOwn);
                }

                ~CReadTransactionScope()
                {
                    try
                    {
                        if(m_bOwn && m_ptrTran.get() && m_ptrTran->IsActive())
                            m_ptrTran->Commit();
                    }
                    catch (...)
                    {
                    }
                }

                bptreedb::ITransactionPtr Get() const { return m_ptrTran; }

            private:
                bptreedb::ITransactionPtr m_ptrTran;
                bool m_bOwn{ false };
            };

            // columns of GE_SPATIAL_TABLES
            enum eMetaColumns
            {
                mcTableName = 0,
                mcViewName,
                mcShapeField,
                mcOIDField,
                mcSpatialIndex,
                mcShapeType,
                mcXMin,
                mcYMin,
                mcXMax,
                mcYMax,
                mcSpatialReference,
                mcCount
            };

            const char* MetaColumnName(int col)
            {
                static const char* names[mcCount] = { "TableName", "ViewName", "ShapeField", "OIDField", "SpatialIndex",
                                                      "ShapeType", "XMin", "YMin", "XMax", "YMax", "SpatialReference" };
                return names[col];
            }

            bptreedb::IFieldSetPtr MetaFieldSet()
            {
                std::vector<std::string> vecFields;
                for(int i = 0; i < mcCount; ++i)
                    vecFields.push_back(MetaColumnName(i));

                return bptreedb::IFieldSet::Create(vecFields);
            }

            std::vector<CommonLib::CVariant> MetaValues(const CBPtreeDBUtils::SSpatialTableInfo& info)
            {
                std::vector<CommonLib::CVariant> values(mcCount);
                values[mcTableName] = info.sTableName;
                values[mcViewName] = info.sViewName;
                values[mcShapeField] = info.sShapeField;
                values[mcOIDField] = info.sOIDField;
                values[mcSpatialIndex] = info.sSpatialIndex;
                values[mcShapeType] = (int32_t)info.shapeType;
                // no extent (an empty table): NULL
                if(IsNormalBox(info.extent))
                {
                    values[mcXMin] = info.extent.xMin;
                    values[mcYMin] = info.extent.yMin;
                    values[mcXMax] = info.extent.xMax;
                    values[mcYMax] = info.extent.yMax;
                }
                values[mcSpatialReference] = info.sSpatialReference;
                return values;
            }

            std::string ReadTextOrEmpty(bptreedb::ISelectCursorPtr ptrCursor, int32_t col)
            {
                return ptrCursor->ColumnIsNull(col) ? std::string() : ptrCursor->ReadText(col);
            }
        }

        bptreedb::eDataTypes CBPtreeDBUtils::FieldType2DBType(eDataTypes type)
        {
            switch(type)
            {
                case dtInteger8:   return bptreedb::dtInteger8;
                case dtInteger16:  return bptreedb::dtInteger16;
                case dtInteger32:  return bptreedb::dtInteger32;
                case dtInteger64:  return bptreedb::dtInteger64;
                case dtUInteger8:  return bptreedb::dtUInteger8;
                case dtUInteger16: return bptreedb::dtUInteger16;
                case dtUInteger32: return bptreedb::dtUInteger32;
                case dtUInteger64: return bptreedb::dtUInteger64;
                case dtFloat:      return bptreedb::dtFloat;
                case dtDouble:     return bptreedb::dtDouble;
                case dtString:     return bptreedb::dtString;
                case dtGuid:       return bptreedb::dtString;
                case dtDate:       return bptreedb::dtString;   // as text (f.e. "YYYYMMDD" of a DBF)
                case dtBlob:       return bptreedb::dtBlob;
                case dtGeometry:   return bptreedb::dtGeometry;
                default:
                    throw CommonLib::CExcBase("Failed to convert to bptreedb type, unsupported type: {0}", (int)type);
            }
        }

        eDataTypes CBPtreeDBUtils::DBType2FieldType(bptreedb::eDataTypes type)
        {
            switch(type)
            {
                case bptreedb::dtNull:       return dtNull;
                case bptreedb::dtInteger8:   return dtInteger8;
                case bptreedb::dtInteger16:  return dtInteger16;
                case bptreedb::dtInteger32:  return dtInteger32;
                case bptreedb::dtInteger64:  return dtInteger64;
                case bptreedb::dtUInteger8:  return dtUInteger8;
                case bptreedb::dtUInteger16: return dtUInteger16;
                case bptreedb::dtUInteger32: return dtUInteger32;
                case bptreedb::dtUInteger64: return dtUInteger64;
                case bptreedb::dtOid:        return dtInteger64;
                case bptreedb::dtFloat:      return dtFloat;
                case bptreedb::dtDouble:     return dtDouble;
                case bptreedb::dtString:     return dtString;
                case bptreedb::dtBlob:       return dtBlob;
                case bptreedb::dtGeometry:   return dtGeometry;
                default:                     return dtUnknown;
            }
        }

        bool CBPtreeDBUtils::IsIntegerType(eDataTypes type)
        {
            return type == dtInteger8 || type == dtInteger16 || type == dtInteger32 || type == dtInteger64 ||
                   type == dtUInteger8 || type == dtUInteger16 || type == dtUInteger32 || type == dtUInteger64;
        }

        CommonLib::CVariant CBPtreeDBUtils::AnyToVariant(const std::any& value)
        {
            if(!value.has_value())
                return CommonLib::CVariant();

            const std::type_info& type = value.type();
            if(type == typeid(int8_t))        return CommonLib::CVariant(std::any_cast<int8_t>(value));
            if(type == typeid(uint8_t))       return CommonLib::CVariant(std::any_cast<uint8_t>(value));
            if(type == typeid(int16_t))       return CommonLib::CVariant(std::any_cast<int16_t>(value));
            if(type == typeid(uint16_t))      return CommonLib::CVariant(std::any_cast<uint16_t>(value));
            if(type == typeid(int32_t))       return CommonLib::CVariant(std::any_cast<int32_t>(value));
            if(type == typeid(uint32_t))      return CommonLib::CVariant(std::any_cast<uint32_t>(value));
            if(type == typeid(int64_t))       return CommonLib::CVariant(std::any_cast<int64_t>(value));
            if(type == typeid(uint64_t))      return CommonLib::CVariant(std::any_cast<uint64_t>(value));
            if(type == typeid(long))          return CommonLib::CVariant((int64_t)std::any_cast<long>(value));
            if(type == typeid(unsigned long)) return CommonLib::CVariant((uint64_t)std::any_cast<unsigned long>(value));
            if(type == typeid(float))         return CommonLib::CVariant(std::any_cast<float>(value));
            if(type == typeid(double))        return CommonLib::CVariant(std::any_cast<double>(value));
            if(type == typeid(bool))          return CommonLib::CVariant(std::any_cast<bool>(value));
            if(type == typeid(std::string))   return CommonLib::CVariant(std::any_cast<std::string>(value));
            if(type == typeid(std::wstring))  return CommonLib::CVariant(std::any_cast<std::wstring>(value));
            if(type == typeid(const char*))   return CommonLib::CVariant(std::string(std::any_cast<const char*>(value)));

            throw CommonLib::CExcBase("unsupported type of the default value: {0}", std::string(type.name()));
        }

        std::any CBPtreeDBUtils::VariantToAny(const CommonLib::CVariant& value)
        {
            if(value.IsNull())                     return std::any();
            if(value.IsType<int8_t>())             return std::any(value.Get<int8_t>());
            if(value.IsType<uint8_t>())            return std::any(value.Get<uint8_t>());
            if(value.IsType<int16_t>())            return std::any(value.Get<int16_t>());
            if(value.IsType<uint16_t>())           return std::any(value.Get<uint16_t>());
            if(value.IsType<int32_t>())            return std::any(value.Get<int32_t>());
            if(value.IsType<uint32_t>())           return std::any(value.Get<uint32_t>());
            if(value.IsType<int64_t>())            return std::any(value.Get<int64_t>());
            if(value.IsType<uint64_t>())           return std::any(value.Get<uint64_t>());
            if(value.IsType<float>())              return std::any(value.Get<float>());
            if(value.IsType<double>())             return std::any(value.Get<double>());
            if(value.IsType<bool>())               return std::any(value.Get<bool>());
            if(value.IsType<CommonLib::astr_t>())  return std::any(value.Get<CommonLib::astr_t>());
            if(value.IsType<CommonLib::wstr_t>())  return std::any(value.Get<CommonLib::wstr_t>());

            return std::any();   // blobs, shapes: no default value in GeoDatabase
        }

        uint32_t CBPtreeDBUtils::StringLength(int nLength, uint32_t nDefaultLength)
        {
            if(nLength <= 0)
                return nDefaultLength;

            return (uint32_t)nLength * 4;
        }

        IFieldsPtr CBPtreeDBUtils::ReadFields(bptreedb::ITablePtr ptrTable)
        {
            try
            {
                IFieldsPtr ptrFields = std::make_shared<CFields>();
                bptreedb::IFieldsPtr ptrDBFields = ptrTable->GetFields();
                for(int32_t i = 0, sz = ptrDBFields->GetFieldCount(); i < sz; ++i)
                {
                    bptreedb::IFieldPtr ptrDBField = ptrDBFields->GetField(i);

                    IFieldPtr ptrField = std::make_shared<CField>();
                    ptrField->SetName(ptrDBField->GetName());
                    ptrField->SetAliasName(ptrDBField->GetAlias());
                    ptrField->SetType(DBType2FieldType(ptrDBField->GetType()));
                    ptrField->SetLength((int)ptrDBField->GetLength());
                    ptrField->SetIsNullable(!ptrDBField->GetIsNotNull());

                    bptreedb::IIndexPtr ptrIndex = ptrDBField->GetIndex();
                    ptrField->SetIsPrimaryKey(ptrDBField->GetIsUnique() || (ptrIndex.get() && ptrIndex->GetType() == bptreedb::itUnique));

                    std::any defValue = VariantToAny(ptrDBField->GetDefaultValue());
                    if(defValue.has_value())
                        ptrField->SetIsDefault(defValue);

                    ptrFields->AddField(ptrField);
                }

                return ptrFields;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read fields of the table {0}", ptrTable->GetName(), exc);
                throw;
            }
        }

        void CBPtreeDBUtils::CreateFields(bptreedb::ITablePtr ptrTable, IFieldsPtr ptrFields, uint32_t nDefaultStringLength,
                                          SBPtreeDBPendingIndexesPtr ptrPending, bptreedb::ITransactionPtr ptrTran)
        {
            try
            {
                if(!ptrFields.get() || ptrFields->GetFieldCount() == 0)
                    throw CommonLib::CExcBase("Fields aren't set");

                // bptreedb has an index per field: a composite primary key isn't indexed
                int nPrimaryKeys = 0;
                for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
                {
                    if(ptrFields->GetField(i)->GetIsPrimaryKey())
                        ++nPrimaryKeys;
                }

                for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
                {
                    IFieldPtr ptrField = ptrFields->GetField(i);

                    bptreedb::SFieldProp prop(ptrField->GetName(), FieldType2DBType(ptrField->GetType()));
                    prop.m_sAlias = ptrField->GetAliasName();
                    if(prop.m_type == bptreedb::dtString)
                        prop.m_nLength = (ptrField->GetType() == dtGuid || ptrField->GetType() == dtDate) ? 64 : StringLength(ptrField->GetLength(), nDefaultStringLength);
                    prop.m_bNotNull = !ptrField->GetIsNullable();
                    prop.m_defaultValue = AnyToVariant(ptrField->GetDefaultValue());

                    // the unique index of the primary key (a string key is indexed up to 256 bytes). Not for an int64 key:
                    // that is the OID of the source - an ordinary field here (the rows are identified by the bptreedb row
                    // id), a filter by it scans the rows
                    bool bUnique = ptrField->GetIsPrimaryKey() && nPrimaryKeys == 1 && prop.m_type != bptreedb::dtInteger64 &&
                                   prop.m_type != bptreedb::dtBlob && prop.m_type != bptreedb::dtGeometry &&
                                   (prop.m_type != bptreedb::dtString || (prop.m_nLength != 0 && prop.m_nLength <= 256));
                    if(bUnique)
                    {
                        if(ptrPending.get())
                            ptrPending->vecUniqueFields.push_back(prop.m_sName);   // built at the end of the load
                        else
                            prop.m_bUnique = true;
                    }

                    ptrTable->CreateField(prop, ptrTran);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create fields of the table {0}", ptrTable->GetName(), exc);
                throw;
            }
        }

        bptreedb::IFieldSetPtr CBPtreeDBUtils::ToDBFieldSet(IFieldSetPtr ptrFieldSet, IFieldsPtr ptrSourceFields)
        {
            std::vector<std::string> vecFields;
            if(ptrFieldSet.get() && ptrFieldSet->GetCount() != 0)
            {
                for(int i = 0, sz = ptrFieldSet->GetCount(); i < sz; ++i)
                    vecFields.push_back(ptrFieldSet->Get(i));
            }
            else if(ptrSourceFields.get())
            {
                for(int i = 0, sz = ptrSourceFields->GetFieldCount(); i < sz; ++i)
                    vecFields.push_back(ptrSourceFields->GetField(i)->GetName());
            }

            if(vecFields.empty())
                return bptreedb::IFieldSetPtr(); // all fields

            return bptreedb::IFieldSet::Create(vecFields);
        }

        bptreedb::SIndexProp CBPtreeDBUtils::MakeSpatialIndexProp(const SBPtreeDBSpatialIndexParams& params, const CommonLib::bbox& extent)
        {
            bool bExtent = IsNormalBox(extent) && extent.xMin <= extent.xMax && extent.yMin <= extent.yMax;
            if(params.type != bsitZOrder || !bExtent)
                return bptreedb::SIndexProp(bptreedb::itSpatialRTree);   // a Z-order grid needs the extent

            bptreedb::SIndexProp prop(bptreedb::itSpatialZOrder);
            prop.m_extent = extent;
            prop.m_extent.type = CommonLib::bbox_type_normal;
            // a degenerate extent (one point, a horizontal / vertical line) - a grid of one unit
            if(prop.m_extent.xMax - prop.m_extent.xMin <= 0.)
            {
                prop.m_extent.xMin -= 0.5;
                prop.m_extent.xMax += 0.5;
            }
            if(prop.m_extent.yMax - prop.m_extent.yMin <= 0.)
            {
                prop.m_extent.yMin -= 0.5;
                prop.m_extent.yMax += 0.5;
            }
            prop.m_dCellSize = params.dCellSize;
            prop.m_nGridCells = params.nGridCells;
            return prop;
        }

        bool CBPtreeDBUtils::HasSpatialIndex(bptreedb::ITablePtr ptrTable, const std::string& sField)
        {
            if(!ptrTable.get() || sField.empty())
                return false;

            bptreedb::IIndexPtr ptrIndex = ptrTable->GetIndex(sField);
            return ptrIndex.get() && (ptrIndex->GetType() == bptreedb::itSpatialRTree || ptrIndex->GetType() == bptreedb::itSpatialZOrder);
        }

        bptreedb::eSpatialQueryMode CBPtreeDBUtils::SpatialRel2QueryMode(eSpatialRel rel)
        {
            // the bounding boxes: within - the feature is inside the filter, contains - the feature covers it,
            // the other relations need the boxes to intersect (the exact check is for the caller)
            switch(rel)
            {
                case srlWithin:   return bptreedb::sqmInside;
                case srlContains: return bptreedb::sqmOver;
                default:          return bptreedb::sqmIntersect;
            }
        }

        const char* CBPtreeDBUtils::RowIdField()
        {
            return "ROWID";
        }

        bool CBPtreeDBUtils::IsRowIdName(const std::string& sName)
        {
            static const char* pszRowId = "ROWID";
            if(sName.size() != 5)
                return false;
            for(size_t i = 0; i < 5; ++i)
            {
                if(std::toupper((unsigned char)sName[i]) != pszRowId[i])
                    return false;
            }
            return true;
        }

        const char* CBPtreeDBUtils::SpatialTablesMetaName()
        {
            return "GE_SPATIAL_TABLES";
        }

        void CBPtreeDBUtils::CreateSpatialTablesMeta(bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext)
        {
            if(ptrTran->GetTransactionDataType() != bptreedb::eTT_DDL)
                throw CommonLib::CExcBase("{0} doesn't exist, it is created in a DDL transaction", SpatialTablesMetaName());

            bptreedb::ITablePtr ptrMeta = pContext->GetDatabase()->GetSchema()->CreateTable(SpatialTablesMetaName(), ptrTran);
            for(int col = 0; col < mcCount; ++col)
            {
                bptreedb::SFieldProp prop;
                prop.m_sName = MetaColumnName(col);
                switch(col)
                {
                    case mcShapeType:
                        prop.m_type = bptreedb::dtInteger32;
                        break;
                    case mcXMin:
                    case mcYMin:
                    case mcXMax:
                    case mcYMax:
                        prop.m_type = bptreedb::dtDouble;
                        break;
                    case mcSpatialReference:
                        prop.m_type = bptreedb::dtString;   // proj4 / WKT: unlimited
                        prop.m_nLength = 0;
                        break;
                    default:
                        prop.m_type = bptreedb::dtString;
                        prop.m_nLength = MetaNameLength;
                        break;
                }
                prop.m_bNotNull = col == mcTableName;
                prop.m_bUnique = col == mcTableName;
                ptrMeta->CreateField(prop, ptrTran);
            }
        }

        void CBPtreeDBUtils::WriteSpatialTableInfo(const SSpatialTableInfo& info, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext)
        {
            try
            {
                if(!pContext->GetDBTable(SpatialTablesMetaName()).get())
                    CreateSpatialTablesMeta(ptrTran, pContext);

                std::vector<CommonLib::CVariant> values = MetaValues(info);
                bptreedb::IFieldSetPtr ptrFieldSet = MetaFieldSet();

                // update the row of the table, or insert it
                bptreedb::IUpdateCursorPtr ptrUpdate = ptrTran->createUpdateCursor(SpatialTablesMetaName(), ptrFieldSet, MetaColumnName(mcTableName),
                                                                                   CommonLib::CVariant(info.sTableName), bptreedb::OpEqual);
                if(ptrUpdate->Next())
                {
                    for(int col = mcViewName; col < mcCount; ++col)
                        ptrUpdate->UpdateValue(col, values[col]);
                    return;
                }
                ptrUpdate.reset();

                bptreedb::IInsertCursorPtr ptrInsert = ptrTran->createInsertCursor(SpatialTablesMetaName(), ptrFieldSet);
                for(int col = 0; col < mcCount; ++col)
                    ptrInsert->BindValue(col, values[col]);
                ptrInsert->Next();
                ptrInsert->Finish();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to write spatial table info, table: {0}", info.sTableName, exc);
                throw;
            }
        }

        bool CBPtreeDBUtils::ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext)
        {
            try
            {
                if(!pContext->GetDBTable(SpatialTablesMetaName()).get())
                    return false;

                bptreedb::ISelectCursorPtr ptrCursor = ptrTran->executeSelectQuery(SpatialTablesMetaName(), MetaFieldSet(), MetaColumnName(mcTableName),
                                                                                   CommonLib::CVariant(sTableName), bptreedb::OpEqual);
                if(!ptrCursor->Next())
                    return false;

                info.sTableName = ReadTextOrEmpty(ptrCursor, mcTableName);
                info.sViewName = ReadTextOrEmpty(ptrCursor, mcViewName);
                info.sShapeField = ReadTextOrEmpty(ptrCursor, mcShapeField);
                info.sOIDField = ReadTextOrEmpty(ptrCursor, mcOIDField);
                info.sSpatialIndex = ReadTextOrEmpty(ptrCursor, mcSpatialIndex);
                info.shapeType = ptrCursor->ColumnIsNull(mcShapeType) ? CommonLib::shape_type_null : (CommonLib::eShapeType)ptrCursor->ReadInt32(mcShapeType);
                info.extent = CommonLib::bbox();
                bool bNullExtent = ptrCursor->ColumnIsNull(mcXMin) || ptrCursor->ColumnIsNull(mcYMin) ||
                                   ptrCursor->ColumnIsNull(mcXMax) || ptrCursor->ColumnIsNull(mcYMax);
                if(!bNullExtent)
                {
                    double xMin = ptrCursor->ReadDouble(mcXMin);
                    double yMin = ptrCursor->ReadDouble(mcYMin);
                    double xMax = ptrCursor->ReadDouble(mcXMax);
                    double yMax = ptrCursor->ReadDouble(mcYMax);
                    if(xMin <= xMax && yMin <= yMax)
                    {
                        info.extent.type = CommonLib::bbox_type_normal;
                        info.extent.xMin = xMin;
                        info.extent.yMin = yMin;
                        info.extent.xMax = xMax;
                        info.extent.yMax = yMax;
                    }
                }
                info.sSpatialReference = ReadTextOrEmpty(ptrCursor, mcSpatialReference);
                return true;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read spatial table info, table: {0}", sTableName, exc);
                throw;
            }
        }

        std::vector<std::string> CBPtreeDBUtils::ReadSpatialTableNames(bptreedb::ITransactionPtr ptrTran, CBPtreeDBContext* pContext)
        {
            try
            {
                std::vector<std::string> vecNames;
                if(!pContext->GetDBTable(SpatialTablesMetaName()).get())
                    return vecNames;

                bptreedb::ISelectCursorPtr ptrCursor = ptrTran->executeSelectQuery(SpatialTablesMetaName(),
                                                                                   bptreedb::IFieldSet::Create(std::vector<std::string>{ MetaColumnName(mcTableName) }));
                while(ptrCursor->Next())
                    vecNames.push_back(ptrCursor->ReadText(0));

                std::sort(vecNames.begin(), vecNames.end());
                return vecNames;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to read spatial tables", exc);
                throw;
            }
        }

        bool CBPtreeDBUtils::ReadSpatialTableInfo(const std::string& sTableName, SSpatialTableInfo& info, CBPtreeDBContext* pContext)
        {
            CReadTransactionScope tran(pContext);
            return ReadSpatialTableInfo(sTableName, info, tran.Get(), pContext);
        }

        std::vector<std::string> CBPtreeDBUtils::ReadSpatialTableNames(CBPtreeDBContext* pContext)
        {
            CReadTransactionScope tran(pContext);
            return ReadSpatialTableNames(tran.Get(), pContext);
        }

        std::string CBPtreeDBUtils::DoubleToSQL(double dValue)
        {
            std::ostringstream stream;
            stream.imbue(std::locale::classic());
            stream.precision(17);
            stream << dValue;
            return stream.str();
        }

    }
}
