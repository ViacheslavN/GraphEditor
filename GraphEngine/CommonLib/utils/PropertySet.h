#pragma once
#include "../CommonLib.h"
#include <any>

namespace CommonLib
{

    typedef std::shared_ptr<class IPropertySet> IPropertySetPtr;

    class IPropertySet
    {
    public:
        IPropertySet(){}
        virtual ~IPropertySet(){}
        virtual int  Count() const = 0;
        virtual bool  PropertyExists(const std::string& name) const = 0;
        virtual const std::any&  GetProperty(const std::string& name) const = 0;
        virtual void  SetProperty(const std::string& name, const std::any& value) = 0;
        virtual void  RemoveProperty(const std::string& name) = 0;
        virtual void  RemoveAllProperties() = 0;

    };


    class CPropertySet
    {
    public:
        CPropertySet();
        virtual ~CPropertySet();
        virtual int  Count() const;
        virtual bool  PropertyExists(const std::string& name) const;
        virtual const std::any&  GetProperty(const std::string& name) const;
        virtual void  SetProperty(const std::string& name, const std::any& value);
        virtual void  RemoveProperty(const std::string& name);
        virtual void  RemoveAllProperties();

    private:
        typedef std::map<std::string, std::any> TMapProp;
        TMapProp m_mapProp;
    };
}
