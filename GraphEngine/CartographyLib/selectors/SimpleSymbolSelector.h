#pragma once
#include "../Cartography.h"

namespace GraphEngine {
    namespace Cartography {

        class CSimpleSymbolSelector : public ISimpleSymbolSelector, public ILegendInfo, public std::enable_shared_from_this<CSimpleSymbolSelector>
        {
        public:
            CSimpleSymbolSelector();
            CSimpleSymbolSelector(Display::ISymbolPtr ptrSymbol);
            virtual ~CSimpleSymbolSelector();

        private:
            CSimpleSymbolSelector(const CSimpleSymbolSelector&);
            CSimpleSymbolSelector& operator=(const CSimpleSymbolSelector&);

        public:
            // ISymbolSelector
            virtual uint32_t               GetSymbolSelectorID() const {return SimpleSymbolSelectorID;}
            virtual bool                   CanAssign(GeoDatabase::ITablePtr ptrTable) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const;
            virtual void                   SetupSymbols(Display::IDisplayPtr ptrDisplay);
            virtual void                   ResetSymbols();
            virtual void                   FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

            // ISimpleSymbolSelector
            virtual const std::string&     GetDescription() const;
            virtual void                   SetDescription(const std::string& sDesc);
            virtual const std::string&     GetLabel() const;
            virtual void                   SetLabel(const std::string& sLabel);
            virtual Display::ISymbolPtr    GetSymbol() const;
            virtual void                   SetSymbol(Display::ISymbolPtr ptrSymbol);

            // ILegendInfo: one group with one class
            virtual int                    GetLegendGroupCount() const;
            virtual ILegendGroupPtr        GetLegendGroup(int nIndex) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            std::string          m_sLabel;
            std::string          m_sDescription;
            Display::ISymbolPtr  m_ptrSymbol;
        };

    }
}
