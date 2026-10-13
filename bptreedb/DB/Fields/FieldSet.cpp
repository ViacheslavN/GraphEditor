#include "FieldSet.h"

namespace bptreedb
{
	IFieldSetPtr IFieldSet::Create()
	{
		return std::make_shared<db::CFieldSet>();
	}

	IFieldSetPtr IFieldSet::Create(const std::vector<std::string>& fields)
	{
		return std::make_shared<db::CFieldSet>(fields);
	}

	namespace db
	{
		CFieldSet::CFieldSet(const std::vector<std::string>& fields)
		{
			for (const std::string& s : fields)
				Add(s);
		}

		const std::string& CFieldSet::Get(int32_t nIndex) const
		{
			if (nIndex < 0 || nIndex >= (int32_t)m_fields.size())
				throw CommonLib::CExcBase("field set index {0} is out of range, fields {1}", nIndex, (uint32_t)m_fields.size());
			return m_fields[nIndex];
		}

		int32_t CFieldSet::Find(const std::string& sField) const
		{
			for (size_t i = 0; i < m_fields.size(); ++i)
				if (m_fields[i] == sField)
					return (int32_t)i;
			return -1;
		}

		void CFieldSet::Add(const std::string& sField)
		{
			if (Find(sField) >= 0)
				throw CommonLib::CExcBase("field '{0}' is in the field set already", sField);
			m_fields.push_back(sField);
		}

		void CFieldSet::Remove(const std::string& sField)
		{
			int32_t n = Find(sField);
			if (n >= 0)
				m_fields.erase(m_fields.begin() + n);
		}
	}
}
