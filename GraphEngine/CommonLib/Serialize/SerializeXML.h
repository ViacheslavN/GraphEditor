#pragma once
#include "CommonLib.h"
#include "SerializeObj.h"
#include "../xml/xml.h"
#include "../str/convertToString.h"

namespace CommonLib {
    class CSerializeObjXML : public ISerializeObj
    {
    public:
        CSerializeObjXML(xml::IXMLNodePtr ptrNode);
        virtual ~CSerializeObjXML();

        virtual const astr& GetName() const;
        virtual void       SetName(const astr&  name);
        virtual ISerializeObjPtr   CreateChildNode(const astr& name) const;
        virtual uint32_t	GetChildCnt() const;
        virtual ISerializeObjPtr	GetChild(uint32_t nIndex) const;
        virtual ISerializeObjPtr	GetChild(const astr& name) const;
        virtual std::vector<ISerializeObjPtr> GetChilds(const astr& name) const;
        virtual bool	IsChildExists(const astr& name) const;

        virtual	void GetBlob(CommonLib::Data::TVecBuffer& data) const;
        virtual void SetBlob(const byte_t* data, size_t size);

        virtual void AddPropertyInt16(const  astr& name, int16_t value);
        virtual void AddPropertyInt16U(const astr& name, uint16_t value);
        virtual void AddPropertyInt32(const astr& name, int32_t value);
        virtual void AddPropertyInt32U(const astr& name, uint32_t value);
        virtual void AddPropertyInt64(const astr& name, int64_t value);
        virtual void AddPropertyIntU64(const astr& name, uint64_t value);
        virtual void AddPropertyDouble(const astr& name, double value);
        virtual void AddPropertyBool(const astr& name, bool value);
        virtual void AddPropertyString(const astr& name, const astr& valueUtf8);
        virtual void AddPropertyWString(const astr& name, const wstr& value);


        virtual bool IsPropertyExists(const astr& name) const;
        virtual const astr& GetProperty(const astr& name) const;
        virtual uint32_t GetPropertyCnt() const;
        virtual const astr&  GetProperty(uint32_t nIndex) const;

        virtual int16_t	GetPropertyInt16(const astr& name, int16_t defValue) const;
        virtual uint16_t GetPropertyInt16U(const astr& name, uint16_t defValue) const;
        virtual int32_t	 GetPropertyInt32(const astr& name, int32_t defValue) const;
        virtual uint32_t GetPropertyInt32U(const astr& name, uint32_t defValue) const;
        virtual int64_t	GetPropertyInt64(const astr& name, int64_t defValue) const;
        virtual uint64_t GetPropertyIntU64(const astr& name, uint64_t defValue) const;
        virtual double  GetPropertyDouble(const astr& name, double defValue) const;
        virtual bool  GetPropertyBool(const astr& name, bool defValue) const;
        virtual astr GetPropertyString(const astr& name, const  astr& defValueUtf8) const;
        virtual wstr GetPropertyWString(const astr& name, const  wstr& defValue) const;


        virtual int16_t	GetPropertyInt16(const astr& name) const;
        virtual uint16_t GetPropertyInt16U(const astr& name) const;
        virtual int32_t	 GetPropertyInt32(const astr& name) const ;
        virtual uint32_t GetPropertyInt32U(const astr& name) const ;
        virtual int64_t	GetPropertyInt64(const astr& name) const;
        virtual uint64_t GetPropertyIntU64(const astr& name) const;
        virtual double  GetPropertyDouble(const astr& name) const;
        virtual bool  GetPropertyBool(const astr& name) const;
        virtual astr GetPropertyString(const astr& name) const;
    private:
        xml::IXMLNodePtr m_ptrNode;
    };


}