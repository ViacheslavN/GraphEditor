#include "../TableBase.h"
#include "ShapefileUtils.h"
#include "ShapeFile.h"
#include "ShapeDBFile.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        class CShapefileSpatialTable : public ITableBase<ITable>
        {
        public:
            typedef  ITableBase<ITable> TBase;

            CShapefileSpatialTable(const std::string& sPath, const std::string& sName, const std::string& sViewName); //open
            CShapefileSpatialTable(const std::string& sPath, const std::string& name,  const std::string& viewName, IFieldsPtr ptrFields); //create
            ~CShapefileSpatialTable();


            virtual IRowPtr		GetRow(int64_t id);
            virtual ISelectCursorPtr	Search(IQueryFilterPtr filter);
            virtual ISelectCursorPtr	Select(const std::string& sqlSelectQuery);


            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            void LoadShapeFile( bool  write);
            void CreateShapeFile(IFieldsPtr ptrFields);
        private:

            ISpatialFieldPtr m_pShapeField;
            IFieldPtr m_pOIDField;
            std::string m_sPath;

            CShapeFilePtr m_ptrShp;
            CShapeDBFilePtr m_ptrDBfile;

        };



    }
    }