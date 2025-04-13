#pragma once
#include "../Cartography.h"
#include "LayerBase.h"


namespace GraphEngine {
    namespace Cartography {

        class  CFeatureLayer : public CLayerBase<IFeatureLayer>
        {
        public:

            typedef CLayerBase<IFeatureLayer> TBase;
            CFeatureLayer();
            ~CFeatureLayer();

            virtual Geometry::IEnvelopePtr GetExtent() const;
            virtual eDrawPhase                GetSupportedDrawPhases() const;
            virtual bool                      IsValid() const;
            virtual bool                      IsActiveOnScale(double scale) const;

            // IFeatureLayer

            virtual const std::string&			     GetDefinitionQuery() const;
            virtual void							 SetDefinitionQuery(const std::string& );
            virtual void                             SetJoins(const std::vector<GeoDatabase::IJoinPtr>& joins);
            virtual const std::string&               GetDisplayField() const;
            virtual void                             SetDisplayField(const std::string& sField);
            virtual const std::string&               GetOIDField() const;
            virtual void                             SetOIDField(const  std::string& sField);
            virtual const std::string&               GetShapeField() const;
            virtual void                             SetShapeField(const  std::string& sField);
            virtual GeoDatabase::ITablePtr           GetLayerTable() const;
            virtual void                             SetLayerTable(GeoDatabase::ITablePtr ptrTable);
            virtual bool                             GetSelectable() const;
            virtual void                             SetSelectable(bool flag);
            virtual int								 GetRendererCount() const;
            virtual IFeatureRendererPtr				 GetRenderer(int index) const;
            virtual void							 AddRenderer(IFeatureRendererPtr ptrRenderer);
            virtual void							 RemoveRenderer(IFeatureRendererPtr	 ptrRenderer);
            virtual void							 ClearRenders();
            virtual void                             DrawFeatures(eDrawPhase phase, const std::vector<int64_t>& vecOids, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel, Display::ISymbolPtr ptrCustomSymbol) const;



            void DrawEx(eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr trackCancel);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);


            virtual void  SelectFeatures(const CommonLib::bbox& extent, ISelectionPtr ptrSelection,  Geometry::ISpatialReferencePtr ptrSpRef);
        private:
            void CalcBB(Display::IDisplayPtr ptrDisplay, CommonLib::bbox& bb);
        private:
            typedef std::vector<IFeatureRendererPtr> TFeatureRenderer;

            std::string  m_sDisplayField;
            std::string   m_sQuery;
            std::string  m_sOIDField;
            std::string  m_sShapeField;
            TFeatureRenderer m_vecRenderers;
            bool m_bSelectable;
            bool m_hasReferenceScale;
            GeoDatabase::ITablePtr m_ptrTable;

            double                        m_dDrawingWidth;
            bool                          m_bDrawingWidthScaleDependent;

            std::vector<GeoDatabase::IJoinPtr> m_vecJoins;

        };


    }

    }