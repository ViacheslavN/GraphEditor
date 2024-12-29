#pragma once

#include "CommonLib.h"
#include "../str/convertToString.h"
#include "../data/ByteArray.h"
#include "../stream/stream.h"

namespace CommonLib
	{
		namespace xml
		{

			typedef std::shared_ptr<class IXMLNode> IXMLNodePtr;
			typedef std::shared_ptr<class IXMLDoc> IXMLDocPtr;

			class IXMLNode
			{
			public:

				IXMLNode() {}
				virtual ~IXMLNode() {}

				virtual IXMLNodePtr	CreateChildNode(const astr& name) = 0;
				virtual uint32_t	GetChildCnt() const = 0;
				virtual IXMLNodePtr	GetChild(uint32_t nIndex) const = 0;
				virtual IXMLNodePtr	GetChild(const astr& name) const = 0;
				virtual std::vector<IXMLNodePtr> GetChilds(const astr& name) const = 0;
				virtual bool	IsChildExists(const astr& name) const = 0;



				virtual IXMLNodePtr	GetParent() const = 0;


				virtual const astr& GetName() const = 0;
				virtual void        SetName(const astr& name) = 0;
				virtual const astr& GetText() const = 0;
				virtual void        SetText(const astr& textUtf8) = 0;
				virtual void        SetText(const wstr&) = 0;


				virtual const astr&   GetCDATA() const = 0;
				virtual void          SetCDATA(const astr& cdata) = 0;

				virtual	void GetBlobCDATA(Data::TVecBuffer& data) const = 0;
				virtual void SetBlobCDATA(const byte_t* data, size_t size) = 0;

				virtual void AddPropertyInt16(const  astr& name, int16_t value) = 0;
				virtual void AddPropertyInt16U(const astr& name, uint16_t value) = 0;
				virtual void AddPropertyInt32(const astr& name, int32_t value) = 0;
				virtual void AddPropertyInt32U(const astr& name, uint32_t value) = 0;
				virtual void AddPropertyInt64(const astr& name, int64_t value) = 0;
				virtual void AddPropertyIntU64(const astr& name, uint64_t value) = 0;
				virtual void AddPropertyDouble(const astr& name, double value) = 0;
				virtual void AddPropertyBool(const astr& name, bool value) = 0;
				virtual void AddPropertyString(const astr& name, const astr& valueUtf8) = 0;
				virtual void AddPropertyWString(const astr& name, const wstr& value) = 0;


				virtual bool IsPropertyExists(const astr& name) const = 0;
				virtual const astr& GetProperty(const astr& name) const = 0;
				virtual uint32_t GetPropertyCnt() const = 0;
				virtual const astr&  GetProperty(uint32_t nIndex) const = 0;

				virtual int16_t	GetPropertyInt16(const astr& name, int16_t defValue) const = 0;
				virtual uint16_t GetPropertyInt16U(const astr& name, uint16_t defValue) const = 0;
				virtual int32_t	 GetPropertyInt32(const astr& name, int32_t defValue) const = 0;
				virtual uint32_t GetPropertyInt32U(const astr& name, uint32_t defValue) const = 0;
				virtual int64_t	GetPropertyInt64(const astr& name, int64_t defValue) const = 0;
				virtual uint64_t GetPropertyIntU64(const astr& name, uint64_t defValue) const = 0;
				virtual double  GetPropertyDouble(const astr& name, double defValue) const = 0;
				virtual bool  GetPropertyBool(const astr& name, bool defValue) const = 0;
				virtual astr GetPropertyString(const astr& name, const  astr& defValueUtf8) const = 0;
				virtual wstr GetPropertyWString(const astr& name, const  wstr& defValue) const = 0;


				virtual int16_t	GetPropertyInt16(const astr& name) const = 0;
				virtual uint16_t GetPropertyInt16U(const astr& name) const = 0;
				virtual int32_t	 GetPropertyInt32(const astr& name) const = 0;
				virtual uint32_t GetPropertyInt32U(const astr& name) const = 0;
				virtual int64_t	GetPropertyInt64(const astr& name) const = 0;
				virtual uint64_t GetPropertyIntU64(const astr& name) const = 0;
				virtual double  GetPropertyDouble(const astr& name) const = 0;
				virtual bool  GetPropertyBool(const astr& name) const = 0;
				virtual astr GetPropertyString(const astr& name) const = 0;
				virtual wstr GetPropertyWString(const astr& name) const = 0;

			};

			class IXMLDoc
			{
			public:
				IXMLDoc() {}
				virtual ~IXMLDoc() {}

				virtual void  Open(const astr& xml) = 0;
				virtual void  Open(CommonLib::IReadStream* pStream) = 0;

				virtual void  Save(const astr& xml) = 0;
				virtual void  Save(CommonLib::IWriteStream* pStream) = 0;

				virtual IXMLNodePtr	 GetNodes() const = 0;
				virtual void Clear() = 0;

			};

		}
	}
