#include "stdafx.h"
#include "SerializeXML.h"

namespace CommonLib {
    CSerializeObjXML::CSerializeObjXML(xml::IXMLNodePtr ptrNode) : m_ptrNode(ptrNode)
    {

    }

    CSerializeObjXML::~CSerializeObjXML()
    {

    }

    const astr& CSerializeObjXML::GetName() const
    {
        return m_ptrNode->GetName();
    }

    void  CSerializeObjXML::SetName(const astr& name)
    {
        m_ptrNode->SetName(name);
    }

    ISerializeObjPtr  CSerializeObjXML::CreateChildNode(const astr& name) const
    {
        return std::make_shared<CSerializeObjXML>(m_ptrNode->CreateChildNode(name));
    }

    uint32_t CSerializeObjXML::GetChildCnt() const
    {
        return m_ptrNode->GetChildCnt();
    }

    ISerializeObjPtr CSerializeObjXML::GetChild(uint32_t nIndex) const
    {
        return std::make_shared<CSerializeObjXML>(m_ptrNode->GetChild(nIndex));
    }

    ISerializeObjPtr CSerializeObjXML::GetChild(const astr& name) const
    {
        return std::make_shared<CSerializeObjXML>(m_ptrNode->GetChild(name));
    }

    std::vector<ISerializeObjPtr> CSerializeObjXML::GetChilds(const astr& name) const
    {
        std::vector<xml::IXMLNodePtr> nodes = m_ptrNode->GetChilds(name);
        std::vector<ISerializeObjPtr> ret;

        for (size_t i = 0, sz = nodes.size(); i < sz; ++i)
        {
            ret.push_back(std::make_shared<CSerializeObjXML>(nodes[i]));
        }

        return ret;
    }

    bool CSerializeObjXML::IsChildExists(const astr& name) const
    {
        return m_ptrNode->IsChildExists(name);
    }

    void CSerializeObjXML::GetBlob(CommonLib::Data::TVecBuffer& data) const
    {
        m_ptrNode->GetBlobCDATA(data);
    }

    void CSerializeObjXML::SetBlob(const byte_t* data, size_t size)
    {
        m_ptrNode->SetBlobCDATA(data, size);
    }

    void CSerializeObjXML::AddPropertyInt16(const  astr& name, int16_t value)
    {
        m_ptrNode->AddPropertyInt16(name, value);
    }

    void CSerializeObjXML::AddPropertyInt16U(const astr& name, uint16_t value)
    {
        m_ptrNode->AddPropertyInt16U(name, value);
    }

    void CSerializeObjXML::AddPropertyInt32(const astr& name, int32_t value)
    {
        m_ptrNode->AddPropertyInt32(name, value);
    }

    void CSerializeObjXML::AddPropertyInt32U(const astr& name, uint32_t value)
    {
        m_ptrNode->AddPropertyInt32U(name, value);
    }

    void CSerializeObjXML::AddPropertyInt64(const astr& name, int64_t value)
    {
        m_ptrNode->AddPropertyInt64(name, value);
    }

    void CSerializeObjXML::AddPropertyIntU64(const astr& name, uint64_t value)
    {
        m_ptrNode->AddPropertyIntU64(name, value);
    }

    void CSerializeObjXML::AddPropertyDouble(const astr& name, double value)
    {
        m_ptrNode->AddPropertyDouble(name, value);
    }

    void CSerializeObjXML::AddPropertyBool(const astr& name, bool value)
    {
        m_ptrNode->AddPropertyBool(name, value);
    }

    void CSerializeObjXML::AddPropertyString(const astr& name, const astr& valueUtf8)
    {
        m_ptrNode->AddPropertyString(name, valueUtf8);
    }

    void CSerializeObjXML::AddPropertyWString(const astr& name, const wstr& value)
    {
        m_ptrNode->AddPropertyWString(name, value);
    }


    bool CSerializeObjXML::IsPropertyExists(const astr& name) const
    {
        return m_ptrNode->IsPropertyExists(name);
    }

    const astr& CSerializeObjXML::GetProperty(const astr& name) const
    {
        return m_ptrNode->GetProperty(name);
    }

    uint32_t CSerializeObjXML::GetPropertyCnt() const
    {
        return m_ptrNode->GetPropertyCnt();
    }

    const astr&  CSerializeObjXML::GetProperty(uint32_t nIndex) const
    {
        return m_ptrNode->GetProperty(nIndex);
    }

    int16_t	CSerializeObjXML::GetPropertyInt16(const astr& name, int16_t defValue) const
    {
        return m_ptrNode->GetPropertyInt16(name, defValue);
    }

    uint16_t CSerializeObjXML::GetPropertyInt16U(const astr& name, uint16_t defValue) const
    {
        return m_ptrNode->GetPropertyInt16U(name, defValue);
    }

    int32_t	 CSerializeObjXML::GetPropertyInt32(const astr& name, int32_t defValue) const
    {
        return m_ptrNode->GetPropertyInt32(name, defValue);
    }

    uint32_t CSerializeObjXML::GetPropertyInt32U(const astr& name, uint32_t defValue) const
    {
        return m_ptrNode->GetPropertyInt32U(name, defValue);
    }

    int64_t	CSerializeObjXML::GetPropertyInt64(const astr& name, int64_t defValue) const
    {
        return m_ptrNode->GetPropertyInt64(name, defValue);
    }

    uint64_t CSerializeObjXML::GetPropertyIntU64(const astr& name, uint64_t defValue) const
    {
        return m_ptrNode->GetPropertyIntU64(name, defValue);
    }

    double  CSerializeObjXML::GetPropertyDouble(const astr& name, double defValue) const
    {
        return m_ptrNode->GetPropertyDouble(name, defValue);
    }

    bool  CSerializeObjXML::GetPropertyBool(const astr& name, bool defValue) const
    {
        return m_ptrNode->GetPropertyBool(name, defValue);
    }

    astr CSerializeObjXML::GetPropertyString(const astr& name, const  astr& defValueUtf8) const
    {
        return m_ptrNode->GetPropertyString(name, defValueUtf8);
    }

    wstr CSerializeObjXML::GetPropertyWString(const astr& name, const  wstr& defValue) const
    {
        return m_ptrNode->GetPropertyWString(name, defValue);
    }

    int16_t	CSerializeObjXML::GetPropertyInt16(const astr& name) const
    {
        return m_ptrNode->GetPropertyInt16(name);
    }

    uint16_t CSerializeObjXML::GetPropertyInt16U(const astr& name) const
    {
        return m_ptrNode->GetPropertyInt16U(name);
    }

    int32_t	 CSerializeObjXML::GetPropertyInt32(const astr& name) const
    {
        return m_ptrNode->GetPropertyInt32(name);
    }

    uint32_t CSerializeObjXML::GetPropertyInt32U(const astr& name) const
    {
        return m_ptrNode->GetPropertyInt32U(name);
    }

    int64_t	CSerializeObjXML::GetPropertyInt64(const astr& name) const
    {
        return m_ptrNode->GetPropertyInt64(name);
    }

    uint64_t CSerializeObjXML::GetPropertyIntU64(const astr& name) const
    {
        return m_ptrNode->GetPropertyIntU64(name);
    }

    double  CSerializeObjXML::GetPropertyDouble(const astr& name) const
    {
        return m_ptrNode->GetPropertyDouble(name);
    }

    bool  CSerializeObjXML::GetPropertyBool(const astr& name) const
    {
        return m_ptrNode->GetPropertyBool(name);
    }

    astr CSerializeObjXML::GetPropertyString(const astr& name) const
    {
        return m_ptrNode->GetPropertyString(name);
    }
}