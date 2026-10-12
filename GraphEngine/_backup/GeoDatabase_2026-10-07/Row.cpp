#include "GeoDatabase.h"
#include "Row.h"


namespace GraphEngine {
    namespace GeoDatabase {

        CRow::CRow(IFieldsPtr ptrFields) : IRowBase<IRow>(ptrFields)
        {
            for (int i = 0; i < m_ptrFields->GetFieldCount(); ++i){
                m_Values.push_back(std::make_shared<CommonLib::CVariant>());
            }

        }

        CRow::~CRow()
        {

        }

        bool CRow::ColumnIsNull(int32_t col) const
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to check if column is null. out of range, index: {0}", col);

            return m_Values[col]->IsNull();
        }


        CommonLib::CVariantPtr CRow::GetValue(int32_t col) const
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to get value, out of range, index: {0}", col);

            return m_Values[col];
        }


        void CRow::SetNull(int32_t col)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set is null, out of range, index: {0}", col);

            m_Values[col]->SetType<CommonLib::CEmptyVariant>();
        }

        void CRow::SetInt8(int32_t col, int8_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set int8, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetUInt8(int32_t col, uint8_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set uint8, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetInt16(int32_t col, int16_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set int16, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetUInt16(int32_t col, uint16_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set uint16, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetInt32(int32_t col, int32_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set int32, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetUInt32(int32_t col, uint32_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set uint32, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetInt64(int32_t col, int64_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set int64, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetUInt64(int32_t col, uint64_t val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set uint64, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetFloat(int32_t col, float val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set float, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetDouble(int32_t col, double val)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set double, out of range, index: {0}", col);

            m_Values[col]->SetVal(val);
        }

        void CRow::SetText(int32_t col, const std::string& text)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set str, out of range, index: {0}", col);

            m_Values[col]->SetVal(text);
        }

        void CRow::SetTextW(int32_t col, const std::wstring& text)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set wstr, out of range, index: {0}", col);

            m_Values[col]->SetVal(text);
        }

        void CRow::SetBlob(int32_t col, const byte_t *pBuf, int32_t size)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set blob, out of range, index: {0}", col);

            if(m_Values[col]->IsType<CommonLib::Data::CBlobPtr>())
            {
                CommonLib::Data::CBlobPtr ptrBlob = m_Values[col]->Get<CommonLib::Data::CBlobPtr>();
                ptrBlob->Copy(pBuf, size);
            }
            else
            {
                CommonLib::Data::CBlobPtr ptrBlob = std::make_shared< CommonLib::Data::CBlob>();
                ptrBlob->Copy(pBuf, size);
                m_Values[col]->SetVal(ptrBlob);
            }

        }

        void CRow::SetShape(int32_t col, CommonLib::IGeoShapePtr ptrShape)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set shape, out of range, index: {0}", col);

            if(m_Values[col]->IsType<CommonLib::Data::CBlobPtr>())
            {
                CommonLib::IGeoShapePtr ptrValShape = m_Values[col]->Get<CommonLib::IGeoShapePtr>();
                ptrValShape->Import(ptrShape->Data(), ptrShape->Size());
            }
            else
            {
                CommonLib::IGeoShapePtr ptrValShape =  ptrShape->Clone();
                m_Values[col]->SetVal(ptrValShape);
            }

        }

        void CRow::SetGuid(int32_t col, const CommonLib::CGuid& guid)
        {
            if(col >= ColumnCount() )
                throw CommonLib::CExcBase("Row: failed to set guid, out of range, index: {0}", col);

            m_Values[col]->SetVal(guid);
        }

    }
}