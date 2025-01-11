#pragma once
#include "GeoDatabase.h"


namespace GraphEngine {
    namespace GeoDatabase {


        class  CQueryFilter : public ISpatialFilter
        {
        public:
            CQueryFilter(const std::string& sWhere = "", IFieldSetPtr fieldSet = nullptr);
            virtual ~CQueryFilter();

        private:
            CQueryFilter(const CQueryFilter&);
            CQueryFilter& operator=(const CQueryFilter&);

        public:
            // IQueryFilter
            virtual IFieldSetPtr                      GetFieldSet() const;
            virtual void                              SetFieldSet(IFieldSetPtr fieldSet);
            virtual Geometry::ISpatialReferencePtr    GetOutputSpatialReference() const;
            virtual void                              SetOutputSpatialReference(Geometry::ISpatialReferencePtr spatRef);
            virtual const std::string& 		  GetWhereClause() const;
            virtual void                              SetWhereClause(const std::string& where);
            virtual const std::vector<IJoinPtr>       GetJoins() const;
            virtual void                              SetJoins(const std::vector<IJoinPtr> & vecJoins);
            virtual void                          SetTablePrefix(const std::string& tablePrefix);
            virtual const std::string&  GetTablePrefix() const;


            // ISpatialFilter
            virtual const std::string&     GetShapeField() const;
            virtual void                    SetShapeField(const std::string& name);
            virtual CommonLib::IGeoShapePtr GetShape() const;
            virtual void                    SetShape(CommonLib::IGeoShapePtr ptrGeoShape);
            virtual eSpatialRel             GetSpatialRel() const;
            virtual void                    SetSpatialRel(eSpatialRel rel);
            virtual double                  GetPrecision() const;
            virtual void                    SetPrecision(double precision);
            virtual CommonLib::bbox			GetBB() const;
            virtual void                    SetBB(const  CommonLib::bbox& bbox );
        private:
            std::string                        m_sWhereClause;
            mutable IFieldSetPtr               m_pFieldSet;
            Geometry::ISpatialReferencePtr     m_pOutputSpatialRef;
            std::string                        m_sShapeField;
            CommonLib::IGeoShapePtr            m_pShape;
            eSpatialRel                        m_SpatialRel;
            double                             m_dPecision;
            CommonLib::bbox					   m_bbox;
            std::vector<IJoinPtr>              m_vecJoins;
            std::string                        m_sTablePrefix;
        };

    }
}