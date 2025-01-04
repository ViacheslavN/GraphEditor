#pragma once
#include "GeoDatabase.h"
#include "../CommonLib/str/StrUtils.h"

namespace GraphEngine
{
    namespace GeoDatabase {
        class CFieldSet : public IFieldSet
        {

        public:
            CFieldSet();
            virtual ~CFieldSet();
        private:
            CFieldSet(const CFieldSet&);
            CFieldSet& operator=(const CFieldSet&);

        public:
            // IFieldSet
            virtual int  GetCount() const;
            virtual int Find(const std::string& field) const;
            virtual void Reset();
            virtual bool Next(std::string& field);
            virtual void Add(const std::string& field);
            virtual void Remove(const std::string& field);
            virtual	const std::string& Get(int nIndex) const;
            virtual void Clear();

        private:
            struct FieldComparator
            {
                bool operator ()(const std::string& str1, const  std::string& str2) const
                {
                    return  CommonLib::StringUtils::Compare(str1, str2, false) < 0;
                }
            };
            typedef std::map<std::string, int, FieldComparator> TMapIndexes;
            typedef std::vector<std::string> TVecFields;
            TMapIndexes m_mapIndexes;
            TVecFields  m_vecFields;
            TVecFields::iterator m_it;

        };

    }
}