#include "Fields.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        CFields::CFields()
        {}

        CFields::~CFields()
        {}


        // IFields
        int CFields::GetFieldCount() const
        {
            return (int)m_vecFields.size();
        }

        void CFields::SetFieldCount(int count)
        {
            m_vecFields.resize(count);
            m_mapFieldIndex.clear();
        }

        IFieldPtr CFields::GetField(int index) const
        {
            return m_vecFields[index];
        }

        IFieldPtr  CFields::GetField(const std::string& name) const
        {
            int nIndex = FindField(name);
            if(nIndex == -1)
                return IFieldPtr();

            return GetField(nIndex);
        }

        void CFields::SetField(int index, IFieldPtr field)
        {
            m_vecFields[index] = field;
            m_mapFieldIndex.clear();
        }

        void CFields::AddField(IFieldPtr field)
        {
            m_vecFields.push_back(IFieldPtr(field));
            m_mapFieldIndex.clear();
        }

        void CFields::RemoveField(int index)
        {
            m_vecFields.erase(m_vecFields.begin() + index);
            m_mapFieldIndex.clear();
        }

        int CFields::FindField(const std::string& sName) const
        {
            if(!m_mapFieldIndex.size())
                RebuildFieldIndexMap();

            TMapFieldIndex::iterator it = m_mapFieldIndex.find(sName);
            if(it == m_mapFieldIndex.end())
            {
                size_t n = sName.find('.');
                if(n != std::string::npos)
                    it = m_mapFieldIndex.find(CommonLib::StringUtils::Mid(sName, (n + 1)));

                if(it == m_mapFieldIndex.end())
                    return -1;
            }
            return (*it).second;
        }

        bool CFields::FieldExists(const std::string& sName) const
        {
            if(!m_mapFieldIndex.size())
                RebuildFieldIndexMap();

            return m_mapFieldIndex.find(sName) != m_mapFieldIndex.end();
        }


        void CFields::RebuildFieldIndexMap() const
        {
            m_mapFieldIndex.clear();

            for(size_t i = 0; i < m_vecFields.size(); ++i)
            {
                IFieldPtr field = m_vecFields[i];
                if(!field.get())
                    continue;
                m_mapFieldIndex.insert(TMapFieldIndex::value_type(field->GetName(), (int)i));
            }
        }
        void CFields::Clear()
        {
            m_vecFields.clear();
            m_mapFieldIndex.clear();
        }

        IFieldsPtr	CFields::Clone() const
        {
            IFieldsPtr pFields = std::make_shared<CFields>();
            for(size_t i = 0; i < m_vecFields.size(); ++i)
            {
                IFieldPtr pField = m_vecFields[i];
                pFields->AddField(pField->Clone());
            }
            return pFields;
        }

    }
}