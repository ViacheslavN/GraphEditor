#include "../CursorBase.h"
#include "ShapeFile.h"
#include "ShapeDBFile.h"
#include "SHPObject.h"
#include "ShapeTree.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        class CShapefileRowCursor : public ICursorBase<ISelectCursor>
        {
        public:

            typedef ICursorBase<ISelectCursor> TBase;
            CShapefileRowCursor(IQueryFilterPtr filter, CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pSourceFields,  Geometry::ISpatialReferencePtr spatRefSource);
            virtual ~CShapefileRowCursor();

        public:
            // ICursor
            virtual bool Next();

            virtual bool ColumnIsNull(int32_t col) const;

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
            virtual CommonLib::CGuid  ReadGuid(int32_t col) const;

            virtual IRowPtr CreateRow() const;
            virtual void FillRow(IRowPtr ptrRow) const;

        protected:

            bool EOC();
            bool AlterShape(CommonLib::IGeoShapePtr ptrShape) const;
        protected:


            int32_t m_nCurrentRowID;
            CShapeFilePtr m_ptrShapeFile;
            CShapeDBFilePtr m_ptrDBFile;
            CShapeTreePtr  m_ptrShapeTree;
            CSHPObjectPtr     m_pCacheObject;
            Geometry::ISpatialReferencePtr m_ptrSpatialReference;
            CommonLib::IGeoShapePtr  m_ptrGeoShapeCache;
            std::vector<int64_t> m_vecOids;
        };
    }
}