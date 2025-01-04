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

            ICursorBase(IQueryFilterPtr ptrFilter, bool recycling, ITablePtr ptrTable) :
                    m_bRecycling(recycling)
                    , m_spatialRel(srlUndefined)
                    , m_nOidFieldIndex(-1)
                    , m_nShapeFieldIndex(-1)
                    , m_nAnnoFieldIndex(-1)
                    , m_bNeedTransform(false)

            {

                m_pTable = pTable;
                m_pFilter = pFilter;
                m_pFieldSet = pFilter->GetFieldSet();

                if(!m_pFilter->GetWhereClause().empty())
                {
                    //TO DO set fields
                }

                m_pSourceFields = m_pTable->GetFields();
                UpdateFields();




                IOIDSetPtr oidSet = m_pFilter->GetOIDSet();
                oidSet->Reset();

                int64_t oid;
                while(oidSet->Next(oid))
                    m_vecOids.push_back(oid);

                std::sort(m_vecOids.begin(), m_vecOids.end());

                m_RowIDIt = m_vecOids.begin();

                // Spatial queries


                ISpatialTable *pSpatialTable =  dynamic_cast<ISpatialTable *>(m_pTable.get());

                if(pSpatialTable)
                {
                    Geometry::ISpatialReferencePtr spatRefOutput = m_pFilter->GetOutputSpatialReference();
                    Geometry::ISpatialReferencePtr spatRefSource = pSpatialTable->GetSpatialReference();

                    ISpatialFilter *spatFilter = dynamic_cast<ISpatialFilter *>(m_pFilter.get());
                    if(spatFilter)
                    {
                        m_spatialRel =  srlUndefined;
                        if(spatFilter)
                            m_spatialRel = spatFilter->GetSpatialRel();

                        if(m_spatialRel != srlUndefined)
                        {
                            if(spatFilter->GetBB().type == CommonLib::bbox_type_normal)
                            {
                                m_pExtentOutput = std::make_shared<Geometry::CEnvelope>(spatFilter->GetBB(), spatRefOutput);
                                m_pExtentSource = std::make_shared<Geometry::CEnvelope>(spatFilter->GetBB(), spatRefOutput);
                                m_pExtentOutput->Project(spatRefOutput);
                                m_pExtentSource->Project(spatRefSource);
                            }
                            else
                            {
                                CommonLib::IGeoShapePtr pShape(spatFilter->GetShape());
                                m_pExtentOutput = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), spatRefOutput);
                                m_pExtentSource = std::make_shared<Geometry::CEnvelope>(pShape->GetBB(), spatRefOutput);

                                m_pExtentOutput->Project(spatRefOutput);
                                m_pExtentSource->Project(spatRefSource);
                            }

                        }
                        else
                        {
                            m_pExtentOutput = std::make_shared<Geometry::CEnvelope>(GisBoundingBox(), spatRefOutput);
                            m_pExtentSource = std::make_shared<Geometry::CEnvelope>(GisBoundingBox(), spatRefSource);
                        }

                        m_bNeedTransform = spatRefOutput != NULL
                                           && spatRefSource != NULL
                                           && !spatRefOutput->IsEqual(spatRefSource);

                    }
                }



            }

            ICursorBase(int64_t nOId, IFieldSet *pFieldSet, ITable* pTable) :
                    m_bRecycling(false)
                    , m_spatialRel(srlUndefined)
                    , m_nOidFieldIndex(-1)
                    , m_nShapeFieldIndex(-1)
                    , m_nAnnoFieldIndex(-1)
                    , m_bNeedTransform(false)
            {
                m_pTable = pTable;
                m_pSourceFields = m_pTable->GetFields();
                m_vecOids.push_back(nOId);
                m_pFieldSet = pFieldSet;
                UpdateFields();

            }
            virtual ~ICursorBase(){}
            virtual IFieldSetPtr GetFieldSet() const
            {
                return m_pFieldSet;
            }
            virtual IFieldsPtr   GetSourceFields() const
            {
                return m_pSourceFields;
            }
            virtual bool         IsFieldSelected(int index) const
            {
                if(index < (int)m_vecFieldsExists.size() && index > -1)
                    return m_vecFieldsExists[index] == 1;

                return false;
            }

            void UpdateFields()
            {

                int fieldCount = m_pSourceFields->GetFieldCount();
                m_vecFieldsExists.resize(fieldCount, 0);
                //	m_vecActualFieldsIndexes.clear();


                m_nOidFieldIndex = -1;
                m_nShapeFieldIndex = -1;
                m_nAnnoFieldIndex = -1;

                if(!m_pFieldSet.get())
                {
                    m_pFieldSet = new CFieldSet();
                    for (int i = 0, sz = m_pSourceFields->GetFieldCount(); i < sz; ++i)
                    {
                        IFieldPtr pField = m_pSourceFields->GetField(i);
                        m_pFieldSet->Add(pField->GetName());
                    }
                }

                m_vecFieldInfo.reserve(m_pFieldSet->GetCount());

                m_pFieldSet->Reset();
                std::string field;
                int nNum = 0;
                while(m_pFieldSet->Next(field))
                {
                    if(field == "*")
                    {
                        //		if(m_pFilter.get())
                        {
                            m_pFieldSet->Clear();
                            for(int i = 0; i < fieldCount; ++i)
                            {
                                IFieldPtr field = m_pSourceFields->GetField(i);
                                m_pFieldSet->Add(field->GetName());
                            }

                            m_pFieldSet->Reset();
                            //m_vecActualFieldsIndexes.clear();
                            //m_vecActualFieldsTypes.clear();
                            continue;
                        }

                    }


                    int fieldIndex = m_pSourceFields->FindField(field);
                    IFieldPtr pField = m_pSourceFields->GetField(fieldIndex);

                    m_vecFieldInfo.push_back(sFieldInfo(nNum,  fieldIndex, pField->GetType()));

                    m_vecFieldsExists[fieldIndex] = 1;
                    //	m_vecActualFieldsIndexes.push_back(fieldIndex);
                    //	m_vecActualFieldsTypes.push_back(m_pSourceFields->GetField(fieldIndex)->GetType());

                    if((pField->GetType() == dtOid32 || pField->GetType() == dtOid64)  && m_nOidFieldIndex < 0)
                        m_nOidFieldIndex = fieldIndex;
                    else if(pField->GetType()== dtGeometry && (m_nShapeFieldIndex < 0 || m_nShapeFieldIndex > fieldIndex))
                        m_nShapeFieldIndex = fieldIndex;
                    else if(pField->GetType() == dtAnnotation && (m_nAnnoFieldIndex < 0 || m_nAnnoFieldIndex > fieldIndex))
                        m_nAnnoFieldIndex = fieldIndex;

                    ++nNum;
                }

                // Change fieldset to right names (from DB)
                /*int actualfieldCount = (int)m_vecActualFieldsIndexes.size();
                IFieldSetPtr fieldSet(new CFieldSet());
                for(int i = 0; i < actualfieldCount; ++i)
                    fieldSet->Add(m_pSourceFields->GetField(m_vecActualFieldsIndexes[i])->GetName());
                fieldSet->Reset();
                m_pFieldSet = fieldSet;*/

                //m_pFilter->SetFieldSet(m_pFieldSet.get());
            }

        protected:
            IQueryFilterPtr m_pFilter;
            IFieldsPtr      m_pSourceFields;
            IFieldSetPtr	m_pFieldSet;


            struct  sFieldInfo
            {
                sFieldInfo(){}
                sFieldInfo(int nRowNum, int nDataSetNum, eDataTypes nType) : m_nRowIndex(nRowNum), m_nDataSetIndex(nDataSetNum), m_nType(nType)
                {}
                int m_nRowIndex;
                int m_nDataSetIndex;
                eDataTypes m_nType;
            };

            typedef std::vector<sFieldInfo> TFieldInfo;
            TFieldInfo m_vecFieldInfo;


            std::vector<int>           m_vecFieldsExists;
            std::vector<int>           m_vecActualFieldsIndexes;
            std::vector<eDataTypes>  m_vecActualFieldsTypes;
            ITablePtr m_pTable;
            IRowPtr   m_pCurrentRow;
            bool m_bRecycling;
            std::vector<int64_t>           m_vecOids;
            std::vector<int64_t>::iterator m_RowIDIt;
            int m_nOidFieldIndex;
            int m_nShapeFieldIndex;
            int m_nAnnoFieldIndex;


            Geometry::IEnvelopePtr  m_pExtentOutput;
            Geometry::IEnvelopePtr  m_pExtentSource;
            bool m_bNeedTransform;
            eSpatialRel				   m_spatialRel;
        };
    }
}