#pragma once
#include "Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class CMap : public IMap
        {
        public:
            CMap();
            ~CMap();

            virtual const std::string&     	          GetName() const;
            virtual void                              SetName(const  std::string& name);
            virtual ILayersPtr                        GetLayers() const;
            virtual void							  SelectFeatures(const CommonLib::bbox& extent, bool resetSelection);
            virtual ISelectionPtr                     GetSelection() const;
            virtual Geometry::IEnvelopePtr            GetFullExtent(Geometry::ISpatialReferencePtr ptrSpatRef = nullptr) const;
            virtual void                              SetFullExtent(Geometry::IEnvelopePtr env);
            virtual Geometry::ISpatialReferencePtr    GetSpatialReference() const;
            virtual void                              SetSpatialReference(Geometry::ISpatialReferencePtr  spatRef);
            virtual void                              Draw(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);
            virtual void                              PartialDraw( eDrawPhase phase, Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);
            virtual ILabelEnginePtr                   GetLabelEngine() const;
            virtual void                              SetLabelEngine(ILabelEnginePtr engine);
            virtual  CommonLib::Units		          GetMapUnits() const;
            virtual void                              SetMapUnits( CommonLib::Units units );
            virtual IGraphicsContainerPtr             GetGraphicsContainer() const;
            virtual void                              SetDelayDrawing(bool delay);
            virtual IBookmarksPtr                     GetBookmarks() const;
            virtual CommonLib::IPropertySetPtr        GetMapProperties();
            virtual Display::IFillSymbolPtr			  GetBackgroundSymbol() const;
            virtual void							  SetBackgroundSymbol(Display::IFillSymbolPtr ptrSymbol);
            virtual Display::IFillSymbolPtr			  GetForegroundSymbol() const;
            virtual void							  SetForegroundSymbol(Display::IFillSymbolPtr symbol);
            virtual void							  SetViewPos(const Display::ViewPosition& pos);
            virtual Display::ViewPosition			  GetViewPos(bool calc_if_absent, Display::IDisplayTransformationPtr pTrans);
            virtual void							  SetExtent(Geometry::IEnvelopePtr ptrExtent);
            virtual Geometry::IEnvelopePtr		      GetExtent(Geometry::ISpatialReferencePtr ptrSpatRef, bool calc_if_absent, Display::IDisplayTransformationPtr ptrTrans);
            virtual void							  SetVerticalFlip(bool flag);
            virtual bool							  GetVerticalFlip() const;
            virtual void							  SetHorizontalFlip(bool flag);
            virtual bool							  GetHorizontalFlip() const;

            virtual double							  GetMinimumScale();
            virtual void							  SetMinimumScale(double scale);
            virtual double							  GetMaximumScale();
            virtual void							  SetMaximumScale(double scale);
            virtual bool							  GetHasReferenceScale() const;
            virtual void							  SetHasReferenceScale(bool flag);
            virtual double							  GetReferenceScale() const;
            virtual void							  SetReferenceScale(double scale);

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:

            std::string m_sName;
            ILayersPtr m_ptrLayers;
            ISelectionPtr m_ptrSelection;
            Geometry::ISpatialReferencePtr	m_ptrSpatialRef;
            double m_dMinScale;
            double m_dMaxScale;
            double m_dReferenceScale;
            CommonLib::Units	m_MapUnits;
            bool m_bFlipVertical;
            bool m_bflipHorizontal;
            mutable Geometry::IEnvelopePtr m_ptrFullExtent;
            Geometry::IEnvelopePtr m_ptrExtent;
            Display::ViewPosition  m_ViewPos;
            bool m_bViewPos;
            bool m_bHasReferenceScale;
            bool m_bDelayDrawing;

            Display::IFillSymbolPtr     m_ptrBackgroundSymbol;
            Display::IFillSymbolPtr		m_ptrForegroundSymbol;
            ILabelEnginePtr				m_ptrLabelEngine;
            IGraphicsContainerPtr		m_ptrGraphicsContainer;
            IBookmarksPtr				m_ptrBookmarks;
            CommonLib::IPropertySetPtr	m_ptrPropertySet;
            bool m_bCalcBB;

        };

    }
    }