#include "../CursorBase.h"
#include "ShapeFile.h"
#include "ShapeDBFile.h"
#include "SHPObject.h"
#include "ShapeTree.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        class CShapefileRowCursor : ICursorBase<ICursor>
        {
        public:

            typedef ICursorBase<ICursor> TBase;
            CShapefileRowCursor(IQueryFilterPtr filter, bool recycling, CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pSourceFields,  Geometry::ISpatialReferencePtr spatRefSource);
            virtual ~CShapefileRowCursor();

        public:
            // IRowCursor
            virtual bool NextRow(IRowPtr* row);
        protected:


            bool NextRowEx(IRowPtr row, IRowPtr rowCache);
            bool EOC();
            bool FillRow(IRowPtr row);
            void SimpleNext();
            bool AlterShape(CommonLib::IGeoShapePtr ptrShape) const;
        protected:

            int m_nCurrentRowID;
            CShapeFilePtr m_ptrShapeFile;
            CShapeDBFilePtr m_ptrDBFile;
            CShapeTreePtr  m_ptrShapeTree;
            CSHPObjectPtr     m_pCacheObject;
            CommonLib::IGeoShapePtr   m_pCacheShape;
            int m_nRecordCount;
            bool m_bInvalidCursor;
            std::vector<int> m_vecRows;


        };
    }
}