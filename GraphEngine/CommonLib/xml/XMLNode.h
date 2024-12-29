#pragma once

#include "CommonLib.h"
#include "xml.h"
#include "../str/convertToString.h"

namespace CommonLib
	{
		namespace xml
		{
			class CXMLNode : public IXMLNode, public std::enable_shared_from_this<CXMLNode>
			{
			public:
				CXMLNode(IXMLNodePtr ptrParent, const astr& name = "");
				virtual ~CXMLNode();

				virtual IXMLNodePtr			   CreateChildNode(const astr& name);
				virtual uint32_t			   GetChildCnt() const;
				virtual IXMLNodePtr			   GetChild(uint32_t nIndex) const;
				virtual IXMLNodePtr			   GetChild(const astr& name) const;
				virtual std::vector<IXMLNodePtr> GetChilds(const astr& name) const;
				virtual bool	IsChildExists(const astr& name) const;
				virtual IXMLNodePtr			   GetParent() const;
				
				virtual const astr& GetName() const;
				virtual void        SetName(const astr& name);

				virtual const astr&  GetText() const;
				virtual void  SetText(const astr& textUtf8);
				virtual void  SetText(const wstr& unicode);

				virtual const astr&  GetCDATA() const;
				virtual void  SetCDATA(const  astr& cdata);

				virtual	void GetBlobCDATA(CommonLib::Data::TVecBuffer& data) const;
				virtual void SetBlobCDATA(const byte_t* data, size_t size);

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
				virtual astr GetPropertyString(const astr& name, const  astr& defValue) const;
				virtual wstr GetPropertyWString(const astr& name, const  wstr& defValue) const;


				virtual int16_t	GetPropertyInt16(const astr& name) const;
				virtual uint16_t GetPropertyInt16U(const astr& name) const;
				virtual int32_t	 GetPropertyInt32(const astr& name) const;
				virtual uint32_t GetPropertyInt32U(const astr& name) const;
				virtual int64_t	GetPropertyInt64(const astr& name) const;
				virtual uint64_t GetPropertyIntU64(const astr& name) const;
				virtual double  GetPropertyDouble(const astr& name) const;
				virtual bool  GetPropertyBool(const astr& name) const;
				virtual astr GetPropertyString(const astr& name) const;
				virtual wstr GetPropertyWString(const astr& name) const;

				void Save(CommonLib::IWriteStream *pSteam);
			private:
				void SetParent(IXMLNodePtr ptrParent);

				template  <class T> 
				void AddProperyT(const astr& name, const T& value)
				{
					AddPropertyString(name, CommonLib::str_utils::AStrFrom(value));
				}

				template  <class T>
				T GetProperyT(const astr& name) const
				{
					const astr& val = GetProperty(name);
					return CommonLib::str_utils::AStr2TVal<T>(val);

				}

				template  <class T>
				T GetProperyT(const astr& name, const T& defValue) const
				{
					if (!IsPropertyExists(name))
						return defValue;

					return GetProperyT<T>(name);

				}			

			private:

				astr m_name;
				astr m_textUtf8;
				astr m_caData;

				std::weak_ptr<IXMLNode> m_ptrParent;
				typedef std::vector<IXMLNodePtr> TNodes;
				typedef std::multimap<astr, uint32_t> TNodesByName;

				typedef std::vector<std::pair<astr, astr> > TVecProp;
				typedef std::map<astr, uint32_t> TPropByName;

				TNodes m_Nodes;
				TNodesByName m_NodeByName;

				TVecProp m_Props;
				TPropByName m_PropsByName;
			};
		}
	}
