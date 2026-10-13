#include "Field.h"
#include "../Values.h"

namespace bptreedb
{
	namespace db
	{
		//------------------------------------------------------------------------------------------------
		// CField

		CField::CField(uint32_t nId, const SFieldProp& prop, uint32_t nPageSize) :
			m_nId(nId), m_sName(prop.m_sName), m_sAlias(prop.m_sAlias), m_type(prop.m_type), m_nLength(prop.m_nLength),
			m_bNotNull(prop.m_bNotNull), m_bUnique(prop.m_bUnique)
		{
			try
			{
				CheckName(m_sName, "field");
				if (m_type == dtUnknown || m_type == dtNull || m_type > dtGeometry)
					throw CommonLib::CExcBase("wrong field type {0}", (int)m_type);
				if (m_bUnique && (m_type == dtBlob || m_type == dtGeometry))
					throw CommonLib::CExcBase("a {0} field can't be unique", DataTypeName(m_type));
				if (m_type != dtString)
					m_nLength = IsNumberType(m_type) ? FixedTypeSize(m_type) : 0;

				m_valueStorage = ChooseValueStorage(m_type, m_nLength, nPageSize);
				m_defaultValue = Convert(prop.m_defaultValue);
				if (m_type == dtGeometry && !m_defaultValue.IsNull())
					throw CommonLib::CExcBase("a geometry field can't have a default value");
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("wrong properties of field '{0}'", prop.m_sName, excSrc);
				throw;
			}
		}

		CommonLib::CVariant CField::Convert(const CommonLib::CVariant& value) const
		{
			try
			{
				return ToFieldValue(value, m_type, m_type == dtString ? m_nLength : 0);
			}
			catch (std::exception& excSrc)
			{
				CommonLib::CExcBase::RegenExcT("wrong value of field '{0}'", m_sName, excSrc);
				throw;
			}
		}

		void CField::Write(CommonLib::IWriteStream* pStream) const
		{
			pStream->Write(m_nId);
			pStream->Write(m_sName);
			pStream->Write(m_sAlias);
			pStream->Write((uint32_t)m_type);
			pStream->Write(m_nLength);
			pStream->Write(m_bNotNull);
			pStream->Write(m_bUnique);
			WriteVariant(pStream, m_defaultValue);
			pStream->Write((uint32_t)m_valueStorage);
			pStream->Write(m_nValuePage);
			pStream->Write(m_ptrIndex.get() != nullptr);
			if (m_ptrIndex.get())
				m_ptrIndex->Write(pStream);
		}

		CFieldPtr CField::Read(CommonLib::IReadStream* pStream)
		{
			CFieldPtr ptrField = std::make_shared<CField>();
			ptrField->m_nId = pStream->ReadIntu32();
			pStream->Read(ptrField->m_sName);
			pStream->Read(ptrField->m_sAlias);
			ptrField->m_type = (eDataTypes)pStream->ReadIntu32();
			ptrField->m_nLength = pStream->ReadIntu32();
			ptrField->m_bNotNull = pStream->ReadBool();
			ptrField->m_bUnique = pStream->ReadBool();
			ReadVariant(pStream, ptrField->m_defaultValue);
			ptrField->m_valueStorage = (EValueStorage)pStream->ReadIntu32();
			ptrField->m_nValuePage = pStream->ReadInt64();
			if (pStream->ReadBool())
			{
				ptrField->m_ptrIndex = std::make_shared<CIndex>();
				ptrField->m_ptrIndex->Read(pStream, ptrField);
			}
			return ptrField;
		}

		//------------------------------------------------------------------------------------------------
		// CFields

		IFieldPtr CFields::GetField(int32_t nIndex) const
		{
			if (nIndex < 0 || nIndex >= (int32_t)m_fields.size())
				throw CommonLib::CExcBase("field index {0} is out of range, fields {1}", nIndex, (uint32_t)m_fields.size());
			return m_fields[nIndex];
		}

		IFieldPtr CFields::GetField(const std::string& sName) const
		{
			int32_t n = FindField(sName);
			return n < 0 ? IFieldPtr() : IFieldPtr(m_fields[n]);
		}

		int32_t CFields::FindField(const std::string& sName) const
		{
			for (size_t i = 0; i < m_fields.size(); ++i)
				if (m_fields[i]->GetName() == sName)
					return (int32_t)i;
			return -1;
		}
	}
}
