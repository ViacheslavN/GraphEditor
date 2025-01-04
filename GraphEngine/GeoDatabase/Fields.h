#pragma once
#include "GeoDatabase.h"
#include "../CommonLib/str/StrUtils.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        class  CFields : public IFields
        {

        public:
            CFields();
            virtual ~CFields();
        private:
            CFields(const CFields&);
            CFields& operator=(const CFields&);

        public:
            // IFields
            virtual int       GetFieldCount() const;
            virtual void      SetFieldCount(int count);
            virtual IFieldPtr GetField(int index) const;
            virtual IFieldPtr  GetField(const std::string& name) const;
            virtual void      SetField(int index, IFieldPtr field);
            virtual void      AddField(IFieldPtr field);
            virtual void      RemoveField(int index);
            virtual int       FindField(const std::string& name) const;
            virtual bool      FieldExists(const std::string& name) const;
            virtual void	  Clear();
            virtual IFieldsPtr	Clone() const;

        private:
            void RebuildFieldIndexMap() const;
            struct FieldComparator
            {
                bool operator ()(const std::string& str1, const  std::string& str2) const
                {
                    return  CommonLib::StringUtils::Compare(str1, str2, false) < 0;
                }
            };
            typedef std::map<const std::string, int, FieldComparator> TMapFieldIndex;
            typedef std::vector<IFieldPtr> TFields;
            TFields m_vecFields;
            mutable TMapFieldIndex m_mapFieldIndex;
        };

    }
}