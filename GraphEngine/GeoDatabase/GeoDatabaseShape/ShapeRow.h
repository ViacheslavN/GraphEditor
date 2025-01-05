#pragma once
#include "../GeoDatabase.h"
#include "../../ThirdParty/ShapeLib/shapefil.h"
#include "ShapeDBFile.h"
#include "ShapeFile.h"
#include "../BaseRow.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        class CShapeRow : public IRowBase<IRow>
        {
        public:

            typedef IRowBase<IRow> TBase;

            CShapeRow(CShapeDBFilePtr ptrDBFile, CShapeFilePtr ptrShapeFile, IFieldsPtr  pFields);
            virtual ~CShapeRow();

            void SetRow(int rowId);
            void SetShape(CommonLib::IGeoShapePtr ptrShape);

            virtual int64_t  GetRowId() const;
            virtual int8_t ReadInt8(int32_t col) const;
            virtual uint8_t ReadUInt8(int32_t col) const ;
            virtual int16_t ReadInt16(int32_t col) const ;
            virtual uint16_t ReadUInt16(int32_t col) const ;
            virtual int32_t ReadInt32(int32_t col) const ;
            virtual uint32_t ReadUInt32(int32_t col) const ;
            virtual int64_t ReadInt64(int32_t col) const;
            virtual uint64_t ReadUInt64(int32_t col) const;
            virtual float ReadFloat(int32_t col) const;
            virtual double ReadDouble(int32_t col) const;
            virtual void ReadText(int32_t col, std::string& text) const;
            virtual std::string ReadText(int32_t col) const;
            virtual void ReadTextW(int32_t col, std::wstring& text) const;
            virtual std::wstring ReadTextW(int32_t col) const;
            virtual void ReadBlob(int col, byte_t **pBuf, int32_t& size) const;
            virtual CommonLib::IGeoShapePtr ReadShape(int32_t col) const ;

            virtual void BindInt8(int32_t col, int8_t val);
            virtual void BindUInt8(int32_t col, uint8_t val);
            virtual void BindInt16(int32_t col, int16_t val) ;
            virtual void BindUInt16(int32_t col, uint16_t val);
            virtual void BindInt32(int32_t col, int32_t val);
            virtual void BindUInt32(int32_t col, uint32_t val);
            virtual void BindInt64(int32_t col, int64_t val);
            virtual void BindUInt64(int32_t col, uint64_t val);
            virtual void BindFloat(int32_t col, float val) ;
            virtual void BindDouble(int32_t col, double val);
            virtual void BindText(int32_t col, const std::string& text, bool copy);
            virtual void BindTextW(int32_t col, const std::wstring& text, bool copy);
            virtual void BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy);
            virtual void BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape) ;

        private:
            CShapeDBFilePtr m_ptrDBFile;
            CShapeFilePtr m_ptrShapeFile;
            CommonLib::IGeoShapePtr  m_ptrGeoShapeCache;
            int m_rowId;


        };
    }
}