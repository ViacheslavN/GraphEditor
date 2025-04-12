#pragma once

#include "GeoDatabase.h"


namespace GraphEngine
{
    namespace GeoDatabase {

        class CJoin : public IJoin
        {
        public:
            CJoin();
            virtual ~CJoin();

            virtual IFieldSetPtr    GetFieldSet() const;
            virtual void    SetFieldSet(IFieldSetPtr  ptrFields);
            virtual const std::string& GetTableName() const;
            virtual void SetTableName(const std::string& tableName);
            virtual eJoinType GetJoinType() const;
            virtual void SetJoinType(eJoinType type);
            virtual eJoinOperation  GetJoinOperation() const;
            virtual void  SetJoinOperation(eJoinOperation joinOp);
            virtual const std::string&  GetTablePrefix() const;
            virtual void  SetTablePrefix(const std::string& tablePrefix);
            virtual const std::string& GetFirstField() const;
            virtual void SetFirstField(const std::string& field);
            virtual const std::string& GetSecondField() const;
            virtual void SetSecondField(const std::string& field);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const ;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            IFieldSetPtr m_ptrFieldSet;
            std::string  m_tableName;
            eJoinType m_joinType;
            eJoinOperation m_joinOp;
            std::string m_tablePrefix;
            std::string m_firstFiled;
            std::string m_secondFiled;
        };

    }
    }