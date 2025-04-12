#include "Join.h"
#include "FieldSet.h"
namespace GraphEngine
{
    namespace GeoDatabase {

        CJoin::CJoin()
        {
            m_ptrFieldSet = std::make_shared<CFieldSet>();
            m_joinType = eJoinType::jtInnerJoin;
            m_joinOp = eJoinOperation::jotEqual;
        }

        CJoin::~CJoin()
        {

        }

         IFieldSetPtr     CJoin::GetFieldSet() const
         {
            return m_ptrFieldSet;
         }

         void     CJoin::SetFieldSet(IFieldSetPtr  ptrFields)
         {
             m_ptrFieldSet = ptrFields;
         }



        const std::string& CJoin::GetTableName() const
        {
            return  m_tableName;
        }

        void CJoin::SetTableName(const std::string& tableName)
        {
            m_tableName = tableName;
        }

         eJoinType  CJoin::GetJoinType() const
         {
             return m_joinType;
         }

         void  CJoin::SetJoinType(eJoinType type)
         {
             m_joinType = type;
         }

        eJoinOperation  CJoin::GetJoinOperation() const
        {
            return m_joinOp;
        }

        void  CJoin::SetJoinOperation(eJoinOperation joinOp)
         {
            m_joinOp = joinOp;
         }

         const std::string&  CJoin::GetTablePrefix() const
         {
             return m_tablePrefix;
         }

         void   CJoin::SetTablePrefix(const std::string& tablePrefix)
         {
             m_tablePrefix = tablePrefix;
         }

        const std::string& CJoin::GetFirstField() const
         {
             return  m_firstFiled;
         }

         void CJoin::SetFirstField(const std::string& field)
         {
             m_firstFiled = field;
         }

         const std::string& CJoin::GetSecondField() const
         {
             return m_secondFiled;
         }

         void CJoin::SetSecondField(const std::string& field)
         {
             m_secondFiled = field;
         }


        void CJoin::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyString("TableName", m_tableName);
                pObj->AddPropertyString("TablePrefix", m_tablePrefix);
                pObj->AddPropertyString("FirstField", m_firstFiled);
                pObj->AddPropertyString("SecondField", m_secondFiled);
                pObj->AddPropertyInt32U("JoinOp",(int32_t)m_joinOp);
                pObj->AddPropertyInt32U("JoinType",(int32_t)m_joinType);

                CommonLib::ISerializeObjPtr ptrFieldsNode = pObj->CreateChildNode("Fields");

                for(int32_t i = 0; i < m_ptrFieldSet->GetCount(); ++i)
                {
                    CommonLib::ISerializeObjPtr ptrFieldNode = ptrFieldsNode->CreateChildNode("Field");
                    ptrFieldNode->AddPropertyString("FieldName", m_ptrFieldSet->Get(i));
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to save CJoin", exc);
            }
        }

        void CJoin::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_tableName = pObj->GetPropertyString("TableName");
                m_tablePrefix = pObj->GetPropertyString("TablePrefix");
                m_firstFiled = pObj->GetPropertyString("FirstField");
                m_secondFiled = pObj->GetPropertyString("SecondField");
                m_joinOp  = (eJoinOperation)pObj->GetPropertyInt32U("JoinOp");
                m_joinType = (eJoinType)pObj->GetPropertyInt32U("JoinType");

                CommonLib::ISerializeObjPtr ptrFieldsNode = pObj->GetChild("Fields");

                m_ptrFieldSet = std::make_shared<CFieldSet>();

                std::vector<CommonLib::ISerializeObjPtr> vecFields = ptrFieldsNode->GetChilds("Field");
                for (int i = 0; i < vecFields.size(); ++i)
                {
                    m_ptrFieldSet->Add(vecFields[i]->GetPropertyString("FieldName"));
                }

            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to load CJoin", exc);
            }
        }


    }
}