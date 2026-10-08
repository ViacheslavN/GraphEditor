#pragma once
#include "SymbolSelectorUtils.h"

namespace GraphEngine {
    namespace Cartography {

        // symbol by the range of a numeric field value (ported from UniGIS RangeSymbolAssigner, without ILegendInfo)
        class CRangeSymbolSelector : public IRangeSymbolSelector
        {
        public:
            CRangeSymbolSelector();
            explicit CRangeSymbolSelector(const std::string& sField);
            virtual ~CRangeSymbolSelector();

        private:
            CRangeSymbolSelector(const CRangeSymbolSelector&);
            CRangeSymbolSelector& operator=(const CRangeSymbolSelector&);

        public:
            // ISymbolSelector
            virtual uint32_t               GetSymbolSelectorID() const {return RangeSymbolSelectorID;}
            virtual bool                   CanAssign(GeoDatabase::ITablePtr ptrTable) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const;
            virtual void                   SetupSymbols(Display::IDisplayPtr ptrDisplay);
            virtual void                   ResetSymbols();
            virtual void                   FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

            // IRangeSymbolSelector
            virtual const std::string&     GetField() const;
            virtual void                   SetField(const std::string& sFieldName);
            virtual int                    GetRangeCount() const;
            virtual void                   SetRangeCount(int nCount);
            virtual int                    AddRange(double dFrom, double dTo, Display::ISymbolPtr ptrSymbol, const std::string& sLabel = std::string());
            virtual void                   RemoveRange(int nIndex);
            virtual void                   GetRange(int nIndex, double* pFrom, double* pTo) const;
            virtual void                   SetRange(int nIndex, double dFrom, double dTo);
            virtual const std::string&     GetLabel(int nIndex) const;
            virtual void                   SetLabel(int nIndex, const std::string& sLabel);
            virtual const std::string&     GetDescription(int nIndex) const;
            virtual void                   SetDescription(int nIndex, const std::string& sDescription);
            virtual Display::ISymbolPtr    GetSymbol(int nIndex) const;
            virtual void                   SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol);
            virtual void                   SortRanges();
            virtual Display::ISymbolPtr    GetDefaultSymbol() const;
            virtual void                   SetDefaultSymbol(Display::ISymbolPtr ptrSymbol);
            virtual const std::string&     GetDefaultLabel() const;
            virtual void                   SetDefaultLabel(const std::string& sLabel);
            virtual bool                   GetUseDefaultSymbol() const;
            virtual void                   SetUseDefaultSymbol(bool bUse);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            struct SRangeEntry
            {
                double              dFrom = 0.;
                double              dTo = 0.;
                std::string         sLabel;
                std::string         sDescription;
                Display::ISymbolPtr ptrSymbol;
            };

            void CheckIndex(int nIndex) const;
            Display::ISymbolPtr DefaultSymbol() const;

            template<class F>
            void ForEachSymbol(F func) const
            {
                for(size_t i = 0; i < m_vecRanges.size(); ++i)
                    if(m_vecRanges[i].ptrSymbol.get())
                        func(m_vecRanges[i].ptrSymbol);
                if(m_ptrDefaultSymbol.get())
                    func(m_ptrDefaultSymbol);
            }

        private:
            std::string              m_sField;
            std::vector<SRangeEntry> m_vecRanges;
            Display::ISymbolPtr      m_ptrDefaultSymbol;
            std::string              m_sDefaultLabel;
            bool                     m_bUseDefaultSymbol;
            mutable int32_t          m_nFieldColumn;   // reset by PrepareFilter
        };

    }
}
