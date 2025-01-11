#pragma once

#include "DataSetBase.h"
#include "Fields.h"

namespace GraphEngine {
    namespace GeoDatabase {
        template< class I>
        class ITableBase : public IDataSetBase<I>
        {
        public:
            typedef IDataSetBase<I> TBase;

            ITableBase(eDatasetType datasetType,  std::string sDatasetName,  std::string sDatasetViewName) : TBase(
                     datasetType,   sDatasetName,sDatasetViewName)
            {
                this->m_pFields = std::make_shared<CFields>();
            }

            virtual ~ITableBase()
            {

            }

            virtual void AddField(IFieldPtr pField)
            {
                m_pFields->AddField(pField);
            }

            virtual void  DeleteField(const std::string& fieldName)
            {
                int nIndex = m_pFields->FindField(fieldName);
                if(nIndex != -1)
                    m_pFields->RemoveField(nIndex);
            }

            virtual IFieldsPtr	GetFields() const
            {
                return m_pFields;
            }

            virtual void SetFields(IFieldsPtr ptrFields)
            {
                m_pFields = ptrFields;
            }
        protected:

        protected:
            mutable IFieldsPtr         m_pFields;
        };
    }
}