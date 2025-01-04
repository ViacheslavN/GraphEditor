#pragma once

#include "GeoDatabase.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        template<class I>
        class IInsertCursorBase : public I
        {
        public:

            IInsertCursorBase(ITablePtr pTable, IFieldSetPtr pFileds)
            {

                m_pTable = pTable;
                m_pFieldSet = pFileds;
                m_pSourceFields = m_pTable->GetFields();

            }

            virtual ~IInsertCursorBase(){}
            virtual IFieldSetPtr GetFieldSet() const
            {
                return	m_pFieldSet;
            }
            virtual IFieldsPtr   GetSourceFields() const
            {
                return m_pSourceFields;
            }


        protected:
            IFieldSetPtr  m_pFieldSet;
            IFieldsPtr m_pSourceFields;
            ITablePtr  m_pTable;

        };
    }
}