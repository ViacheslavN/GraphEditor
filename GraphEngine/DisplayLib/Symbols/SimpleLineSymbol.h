#pragma once

#include "SymbolBase.h"

namespace GraphEngine {
    namespace Display {

        class CSimpleLineSymbol : public CSymbolBase<ISimpleLineSymbol>
        {
        public:

            typedef CSymbolBase<ISimpleLineSymbol> TBase;

            CSimpleLineSymbol();
            CSimpleLineSymbol( const Color &color, double width, eSimpleLineStyle style = SimpleLineStyleSolid );
            virtual ~CSimpleLineSymbol();

            virtual void DrawDirectly(IDisplayPtr ptrDisplay, const GPoint* lpPoints, const int *lpPolyCounts, int nCount );
            //CSymbol
            virtual void  DrawGeometryEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount);
            virtual void  QueryBoundaryRectEx(IDisplayPtr ptrDisplay, const GPoint* points, const int* polyCounts, int polyCount,  GRect &rect) const;
            // ScaleDependent: the width and the dashes are multiplied by reference scale / scale
            virtual void  Prepare(IDisplayPtr ptrDisplay);
            //ILineSymbol
            virtual Color  GetColor() const ;
            virtual void   SetColor(const Color &color) ;
            virtual double GetWidth() const ;
            virtual void   SetWidth(double width) ;

            //ISimpleLineSymbol
            virtual ePenType			GetStyle() const ;
            virtual void                SetStyle( ePenType style ) ;
            virtual eCapType             GetCapType() const ;
            virtual void                SetCapType( eCapType cap ) ;
            virtual eJoinType            GetJoinType() const ;
            virtual void                SetJoinType( eJoinType join ) ;

            // custom dash pattern in pixels (dash, gap pairs) - replaces the pattern of the style, saved with the symbol
            void                        AddDash(double dDash, double dGap);
            void                        ClearDashes();
            const TPenTemplates&        GetDashes() const;



            //ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:

            const PenPtr& DrawPen() const { return m_ptrDevicePen ? m_ptrDevicePen : m_ptrPen; }
            void Changed();
            void Draw( IGraphicsPtr ptrGraphics, IDisplayTransformationPtr pDT, CommonLib::GisXYPoint *pPt, uint32_t nCnt);
        private:
            PenPtr m_ptrPen;
            PenPtr m_ptrDevicePen;   // pen with the scaled width / dashes (Prepare), null - m_ptrPen is used



        };



    }
}