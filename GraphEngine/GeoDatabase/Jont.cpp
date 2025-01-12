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

         ITablePtr  CJoin::GetTable() const
         {
            return m_ptrTable;
         }

         void  CJoin::SetTable(ITablePtr ptrTable)
         {
             m_ptrTable = ptrTable;
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



    }
}