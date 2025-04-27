#pragma once
#include "GeoDatabase.h"
#include "FieldSet.h"
#include "../GisGeometry/Geometry.h"
#include "../GisGeometry/Envelope.h"

namespace GraphEngine
{
    namespace GeoDatabase {

        template<class I>
        class ICursorBase : public I
        {
        public:

            ICursorBase(IFieldsPtr  ptrFields):
                    m_spatialRel(srlUndefined)
                    , m_bNeedTransform(false)
                    , m_ptrFields(ptrFields)
            {

            }


            ICursorBase(IFieldsPtr  ptrFields, IQueryFilterPtr ptrFilter):
                    m_spatialRel(srlUndefined)
                    , m_bNeedTransform(false)
                    , m_ptrFilter(ptrFilter)
                    , m_ptrFields(ptrFields)
            {

            }

            ICursorBase(IFieldsPtr  ptrFields, IQueryFilterPtr ptrFilter, Geometry::ISpatialReferencePtr ptrSpatRefSource) :
             m_spatialRel(srlUndefined)
            , m_bNeedTransform(false)
            , m_ptrFilter(ptrFilter)
            , m_ptrFields(ptrFields)
            {
                // Spatial queries
                if(ptrSpatRefSource.get() != nullptr)
                {
                    Geometry::ISpatialReferencePtr ptrSpatRefOutput = m_ptrFilter->GetOutputSpatialReference();
                    ISpatialFilter *pSpatialFilter = dynamic_cast<ISpatialFilter *>(m_ptrFilter.get());
                    if(pSpatialFilter)
                    {
                        m_spatialRel = pSpatialFilter->GetSpatialRel();
                        if(m_spatialRel != srlUndefined)
                        {
                            if(pSpatialFilter->GetBB().type == CommonLib::bbox_type_normal)
                            {
                                m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(pSpatialFilter->GetBB(), ptrSpatRefOutput);
                                m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(pSpatialFilter->GetBB(), ptrSpatRefOutput);

                                m_ptrExtentOutput->Project(ptrSpatRefOutput);
                                m_ptrExtentSource->Project(ptrSpatRefSource);
                            }
                            else
                            {
                                CommonLib::IGeoShapePtr pShape = pSpatialFilter->GetShape();
                                m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), ptrSpatRefOutput);
                                m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), ptrSpatRefOutput);

                                m_ptrExtentOutput->Project(ptrSpatRefOutput);
                                m_ptrExtentSource->Project(ptrSpatRefSource);
                            }

                        }
                        else
                        {
                            m_ptrExtentOutput = std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrSpatRefOutput);
                            m_ptrExtentSource = std::make_shared<Geometry::CEnvelope>(CommonLib::bbox(), ptrSpatRefSource);
                        }

