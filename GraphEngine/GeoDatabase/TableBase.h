#pragma once

#include "DataSetBase.h"

namespace GraphEngine {
    namespace GeoDatabase {
        template< class I>
        class ITableBase : public IDataSetBase<I>
        {
        public:
            typedef IDataSetBase<I> TBase;

            ITableBase() : TBase()
            {
                this->m_DatasetType = dtTypeTable;
                this->m_pFields = std::make_shared<CFields>();
                this->m_bHashOID = false;
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
                m_pFields = pFields;
            }

            void SetOIDFieldName(const std::string& sOIDFieldName)
            {
                m_sOIDFieldName = sOIDFieldName;
            }

            virtual bool   HasOIDField() const
            {
                return m_bHashOID;
            }

            virtual void   SetHasOIDField(bool bFlag)
            {
                m_bHashOID = bFlag;
            }

            virtual const std::string&	 GetOIDFieldName() const
            {
                return m_sOIDFieldName;
            }

        protected:
            mutable IFieldsPtr         m_pFields;
            mutable std::string        m_sOIDFieldName;
            bool					   m_bHashOID;
        };
    }
}