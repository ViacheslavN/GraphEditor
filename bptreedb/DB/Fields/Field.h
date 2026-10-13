#pragma once

// A field of a table (metadata stored in the schema) and the snapshot of the fields of a table (IFields).

#include "../DBCommon.h"
#include "../DBTrees.h"
#include "Index.h"

namespace bptreedb
{
	namespace db
	{
		class CField : public IField
		{
		public:
			CField() {}
			CField(uint32_t nId, const SFieldProp& prop, uint32_t nPageSize);
			virtual ~CField() {}

			// IField
			virtual eDataTypes GetType() const { return m_type; }
			virtual const std::string& GetName() const { return m_sName; }
			virtual const std::string& GetAlias() const { return m_sAlias; }
			virtual uint32_t GetLength() const { return m_nLength; }
			virtual bool GetIsNotNull() const { return m_bNotNull; }
			virtual const CommonLib::CVariant& GetDefaultValue() const { return m_defaultValue; }
			virtual bool GetIsUnique() const { return m_bUnique; }
			virtual IIndexPtr GetIndex() const { return m_ptrIndex; }

			uint32_t GetId() const { return m_nId; }
			EValueStorage GetValueStorage() const { return m_valueStorage; }
			int64_t GetValuePage() const { return m_nValuePage; }
			void SetValuePage(int64_t nPage) { m_nValuePage = nPage; }
			CIndexPtr GetIndexImpl() const { return m_ptrIndex; }
			void SetIndex(CIndexPtr ptrIndex) { m_ptrIndex = ptrIndex; }

			// a value converted to the type of the field (throws if it doesn't fit)
			CommonLib::CVariant Convert(const CommonLib::CVariant& value) const;

			void Write(CommonLib::IWriteStream* pStream) const;
			static CFieldPtr Read(CommonLib::IReadStream* pStream);

		private:
			uint32_t m_nId{ 0 };
			std::string m_sName;
			std::string m_sAlias;
			eDataTypes m_type{ dtUnknown };
			uint32_t m_nLength{ 0 };
			bool m_bNotNull{ false };
			bool m_bUnique{ false };
			CommonLib::CVariant m_defaultValue;
			EValueStorage m_valueStorage{ evsNumber };
			int64_t m_nValuePage{ -1 };
			CIndexPtr m_ptrIndex;
		};

		class CFields : public IFields
		{
		public:
			CFields(const std::vector<CFieldPtr>& fields) : m_fields(fields) {}
			virtual ~CFields() {}

			virtual int32_t GetFieldCount() const { return (int32_t)m_fields.size(); }
			virtual IFieldPtr GetField(int32_t nIndex) const;
			virtual IFieldPtr GetField(const std::string& sName) const;
			virtual int32_t FindField(const std::string& sName) const;

		private:
			std::vector<CFieldPtr> m_fields;
		};
	}
}
