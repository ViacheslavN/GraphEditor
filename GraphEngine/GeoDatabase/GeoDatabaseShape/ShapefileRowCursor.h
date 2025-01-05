#include "../CursorBase.h"
#include "ShapeFile.h"
#include "ShapeDBFile.h"
#include "SHPObject.h"
#include "ShapeTree.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        class CShapefileRowCursor : public ICursorBase<ICursor>
        {
        public:

            typedef ICursorBase<ICursor> TBase;
            CShapefileRowCursor(IQueryFilterPtr filter, CShapeFilePtr ptrShapeFile, CShapeDBFilePtr ptrDBFile,  IFieldsPtr pSourceFields,  Geometry::ISpatialReferencePtr spatRefSource);
            virtual ~CShapefileRowCursor();

        public:
            // IRowCursor
            virtual bool NextRow();
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