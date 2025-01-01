#pragma once

#include "MarkerSymbolBase.h"

namespace GraphEngine {
    namespace Display {
        class CSimpleMarketSymbol : public CMarkerSymbolBase<ISimpleMarkerSymbol>
        {
        public:

            typedef CMarkerSymbolBase<ISimpleMarkerSymbol> TSymbolBase;

            CSimpleMarketSymbol();
            virtual  ~CSimpleMarketSymbol();
            virtual void                 SetStyle(eSimpleMarkerStyle style);
            virtual void                 SetOutline(bool bOutline);
            virtual bool                 IsOutline() const;
            virtual Color                GetOutlineColor() const;
            virtual void                 SetOutlineColor( const Color &color);
            virtual double               GetOutlineSize() const;
            virtual void                 SetOutlineSize(double size);

            virtual bool CanDraw(CommonLib::IGeoShapePtr ptrShape) const;

            //ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);


            virtual void  DrawGeometryEx(IDisplayPtr pDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void  QueryBoundaryRectEx(IDisplayPtr pDisplay, const GPoint* points, const int* polyCounts, int polyCount,   GRect &rect) const;
            virtual void  Prepare(IDisplayPtr pDisplay);
            virtual void DrawDirectly(IDisplayPtr display, const GPoint* lpPoints, const int *lpPolyCounts, int nCount );

        private:
            void DrawCircle(const GPoint& point, IGraphicsPtr pGraphics);
            void DrawSquare(const GPoint& point, IGraphicsPtr pGraphics);
            void DrawDiamond(const GPoint& point, IGraphicsPtr pGraphics);
            void DrawX(const GPoint& point, IGraphicsPtr pGraphics);
            void DrawCross(const GPoint& point, IGraphicsPtr pGraphics);
        private:

            eSimpleMarkerStyle m_style;
            bool              m_bIsOutline;
            Color             m_OutlineColor;
            double            m_dOutlineSize;

            PenPtr  m_ptrPen;
            PenPtr   m_ptrOutlinePen;
            BrushPtr  m_ptrBrush;
        };



    }
    }
