#pragma once
#include "GeoDatabase.h"
#include "../CommonLib/str/StrUtils.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        template< class I>
        class IRowBase : public I{
        public:
            IRowBase(IFieldsPtr  ptrFields) : m_ptrFields(ptrFields)
            {}

            virtual ~IRowBase()
            {}

            virtual int32_t  ColumnCount() const
            {
                return m_ptrFields->GetFieldCount();
            }

            virtual std::string ColumnName(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row: failed to get column name out of range, index: {0}", col);

                return m_ptrFields->GetField(col)->GetName();
            }

            virtual bool ColumnIsNull(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row: failed to get is column null,  out of range: index: {0}", col);

                return m_ptrFields->GetField(col)->GetIsNullable();
            }

            virtual eDataTypes GetColumnType(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row: failed to get   column type, out of range: index: {0}", col);

                return m_ptrFields->GetField(col)->GetType();
            }

            virtual int32_t GetColumnBytes(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row failed to get column lengh, out of range: index: {0}", col);

                return m_ptrFields->GetField(col)->GetLength();
            }

        protected:
            bool IsIntegerType(int col) const
            {
                eDataTypes type =  GetColumnType(col);

                return type == dtInteger8 ||
                       type ==  dtInteger16  ||
                       type ==  dtInteger32  ||
                       type ==  dtInteger64 ||
                       type ==  dtUInteger8 ||
                       type ==  dtUInteger16 ||
                       type ==  dtUInteger32 ||
                       type ==  dtUInteger64;
            }

            void CheckIfFiledIsInteger(int col) const
            {
                if(!IsIntegerType(col))
                    throw CommonLib::CExcBase("Row field: {0} isn't integer type", col);
            }

            void CheckIfFiledIsStringType(int col) const
            {
                eDataTypes type =  GetColumnType(col);
                if(type != dtString)
                    throw CommonLib::CExcBase("Row field: {0} isn't string type", col);
            }

            void CheckIfFiledIsDouble(int col) const
            {
                eDataTypes type =  GetColumnType(col);
                if(type != dtFloat && type != dtDouble)
                    throw CommonLib::CExcBase("Row field: {0} isn't double type", col);
            }

        protected:
            CommonLib::IGeoShapePtr  m_ptrGeoShapeCache;
            IFieldsPtr  m_ptrFields;
        };

    }
}