#pragma once
#include "GeoDatabase.h"
#include "BaseRow.h"

namespace GraphEngine {
    namespace GeoDatabase {

        class CRow : public IRowBase<IRow>
        {
        public:
            CRow(IFieldsPtr ptrFields);
            virtual ~CRow();

            virtual bool ColumnIsNull(int32_t col) const;

            virtual CommonLib::CVariantPtr GetValue(int32_t col) const;

            virtual void SetNull(int32_t col);
            virtual void SetInt8(int32_t col, int8_t val);
            virtual void SetUInt8(int32_t col, uint8_t val);
            virtual void SetInt16(int32_t col, int16_t val) ;
            virtual void SetUInt16(int32_t col, uint16_t val);
            virtual void SetInt32(int32_t col, int32_t val);
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

            std::vector<CommonLib::CVariantPtr> m_Values;


        };
    }
    }
