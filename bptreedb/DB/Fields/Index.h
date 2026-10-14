#pragma once

// An index of a field (one per field): unique / regular (value, row id) tree or a spatial index of a geometry field.

#include "../DBCommon.h"
#include "../DBTrees.h"

namespace bptreedb
{
	namespace db
	{
		typedef std::shared_ptr<class CField> CFieldPtr;
		typedef std::shared_ptr<class CIndex> CIndexPtr;

		class CIndex : public IIndex
		{
		public:
			CIndex() {}
			CIndex(eIndexTypes type, CFieldPtr ptrField);
			virtual ~CIndex() {}

			// IIndex
			virtual eIndexTypes GetType() const { return m_type; }
			virtual IFieldPtr GetField() const;

			bool IsSpatial() const { return m_type == itSpatialRTree || m_type == itSpatialZOrder; }
			bool IsUnique() const { return m_type == itUnique; }
			int64_t GetTreePage() const { return m_nTreePage; }
			void SetTreePage(int64_t nPage) { m_nTreePage = nPage; }
			const SSpatialIndexParams& GetSpatialParams() const { return m_spatial; }

			// the format of the index tree: 0 - the first one (the files of older versions), FormatStringCut - the
			// string keys by TStringMultiKeyCutEncoder. Stored in the high 16 bits of the type.
			static constexpr uint32_t FormatStringCut = 1;
			static uint32_t NewFormat(eDataTypes type) { return type == dtString ? FormatStringCut : 0; }
			uint32_t GetFormat() const { return m_nFormat; }
			void SetFormat(uint32_t nFormat) { m_nFormat = nFormat; }
			void SetSpatialParams(const SSpatialIndexParams& params) { m_spatial = params; }

			void Write(CommonLib::IWriteStream* pStream) const;
			void Read(CommonLib::IReadStream* pStream, CFieldPtr ptrField);

		private:
			eIndexTypes m_type{ itUnknown };
			std::weak_ptr<CField> m_ptrField;      // the field owns the index
			int64_t m_nTreePage{ -1 };
			uint32_t m_nFormat{ 0 };
			SSpatialIndexParams m_spatial;
		};
	}
}
