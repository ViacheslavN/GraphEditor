#include "../SpatialTableBase.h"
#include "ShapefileUtils.h"
#include "ShapeFile.h"
#include "ShapeDBFile.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        class CShapefileSpatialTable : public ISpatialTableBase<CShapefileSpatialTable>
        {
        public:
            typedef  ISpatialTableBase<CShapefileSpatialTable> TBase;

            CShapefileSpatialTable(const std::string& sPath, const std::string& sName, const std::string& sViewName);
            ~CShapefileSpatialTable();


            virtual IRowPtr		GetRow(int64_t id);
            virtual ICursorPtr	Search(IQueryFilterPtr filter, bool recycling);


            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:

            ISpatialFieldPtr m_pShapeField;
            IFieldPtr m_pOIDField;
            std::string m_sPath;

            CShapeFilePtr m_ptrShp;
            CShapeDBFilePtr m_ptrDBfile;

        };



    }
    }