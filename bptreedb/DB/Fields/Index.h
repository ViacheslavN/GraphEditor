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
			void SetSpatialParams(const SSpatialIndexParams& params) { m_spatial = params; }

			void Write(CommonLib::IWriteStream* pStream) const;
			void Read(CommonLib::IReadStream* pStream, CFieldPtr ptrField);

		private:
			eIndexTypes m_type{ itUnknown };
			std::weak_ptr<CField> m_ptrField;      // the field owns the index
			int64_t m_nTreePage{ -1 };
			SSpatialIndexParams m_spatial;
		};
	}
}
