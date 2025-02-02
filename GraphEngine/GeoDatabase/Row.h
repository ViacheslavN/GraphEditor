#pragma once
#include "GeoDatabase.h"


namespace GraphEngine {
    namespace GeoDatabase {

        class CRow : public IRow
        {
        public:
            virtual int32_t  ColumnCount() const ;
            virtual std::string ColumnName(int32_t col) const ;
            virtual bool ColumnIsNull(int32_t col) const;
            virtual eDataTypes GetColumnType(int32_t col) const;

            virtual std::any GetValue(int32_t col) const;
            virtual CommonLib::Data::CBlobPtr GetBlobValue(int32_t col) const;
            virtual CommonLib::IGeoShapePtr GetShapeValue(int32_t col) const;

            virtual void SetInt8(int32_t col, int8_t val);
            virtual void SetUInt8(int32_t col, uint8_t val);
            virtual void SetInt16(int32_t col, int16_t val) ;
            virtual void SetUInt16(int32_t col, uint16_t val);
            virtual void BSetInt32(int32_t col, int32_t val);
            virtual void SetUInt32(int32_t col, uint32_t val);
            virtual void SetInt64(int32_t col, int64_t val);
            virtual void SetUInt64(int32_t col, uint64_t val);
            virtual void SetFloat(int32_t col, float val) ;
            virtual void SetDouble(int32_t col, double val);
            virtual void SetText(int32_t col, const std::string& text) ;
            virtual void SetTextW(int32_t col, const std::wstring& text);
            virtual void SetBlob(int32_t col, const byte_t *pBuf, int32_t size);
            virtual void SetShape(int32_t col, CommonLib::IGeoShapePtr ptrShape);
            virtual void SetGuid(int32_t col, const CommonLib::CGuid& guid);

        private:
            struct SValue
            {
                std::any m_value;
                CommonLib::IGeoShapePtr m_ptrShape;
                CommonLib::Data::CBlobPtr m_ptrBlob;
            };

            std::vector<SValue> m_Values;

        };
    }
    }
