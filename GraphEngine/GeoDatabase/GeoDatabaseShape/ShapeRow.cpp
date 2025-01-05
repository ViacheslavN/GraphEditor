#include "ShapeRow.h"
#include "../../CommonLib/str/StringEncoding.h"


namespace GraphEngine {
    namespace GeoDatabase {

        CShapeRow::CShapeRow(CShapeDBFilePtr ptrDBFile, CShapeFilePtr ptrShapeFile, IFieldsPtr  ptrFields ):
                TBase(ptrFields),
                m_ptrDBFile(ptrDBFile),
                m_rowId(-1),
                m_ptrShapeFile(ptrShapeFile)
        {

        }

        CShapeRow::~CShapeRow()
        {

        }

        void CShapeRow::SetRow(int rowId)
        {
            m_rowId = rowId;
        }

        int64_t  CShapeRow::GetRowId() const
        {
            return (int64_t)m_rowId;
        }

        int8_t CShapeRow::ReadInt8(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int8_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        uint8_t CShapeRow::ReadUInt8(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint8_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        int16_t CShapeRow::ReadInt16(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int16_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        uint16_t CShapeRow::ReadUInt16(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint16_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        int32_t CShapeRow::ReadInt32(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int32_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        uint32_t CShapeRow::ReadUInt32(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint32_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        int64_t CShapeRow::ReadInt64(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (int64_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        uint64_t CShapeRow::ReadUInt64(int32_t col) const
        {
            CheckIfFiledIsInteger(col);
            return (uint64_t)m_ptrDBFile->ReadIntegerAttribute(m_rowId, col);
        }

        float CShapeRow::ReadFloat(int32_t col) const
        {
            CheckIfFiledIsDouble(col);
            return  (float)m_ptrDBFile->ReadDoubleAttribute(m_rowId, col);
        }

        double CShapeRow::ReadDouble(int32_t col) const
        {
            CheckIfFiledIsDouble(col);
            return  m_ptrDBFile->ReadDoubleAttribute(m_rowId, col);
        }

        void CShapeRow::ReadText(int32_t col, std::string& text) const
        {
            text = ReadText(col);
        }

        std::string CShapeRow::ReadText(int32_t col) const
        {
            CheckIfFiledIsStringType(col);
            const char * pszData = m_ptrDBFile->ReadStringAttribute(m_rowId, col);
            if(pszData == NULL)
                return  std::string();

            return  pszData;
        }

        void CShapeRow::ReadTextW(int32_t col, std::wstring& text) const
        {
            text = ReadTextW(col);
        }

        std::wstring CShapeRow::ReadTextW(int32_t col) const
        {
            CheckIfFiledIsStringType(col);
            std::string  sResult = ReadText(col);
            return CommonLib::StringEncoding::str_utf82w_safe(sResult);
        }

        void CShapeRow::ReadBlob(int col, byte_t **pBuf, int32_t& size) const
        {
            throw CommonLib::CExcBase("ShapeRow doesn't support blob type");
        }

        CommonLib::IGeoShapePtr CShapeRow::ReadShape(int32_t col) const
        {
        /*    CSHPObjectPtr ptrObject =  m_ptrShapeFile->ReadObject(m_rowId);
            if(!m_ptrGeoShapeCache.get())
                m_ptrGeoShapeCache = std::make_shared<CommonLib::CGeoShape>();

            CShapefileUtils::SHPObjectToGeometry(ptrObject, m_ptrGeoShapeCache);*/

            return m_ptrGeoShapeCache;
        }

        void CShapeRow::SetShape(CommonLib::IGeoShapePtr ptrShape)
        {
            m_ptrGeoShapeCache = ptrShape;
        }



        void CShapeRow::BindInt8(int32_t col, int8_t val)
        {

        }

        void CShapeRow::BindUInt8(int32_t col, uint8_t val)
        {

        }

        void CShapeRow::BindInt16(int32_t col, int16_t val)
        {

        }

        void CShapeRow::BindUInt16(int32_t col, uint16_t val)
        {

        }

        void CShapeRow::BindInt32(int32_t col, int32_t val)
        {

        }

        void CShapeRow::BindUInt32(int32_t col, uint32_t val)
        {

        }

        void CShapeRow::BindInt64(int32_t col, int64_t val)
        {

        }

        void CShapeRow::BindUInt64(int32_t col, uint64_t val)
        {

        }

        void CShapeRow::BindFloat(int32_t col, float val)
        {

        }

        void CShapeRow::BindDouble(int32_t col, double val)
        {

        }

        void CShapeRow::BindText(int32_t col, const std::string& text, bool copy)
        {

        }

        void CShapeRow::BindTextW(int32_t col, const std::wstring& text, bool copy)
        {

        }

        void CShapeRow::BindBlob(int32_t col, const byte_t *pBuf, int32_t size, bool copy)
        {

        }

        void CShapeRow::BindShape(int32_t col, CommonLib::IGeoShapePtr ptrShape)
        {

        }
    }
}