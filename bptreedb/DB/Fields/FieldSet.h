#pragma once

// Names of the fields a cursor works with (IFieldSet).

#include "../DBCommon.h"

namespace bptreedb
{
	namespace db
	{
		class CFieldSet : public IFieldSet
		{
		public:
			CFieldSet() {}
			CFieldSet(const std::vector<std::string>& fields);
			virtual ~CFieldSet() {}

			virtual int32_t GetCount() const { return (int32_t)m_fields.size(); }
			virtual const std::string& Get(int32_t nIndex) const;
			virtual int32_t Find(const std::string& sField) const;
			virtual void Add(const std::string& sField);
			virtual void Remove(const std::string& sField);
			virtual void Clear() { m_fields.clear(); }

		private:
			std::vector<std::string> m_fields;
		};
	}
}
