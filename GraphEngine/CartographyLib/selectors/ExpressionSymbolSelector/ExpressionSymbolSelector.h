#pragma once
#include "../SymbolSelectorUtils.h"

namespace GraphEngine {
    namespace Cartography {

        // symbol by SQL expressions (ported from UniGIS ExpressionSymbolAssigner): the expressions are checked in order,
        // the first one true for the row gives the symbol; none - the default symbol (if used).
        //
        // An expression is a WHERE condition (GeoDatabase ISQLRowFilter): POP > 1000 AND NAME LIKE 'A%'. An empty one is
        // true for every row. Fields of a join of the layer are qualified by the prefix of the join: t2.TYPE = 'road';
        // PrepareFilter adds them to the field set of that join, so the cursor returns them (as the column "t2.TYPE").
        // A wrong expression is refused by SetExpression; loaded from a document it is kept, but never matches.
        class CExpressionSymbolSelector : public IExpressionSymbolSelector, public ILegendInfo,
                                          public std::enable_shared_from_this<CExpressionSymbolSelector>
        {
        public:
            CExpressionSymbolSelector();
            virtual ~CExpressionSymbolSelector();

        private:
            CExpressionSymbolSelector(const CExpressionSymbolSelector&);
            CExpressionSymbolSelector& operator=(const CExpressionSymbolSelector&);

        public:
            // ISymbolSelector
            virtual uint32_t               GetSymbolSelectorID() const {return ExpressionSymbolSelectorID;}
            virtual bool                   CanAssign(GeoDatabase::ITablePtr ptrTable) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const;
            virtual void                   SetupSymbols(Display::IDisplayPtr ptrDisplay);
            virtual void                   ResetSymbols();
            virtual void                   FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

            // IExpressionSymbolSelector
            virtual const std::string&     GetExpression(int nIndex) const;
            virtual void                   SetExpression(int nIndex, const std::string& sExpr);
            virtual int                    GetExpressionCount() const;
            virtual void                   SetExpressionCount(int nCount);
            virtual const std::string&     GetLabel(int nIndex) const;
            virtual void                   SetLabel(int nIndex, const std::string& sLabel);
            virtual const std::string&     GetDescription(int nIndex) const;
            virtual void                   SetDescription(int nIndex, const std::string& sDescription);
            virtual Display::ISymbolPtr    GetSymbol(int nIndex) const;
            virtual void                   SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol);
            virtual Display::ISymbolPtr    GetDefaultSymbol() const;
            virtual void                   SetDefaultSymbol(Display::ISymbolPtr ptrSymbol);
            virtual const std::string&     GetDefaultLabel() const;
            virtual void                   SetDefaultLabel(const std::string& sLabel);
            virtual bool                   GetUseDefaultSymbol() const;
            virtual void                   SetUseDefaultSymbol(bool bUse);

            // ILegendInfo: one group, a class per expression, the default symbol is the last class
            virtual int                    GetLegendGroupCount() const;
            virtual ILegendGroupPtr        GetLegendGroup(int nIndex) const;

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

        private:
            struct SExpressionEntry
            {
                std::string                   sExpression;
                std::string                   sLabel;
                std::string                   sDescription;
                Display::ISymbolPtr           ptrSymbol;
                GeoDatabase::ISQLRowFilterPtr ptrFilter;        // empty for an empty or a wrong expression
                bool                          bValid = true;    // false - a wrong expression (loaded): never matches
                mutable bool                  bFailed = false;  // evaluation failed (f.e. no field in the rows), reset by PrepareFilter
            };

            void CheckIndex(int nIndex) const;
            Display::ISymbolPtr DefaultSymbol() const;
            static void ParseExpression(SExpressionEntry& entry, const std::string& sExpr, bool bThrow);

            template<class F>
            void ForEachSymbol(F func) const
            {
                for(size_t i = 0; i < m_vecEntries.size(); ++i)
                    if(m_vecEntries[i].ptrSymbol.get())
                        func(m_vecEntries[i].ptrSymbol);
                if(m_ptrDefaultSymbol.get())
                    func(m_ptrDefaultSymbol);
            }

        private:
            std::vector<SExpressionEntry> m_vecEntries;
            Display::ISymbolPtr           m_ptrDefaultSymbol;
            std::string                   m_sDefaultLabel;
            bool                          m_bUseDefaultSymbol;
        };

    }
}
