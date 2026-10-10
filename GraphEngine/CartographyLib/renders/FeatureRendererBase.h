#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        template< class I>
        class CFeatureRendererBase : public I
        {
        public:
            CFeatureRendererBase() : m_dMinimumScale(0.), m_dMaximumScale(0.), m_nShapeFieldIndex(-1),
                                     m_nFeatureRendererID(UndefineFeatureRendererID), m_bShowInLegend(true)
            {}

            virtual ~CFeatureRendererBase()
            {}

            virtual uint32_t GetFeatureRendererID()  const
            {
                return m_nFeatureRendererID;
            }

            virtual double GetMaximumScale() const
            {
                return m_dMaximumScale;
            }

            virtual void SetMaximumScale(double scale)
            {
                m_dMaximumScale = scale;
            }

            virtual double GetMinimumScale() const
            {
                return m_dMinimumScale;
            }

            virtual void SetMinimumScale(double scale)
            {
                m_dMinimumScale = scale;
            }

            virtual const std::string& GetShapeField() const
            {
                return m_sShapeField;
            }

            virtual void SetShapeField(const std::string& field)
            {
                m_sShapeField = field;
                m_nShapeFieldIndex = -1;
            }

            virtual bool GetShowInLegend() const
            {
                return m_bShowInLegend;
            }

            virtual void SetShowInLegend(bool bShow)
            {
                m_bShowInLegend = bShow;
            }

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const
            {
                pObj->AddPropertyInt32U("FeatureRendererID", GetFeatureRendererID());
                pObj->AddPropertyDouble("MinScale", m_dMinimumScale);
                pObj->AddPropertyDouble("MaxScale", m_dMaximumScale);
                pObj->AddPropertyString("ShapeField", m_sShapeField);
                pObj->AddPropertyBool("ShowInLegend", m_bShowInLegend);
            }

            virtual void Load(CommonLib::ISerializeObjPtr pObj)
            {
                m_dMinimumScale = pObj->GetPropertyDouble("MinScale", m_dMinimumScale);
                m_dMaximumScale = pObj->GetPropertyDouble("MaxScale", m_dMaximumScale);
                m_sShapeField = pObj->GetPropertyString("ShapeField", m_sShapeField);
                m_bShowInLegend = pObj->GetPropertyBool("ShowInLegend", true);
                m_nShapeFieldIndex = -1;
            }

        protected:
            // resolves m_sShapeField (empty - table shape field, digit - index among geometry fields) and adds it to the filter
            void PrepareShapeField(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
            {
                if(m_sShapeField.empty())
                {
                    m_sShapeField = ptrTable->GetShapeFieldName();
                }
                else if(isdigit((unsigned char)m_sShapeField[0]))
                {
                    // shape field is set as index among geometry fields ("0" - first geometry field, ...)
                    int nShapeIndex = atoi(m_sShapeField.c_str());
                    GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
                    int nShape = -1;
                    for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
                    {
                        GeoDatabase::IFieldPtr ptrField = ptrFields->GetField(i);
                        if(ptrField->GetType() == GeoDatabase::dtGeometry)
                            ++nShape;

                        if(nShape == nShapeIndex)
                        {
                            m_sShapeField = ptrField->GetName();
                            break;
                        }
                    }
                }

                if(ptrFilter->GetFieldSet()->Find(m_sShapeField) < 0)
                    ptrFilter->GetFieldSet()->Add(m_sShapeField);

                m_nShapeFieldIndex = -1; // column index is resolved on the cursor row, see GetShape
            }

            CommonLib::IGeoShapePtr GetShape(GeoDatabase::IRowPtr ptrRow) const
            {
                int32_t nColumns = ptrRow->ColumnCount();
                if(m_nShapeFieldIndex < 0 || m_nShapeFieldIndex >= nColumns || ptrRow->GetColumnType(m_nShapeFieldIndex) != GeoDatabase::dtGeometry)
                {
                    m_nShapeFieldIndex = -1;
                    for(int32_t i = 0; i < nColumns; ++i)
                    {
                        if(ptrRow->GetColumnType(i) != GeoDatabase::dtGeometry)
                            continue;

                        if(m_sShapeField.empty() || ptrRow->ColumnName(i) == m_sShapeField)
                        {
                            m_nShapeFieldIndex = i;
                            break;
                        }

                        if(m_nShapeFieldIndex < 0)
                            m_nShapeFieldIndex = i; // fallback: first geometry column
                    }

                    if(m_nShapeFieldIndex < 0)
                        throw CommonLib::CExcBase("FeatureRenderer: shape field {0} not found", m_sShapeField);
                }

                if(ptrRow->ColumnIsNull(m_nShapeFieldIndex))
                    return CommonLib::IGeoShapePtr();

                CommonLib::CVariantPtr ptrVal = ptrRow->GetValue(m_nShapeFieldIndex);
                if(!ptrVal.get() || !ptrVal->IsType<CommonLib::IGeoShapePtr>())
                    return CommonLib::IGeoShapePtr();

                return ptrVal->Get<CommonLib::IGeoShapePtr>();
            }

        protected:
            double                    m_dMinimumScale;
            double                    m_dMaximumScale;
            mutable std::string       m_sShapeField;
            mutable int32_t           m_nShapeFieldIndex;
            uint32_t                  m_nFeatureRendererID;
            bool                      m_bShowInLegend;
        };

    }
}
