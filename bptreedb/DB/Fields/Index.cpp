#include "Index.h"
#include "Field.h"

namespace bptreedb
{
	namespace db
	{
		CIndex::CIndex(eIndexTypes type, CFieldPtr ptrField) : m_type(type), m_ptrField(ptrField)
		{
		}

		IFieldPtr CIndex::GetField() const
		{
			return m_ptrField.lock();
		}

		void CIndex::Write(CommonLib::IWriteStream* pStream) const
		{
			pStream->Write((uint32_t)m_type | (m_nFormat << 16));
			pStream->Write(m_nTreePage);
			if (IsSpatial())
			{
				pStream->Write(m_spatial.m_extent.xMin);
				pStream->Write(m_spatial.m_extent.yMin);
				pStream->Write(m_spatial.m_extent.xMax);
				pStream->Write(m_spatial.m_extent.yMax);
				pStream->Write(m_spatial.m_dCellSize);
			}
		}

		void CIndex::Read(CommonLib::IReadStream* pStream, CFieldPtr ptrField)
		{
			m_ptrField = ptrField;
			uint32_t nType = pStream->ReadIntu32();
			m_type = (eIndexTypes)(nType & 0xFFFF);
			m_nFormat = nType >> 16;
			m_nTreePage = pStream->ReadInt64();
			m_spatial.m_type = m_type;
			if (IsSpatial())
			{
				m_spatial.m_extent.xMin = pStream->ReadDouble();
				m_spatial.m_extent.yMin = pStream->ReadDouble();
				m_spatial.m_extent.xMax = pStream->ReadDouble();
				m_spatial.m_extent.yMax = pStream->ReadDouble();
				m_spatial.m_dCellSize = pStream->ReadDouble();
			}
		}
	}
}