                        m_bNeedTransform = ptrSpatRefOutput.get() != nullptr
                                           && ptrSpatRefSource.get() != nullptr
                                           && !ptrSpatRefOutput->IsEqual(ptrSpatRefSource);

                    }
                }
            }



            virtual ~ICursorBase(){}


            virtual int32_t  ColumnCount() const
            {
                if(m_ptrFields.get() == nullptr)
                    throw CommonLib::CExcBase("Not information");

                return m_ptrFields->GetFieldCount();
            }

            virtual std::string ColumnName(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row: failed to get column name out of range, index: {0}", col);

                return m_ptrFields->GetField(col)->GetName();
            }



            virtual eDataTypes GetColumnType(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row: failed to get   column type, out of range: index: {0}", col);

                return m_ptrFields->GetField(col)->GetType();
            }

            virtual int32_t GetColumnBytes(int32_t col) const
            {
                if(col >= ColumnCount() )
                    throw CommonLib::CExcBase("Row failed to get column lengh, out of range: index: {0}", col);

                return m_ptrFields->GetField(col)->GetLength();
            }

            virtual int32_t FindFieldByName(const std::string& name) const
            {
                return  m_ptrFields->FindField(name);
            }

        protected:
            bool IsIntegerType(int col) const
            {
                eDataTypes type =  GetColumnType(col);

                return type == dtInteger8 ||
                       type ==  dtInteger16  ||
                       type ==  dtInteger32  ||
                       type ==  dtInteger64 ||
                       type ==  dtUInteger8 ||
                       type ==  dtUInteger16 ||
                       type ==  dtUInteger32 ||
                       type ==  dtUInteger64;
            }

            void CheckIfFiledIsInteger(int col) const
            {
                if(!IsIntegerType(col))
                    throw CommonLib::CExcBase("Row field: {0} isn't integer type", col);
            }

            void CheckIfFiledIsStringType(int col) const
            {
                eDataTypes type =  GetColumnType(col);
                if(type != dtString)
                    throw CommonLib::CExcBase("Row field: {0} isn't string type", col);
            }

            void CheckIfFiledIsDouble(int col) const
            {
                eDataTypes type =  GetColumnType(col);
                if(type != dtFloat && type != dtDouble)
                    throw CommonLib::CExcBase("Row field: {0} isn't double type", col);
            }


            std::string CreateSelectPartQuery( IQueryFilterPtr ptrFilter, IFieldsPtr ptrSourceFields )
            {


                const std::vector<IJoinPtr>& vecJoints =  ptrFilter->GetJoins();

                std::vector<std::string> gueryFields;
                IFieldSetPtr ptrFieldsSet =  ptrFilter->GetFieldSet();
                const std::string& strMainTablePrefix = ptrFilter->GetTablePrefix();

                if(ptrFieldsSet->GetCount() != 0)
                {
                    for(int i = 0, sz = ptrFieldsSet->GetCount(); i < sz; ++i)
                    {
                        if(strMainTablePrefix.empty())
                            gueryFields.push_back( ptrFieldsSet->Get(i));
                        else
                            gueryFields.push_back(strMainTablePrefix + "." + ptrFieldsSet->Get(i));
                    }
                }
                else
                {
                    for(int  i = 0, sz = ptrSourceFields->GetFieldCount(); i < sz; ++i)
                    {
                        if(strMainTablePrefix.empty())
                            gueryFields.push_back(ptrSourceFields->GetField(i)->GetName());
                        else
                            gueryFields.push_back(strMainTablePrefix + "." + ptrSourceFields->GetField(i)->GetName());
                    }
                }


                for(size_t i = 0, sz = vecJoints.size(); i < sz; ++i)
                {
                    IJoinPtr ptrJoin = vecJoints[i];
                    IFieldSetPtr ptrJointFieldsSet = ptrJoin->GetFieldSet();
                    std::string jointPrefix = ptrJoin->GetTablePrefix();

                    if(ptrJointFieldsSet->GetCount() != 0)
                    {
                        for(int i = 0, sz = ptrJointFieldsSet->GetCount(); i < sz; ++i)
                        {
                            if(jointPrefix.empty())
                                gueryFields.push_back(ptrJointFieldsSet->Get(i));
                            else
                                gueryFields.push_back(jointPrefix + "." + ptrJointFieldsSet->Get(i));
                        }
                    }
                    /*else
                    {
                        IFieldsPtr jointFields = ptrJoin->GetTable()->GetFields();
                        for(int  i = 0, sz = jointFields->GetFieldCount(); i < sz; ++i)
                        {
                            if(jointPrefix.empty())
                                gueryFields.push_back(jointFields->GetField(i)->GetName());
                            else
                                gueryFields.push_back(jointPrefix + "." + jointFields->GetField(i)->GetName());
                        }
                    }*/
                }

                if(gueryFields.empty())
                    throw CommonLib::CExcBase("Not fields are set");


                std::string strSqlQuery = "SELECT ";
                strSqlQuery += gueryFields[0];

                for(size_t i = 1, sz = gueryFields.size(); i < sz; ++i)
                {
                    strSqlQuery += ",";
                    strSqlQuery += gueryFields[i];
                }


                return  strSqlQuery;
            }

            std::string CreateJoinPartQuery( const std::vector<IJoinPtr>& vecJoins,  const std::string& sTablePrefix)
            {
                std::string sSQl;

                for(size_t i = 0, sz = vecJoins.size(); i < sz; ++i)
                {
                    IJoinPtr ptrJoin = vecJoins[i];

                    std::string sValPrefix = ptrJoin->GetTablePrefix();

                    switch(ptrJoin->GetJoinType())
                    {
                        case eJoinType::jtInnerJoin:
                            sSQl +=" INNER JOIN ";
                        break;
                        case eJoinType::jtLeftJoin:
                            sSQl +=" LEFT JOIN ";
                            break;
                        case eJoinType::jtOuterJoin:
                            sSQl +="FULL OUTER JOIN ";
                            break;
                        case eJoinType::jtRightJoin:
                            sSQl +=" RIGHT JOIN ";
                            break;
                    }

                    sSQl += ptrJoin->GetTableName();
                    if(!sValPrefix.empty())
                        sSQl +=  " AS " + sValPrefix;

                    sSQl += " ON ";

                    if(!sValPrefix.empty())
                        sSQl+= sValPrefix + ".";

                    sSQl += ptrJoin->GetFirstField();

                    switch(ptrJoin->GetJoinOperation())
                    {
                        case eJoinOperation::jotEqual:
                            sSQl +=" = ";
                            break;
                        case eJoinOperation::jotNotEqual:
                            sSQl +=" !=";
                            break;
                        case eJoinOperation::jotLess:
                            sSQl +=" <";
                            break;
                        case eJoinOperation::jotLessOrEqual:
                            sSQl += " <= ";
                            break;
                        case eJoinOperation::jotGreater:
                            sSQl +=" >";
                            break;
                        case eJoinOperation::jotGreaterOrEqual:
                            sSQl += " >= ";
                            break;
                    }

                    if(!sTablePrefix.empty())
                        sSQl+= sTablePrefix + ".";

                    sSQl += ptrJoin->GetSecondField() + "\n";


                }


                return  sSQl;
            }




        protected:

            IQueryFilterPtr m_ptrFilter;
            IFieldsPtr      m_ptrFields;

            //spatial part
            Geometry::IEnvelopePtr  m_ptrExtentOutput;
            Geometry::IEnvelopePtr  m_ptrExtentSource;
            bool m_bNeedTransform;
            eSpatialRel				   m_spatialRel;

        };
    }
}