#pragma once

#include "SymbolFillBase.h"


namespace GraphEngine {
    namespace Display {

        class CSimpleFillSymbol : public CSymbolFillBase<ISimpleFillSymbol>
        {
        public:


            typedef CSymbolFillBase<ISimpleFillSymbol> TSymbolBase;

            CSimpleFillSymbol();
            virtual ~CSimpleFillSymbol();


            //ISimpleFillSymbol
            virtual eSimpleFillStyle GetStyle() const;
            virtual void SetStyle(eSimpleFillStyle style);
            virtual Color GetBackgroundColor() const;
            virtual void  SetBackgroundColor( Color color );

            virtual void  DrawGeometryEx(IDisplayPtr pDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void  QueryBoundaryRectEx(IDisplayPtr pDisplay, const GPoint* points, const int* polyCounts, int polyCount,   GRect &rect) const;
            virtual void  Prepare(IDisplayPtr pDisplay);

            virtual void FillRect(IDisplayPtr pDisplay, const Display::GRect& rect);
            virtual void DrawDirectly(IDisplayPtr display, const GPoint* lpPoints, const int *lpPolyCounts, int nCount );

            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        public:
            PenPtr m_ptrPen;
            eSimpleFillStyle m_FillStyle;
            BitmapPtr m_ptrBitmap;


        };


    }
    }
