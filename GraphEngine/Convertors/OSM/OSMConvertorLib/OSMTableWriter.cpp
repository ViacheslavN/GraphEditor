#include "OSMTableWriter.h"
#include "../../../GeoDatabase/Field.h"
#include "../../../GeoDatabase/Fields.h"
#include "../../../GisGeometry/Envelope.h"

namespace GraphEngine {
    namespace Convertors {

        const char* COSMTableWriter::OIDField = "OID";
        const char* COSMTableWriter::ShapeField = "Shape";

        COSMTableWriter::COSMTableWriter(GeoDatabase::IDatabaseWorkspacePtr ptrWorkspace, GeoDatabase::ITransactionPtr ptrTransaction,
                                         const std::string& sTableName, const std::vector<SField>& vecFields,
                                         CommonLib::eShapeType shapeType, Geometry::ISpatialReferencePtr ptrSpatRef) :
                m_sTableName(sTableName), m_vecFields(vecFields), m_nOIDColumn(-1), m_nShapeColumn(-1), m_ptrSpatRef(ptrSpatRef), m_nRows(0)
        {
            try
            {
                m_extent.type = CommonLib::bbox_type_null;

                GeoDatabase::IFieldsPtr ptrFields = std::make_shared<GeoDatabase::CFields>();
                auto addField = [&ptrFields](const std::string& sName, GeoDatabase::eDataTypes type, bool bPrimaryKey)
                {
                    GeoDatabase::IFieldPtr ptrField = std::make_shared<GeoDatabase::CField>();
                    ptrField->SetName(sName);
                    ptrField->SetType(type);
                    ptrField->SetIsNullable(!bPrimaryKey);
                    ptrField->SetIsPrimaryKey(bPrimaryKey);
                    ptrFields->AddField(ptrField);
                };

                addField(OIDField, GeoDatabase::dtInteger64, true);
                for(size_t i = 0; i < m_vecFields.size(); ++i)
                    addField(m_vecFields[i].sName, m_vecFields[i].type, false);

                bool bSpatial = shapeType != CommonLib::shape_type_null;
                if(bSpatial)
                {
                    addField(ShapeField, GeoDatabase::dtGeometry, false);
                    m_ptrTable = ptrWorkspace->CreateTableWithSpatialIndex(sTableName, sTableName, sTableName + "_sidx", ShapeField, OIDField, ptrFields,
                                                                         shapeType, Geometry::IEnvelopePtr(), ptrSpatRef);
                }
                else
                {
                    m_ptrTable = ptrWorkspace->CreateTable(sTableName, sTableName, ptrFields);
                    m_ptrTable->SetOIDFieldName(OIDField);
                }

                m_ptrInsert = ptrTransaction->CreateInsertCusor(m_ptrTable);

                m_vecColumns.assign(m_vecFields.size(), -1);
                for(int32_t col = 0; col < m_ptrInsert->ColumnCount(); ++col)
                {
                    std::string sName = m_ptrInsert->ColumnName(col);
                    if(sName == OIDField)
                        m_nOIDColumn = col;
                    else if(bSpatial && sName == ShapeField)
                        m_nShapeColumn = col;
                    else
                    {
                        int nField = FindField(sName);
                        if(nField >= 0)
                            m_vecColumns[nField] = col;
                    }
                }

                m_vecValues.resize(m_vecFields.size());
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to create table {0}", sTableName, exc);
                throw;
            }
        }

        int COSMTableWriter::FindField(const std::string& sName) const
        {
            for(size_t i = 0; i < m_vecFields.size(); ++i)
            {
                if(m_vecFields[i].sName == sName)
                    return (int)i;
            }
            return -1;
        }

        void COSMTableWriter::SetInt64(int nField, int64_t nValue)
        {
            if(nField < 0 || nField >= (int)m_vecValues.size())
                return;
            SValue& value = m_vecValues[nField];
            value.bSet = true;
            value.bText = false;
            value.nValue = nValue;
        }

        void COSMTableWriter::SetText(int nField, const std::string& sValue)
        {
            if(nField < 0 || nField >= (int)m_vecValues.size())
                return;
            SValue& value = m_vecValues[nField];
            value.bSet = true;
            value.bText = true;
            value.sValue = sValue;
        }

        void COSMTableWriter::SetShape(CommonLib::IGeoShapePtr ptrShape)
        {
            m_ptrShape = ptrShape;
        }

        void COSMTableWriter::Insert()
        {
            try
            {
                ++m_nRows;
                if(m_nOIDColumn >= 0)
                    m_ptrInsert->BindInt64(m_nOIDColumn, (int64_t)m_nRows);

                for(size_t i = 0; i < m_vecFields.size(); ++i)
                {
                    int32_t col = m_vecColumns[i];
                    SValue& value = m_vecValues[i];
                    if(col >= 0)
                    {
                        if(!value.bSet)
                            m_ptrInsert->BindBlob(col, nullptr, 0, true);   // NULL
                        else if(value.bText)
                            m_ptrInsert->BindText(col, value.sValue, true);
                        else
                            m_ptrInsert->BindInt64(col, value.nValue);
                    }
                    value.bSet = false;
                }

                if(m_nShapeColumn >= 0)
                {
                    if(m_ptrShape.get() && m_ptrCompressor.get() && m_ptrCompressor->Compress(*m_ptrShape, m_vecBlob))
                    {
                        // the box of the compressed shape is the one of the rounded coordinates
                        if(!m_ptrCompressed.get())
                            m_ptrCompressed = std::make_shared<CommonLib::CGeoShape>();
                        m_ptrCompressed->Import(m_vecBlob.data(), (uint32_t)m_vecBlob.size());
                        m_ptrShape = m_ptrCompressed;
                    }
                    if(m_ptrShape.get())
                    {
                        CommonLib::bbox bb = m_ptrShape->GetBB();
                        if(m_extent.type == CommonLib::bbox_type_null)
                            m_extent = bb;
                        else
                        {
                            m_extent.xMin = (std::min)(m_extent.xMin, bb.xMin);
                            m_extent.yMin = (std::min)(m_extent.yMin, bb.yMin);
                            m_extent.xMax = (std::max)(m_extent.xMax, bb.xMax);
                            m_extent.yMax = (std::max)(m_extent.yMax, bb.yMax);
                        }
                        m_extent.type = CommonLib::bbox_type_normal;
                        m_ptrInsert->BindShape(m_nShapeColumn, m_ptrShape, true);
                    }
                    else
                        m_ptrInsert->BindBlob(m_nShapeColumn, nullptr, 0, true);
                }
                m_ptrShape.reset();

                m_ptrInsert->Next();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("Failed to insert a row into {0}", m_sTableName, exc);
                throw;
            }
        }

        void COSMTableWriter::Finish()
        {
            if(m_nShapeColumn < 0 || m_extent.type != CommonLib::bbox_type_normal)
                return;
            m_ptrTable->SetExtent(std::make_shared<Geometry::CEnvelope>(m_extent, m_ptrSpatRef));
        }
    }
}
