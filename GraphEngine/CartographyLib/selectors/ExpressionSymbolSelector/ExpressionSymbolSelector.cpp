#include "ExpressionSymbolSelector.h"
#include "../../legend/Legend.h"
#include "../../../GeoDatabase/FieldSet.h"
#include "../../../GeoDatabase/SQLRowFilter/SQLRowFilter.h"

namespace GraphEngine {
    namespace Cartography {

        namespace
        {
            bool EqualNoCase(const std::string& a, const std::string& b)
            {
                if(a.size() != b.size())
                    return false;
                for(size_t i = 0; i < a.size(); ++i)
                {
                    char ca = a[i], cb = b[i];
                    if(ca >= 'A' && ca <= 'Z') ca = (char)(ca - 'A' + 'a');
                    if(cb >= 'A' && cb <= 'Z') cb = (char)(cb - 'A' + 'a');
                    if(ca != cb)
                        return false;
                }
                return true;
            }

            // the name of the field in the table as it is written there (SQL names ignore case), empty - no such field
            std::string FindTableField(GeoDatabase::ITablePtr ptrTable, const std::string& sName)
            {
                GeoDatabase::IFieldsPtr ptrFields = ptrTable.get() ? ptrTable->GetFields() : GeoDatabase::IFieldsPtr();
                if(!ptrFields.get())
                    return std::string();

                for(int i = 0, sz = ptrFields->GetFieldCount(); i < sz; ++i)
                {
                    const std::string& sField = ptrFields->GetField(i)->GetName();
                    if(EqualNoCase(sField, sName))
                        return sField;
                }
                return std::string();
            }

            void AddField(GeoDatabase::IFieldSetPtr ptrFieldSet, const std::string& sField)
            {
                for(int i = 0, sz = ptrFieldSet->GetCount(); i < sz; ++i)
                    if(EqualNoCase(ptrFieldSet->Get(i), sField))
                        return;
                ptrFieldSet->Add(sField);
            }
        }

        CExpressionSymbolSelector::CExpressionSymbolSelector() : m_sDefaultLabel("<all other values>"), m_bUseDefaultSymbol(true)
        {

        }

        CExpressionSymbolSelector::~CExpressionSymbolSelector()
        {

        }

        void CExpressionSymbolSelector::CheckIndex(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecEntries.size())
                throw CommonLib::CExcBase("ExpressionSymbolSelector: expression index out of range: {0}", nIndex);
        }

        Display::ISymbolPtr CExpressionSymbolSelector::DefaultSymbol() const
        {
            return m_bUseDefaultSymbol ? m_ptrDefaultSymbol : Display::ISymbolPtr();
        }

        void CExpressionSymbolSelector::ParseExpression(SExpressionEntry& entry, const std::string& sExpr, bool bThrow)
        {
            GeoDatabase::ISQLRowFilterPtr ptrFilter;
            bool bValid = true;
            if(sExpr.find_first_not_of(" \t\r\n") != std::string::npos)
            {
                try
                {
                    ptrFilter = GeoDatabase::CSQLRowFilter::Create(sExpr);
                }
                catch (std::exception& exc)
                {
                    if(bThrow)
                    {
                        CommonLib::CExcBase::RegenExc("ExpressionSymbolSelector: wrong expression {0}", sExpr, exc);
                        throw;
                    }
                    bValid = false;
                }
            }

            entry.sExpression = sExpr;
            entry.ptrFilter = ptrFilter;
            entry.bValid = bValid;
            entry.bFailed = false;
        }

        // ISymbolSelector

        bool CExpressionSymbolSelector::CanAssign(GeoDatabase::ITablePtr ptrTable) const
        {
            if(!ptrTable.get())
                return false;

            // the fields without a prefix are of the table; the prefixed ones may be of a join, they are checked on the rows
            for(size_t i = 0; i < m_vecEntries.size(); ++i)
            {
                GeoDatabase::ISQLRowFilterPtr ptrFilter = m_vecEntries[i].ptrFilter;
                if(!ptrFilter.get())
                    continue;

                for(int f = 0, sz = ptrFilter->GetFieldCount(); f < sz; ++f)
                {
                    if(ptrFilter->GetFieldPrefix(f).empty() && FindTableField(ptrTable, ptrFilter->GetFieldName(f)).empty())
                        return false;
                }
            }
            return true;
        }

        void CExpressionSymbolSelector::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            for(size_t i = 0; i < m_vecEntries.size(); ++i)
                m_vecEntries[i].bFailed = false;

            if(!ptrFilter.get() || !ptrTable.get())
                return;

            const std::string& sMainPrefix = ptrFilter->GetTablePrefix();
            const std::vector<GeoDatabase::IJoinPtr> vecJoins = ptrFilter->GetJoins();
            GeoDatabase::IFieldSetPtr ptrMainFields = ptrFilter->GetFieldSet();

            for(size_t i = 0; i < m_vecEntries.size(); ++i)
            {
                GeoDatabase::ISQLRowFilterPtr ptrRowFilter = m_vecEntries[i].ptrFilter;
                if(!ptrRowFilter.get())
                    continue;

                for(int f = 0, sz = ptrRowFilter->GetFieldCount(); f < sz; ++f)
                {
                    const std::string& sPrefix = ptrRowFilter->GetFieldPrefix(f);
                    const std::string& sName = ptrRowFilter->GetFieldName(f);

                    if(sPrefix.empty() || EqualNoCase(sPrefix, sMainPrefix))
                    {
                        // added even to an empty field set, as the other selectors do: the layer fills it in parts
                        // (the renderers, then the OID), so an empty set here doesn't mean "all fields"
                        std::string sField = FindTableField(ptrTable, sName);
                        if(ptrMainFields.get() && !sField.empty())
                            AddField(ptrMainFields, sField);
                        continue;
                    }

                    // a field of a join: an empty field set of a join selects nothing, so the field is always added
                    for(size_t j = 0; j < vecJoins.size(); ++j)
                    {
                        GeoDatabase::IJoinPtr ptrJoin = vecJoins[j];
                        if(!ptrJoin.get() || !EqualNoCase(ptrJoin->GetTablePrefix(), sPrefix))
                            continue;

                        GeoDatabase::IFieldSetPtr ptrJoinFields = ptrJoin->GetFieldSet();
                        if(!ptrJoinFields.get())
                        {
                            ptrJoinFields = std::make_shared<GeoDatabase::CFieldSet>();
                            ptrJoin->SetFieldSet(ptrJoinFields);
                        }
                        AddField(ptrJoinFields, sName);
                        break;
                    }
                }
            }
        }

        Display::ISymbolPtr CExpressionSymbolSelector::GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!ptrRow.get())
                return DefaultSymbol();

            for(size_t i = 0; i < m_vecEntries.size(); ++i)
            {
                const SExpressionEntry& entry = m_vecEntries[i];
                if(!entry.bValid || entry.bFailed)
                    continue;
                if(!entry.ptrFilter.get())
                    return entry.ptrSymbol;   // an empty expression: every row

                try
                {
                    if(entry.ptrFilter->EvaluateRow(ptrRow))
                        return entry.ptrSymbol;
                }
                catch (std::exception&)
                {
                    // a field isn't in the rows of this cursor: the expression is off until the next PrepareFilter
                    entry.bFailed = true;
                }
            }
            return DefaultSymbol();
        }

        void CExpressionSymbolSelector::SetupSymbols(Display::IDisplayPtr ptrDisplay)
        {
            if(!ptrDisplay.get())
                return;
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Prepare(ptrDisplay); });
        }

        void CExpressionSymbolSelector::ResetSymbols()
        {
            ForEachSymbol([](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Reset(); });
        }

        void CExpressionSymbolSelector::FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->FlushBuffers(ptrDisplay, ptrTrackCancel); });
        }

        // IExpressionSymbolSelector

        const std::string& CExpressionSymbolSelector::GetExpression(int nIndex) const
        {
            CheckIndex(nIndex);
            return m_vecEntries[nIndex].sExpression;
        }

        void CExpressionSymbolSelector::SetExpression(int nIndex, const std::string& sExpr)
        {
            CheckIndex(nIndex);
            SExpressionEntry entry = m_vecEntries[nIndex];
            ParseExpression(entry, sExpr, true);   // throws on a wrong expression, the old one stays
            m_vecEntries[nIndex] = entry;
        }

        int CExpressionSymbolSelector::GetExpressionCount() const
        {
            return (int)m_vecEntries.size();
        }

        void CExpressionSymbolSelector::SetExpressionCount(int nCount)
        {
            if(nCount < 0)
                throw CommonLib::CExcBase("ExpressionSymbolSelector: wrong expression count: {0}", nCount);
            m_vecEntries.resize(nCount);
        }

        const std::string& CExpressionSymbolSelector::GetLabel(int nIndex) const { CheckIndex(nIndex); return m_vecEntries[nIndex].sLabel; }
        void CExpressionSymbolSelector::SetLabel(int nIndex, const std::string& sLabel) { CheckIndex(nIndex); m_vecEntries[nIndex].sLabel = sLabel; }
        const std::string& CExpressionSymbolSelector::GetDescription(int nIndex) const { CheckIndex(nIndex); return m_vecEntries[nIndex].sDescription; }
        void CExpressionSymbolSelector::SetDescription(int nIndex, const std::string& sDescription) { CheckIndex(nIndex); m_vecEntries[nIndex].sDescription = sDescription; }
        Display::ISymbolPtr CExpressionSymbolSelector::GetSymbol(int nIndex) const { CheckIndex(nIndex); return m_vecEntries[nIndex].ptrSymbol; }
        void CExpressionSymbolSelector::SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol) { CheckIndex(nIndex); m_vecEntries[nIndex].ptrSymbol = ptrSymbol; }

        Display::ISymbolPtr CExpressionSymbolSelector::GetDefaultSymbol() const { return m_ptrDefaultSymbol; }
        void CExpressionSymbolSelector::SetDefaultSymbol(Display::ISymbolPtr ptrSymbol) { m_ptrDefaultSymbol = ptrSymbol; }
        const std::string& CExpressionSymbolSelector::GetDefaultLabel() const { return m_sDefaultLabel; }
        void CExpressionSymbolSelector::SetDefaultLabel(const std::string& sLabel) { m_sDefaultLabel = sLabel; }
        bool CExpressionSymbolSelector::GetUseDefaultSymbol() const { return m_bUseDefaultSymbol; }
        void CExpressionSymbolSelector::SetUseDefaultSymbol(bool bUse) { m_bUseDefaultSymbol = bUse; }

        // ILegendInfo

        int CExpressionSymbolSelector::GetLegendGroupCount() const
        {
            return 1;
        }

        ILegendGroupPtr CExpressionSymbolSelector::GetLegendGroup(int nIndex) const
        {
            if(nIndex != 0)
                throw CommonLib::CExcBase("ExpressionSymbolSelector: legend group index out of range: {0}", nIndex);

            std::weak_ptr<CExpressionSymbolSelector> wSelf = std::const_pointer_cast<CExpressionSymbolSelector>(weak_from_this().lock());
            CLegendGroupPtr ptrGroup = std::make_shared<CLegendGroup>(std::string());
            for(int i = 0; i < (int)m_vecEntries.size(); ++i)
            {
                const SExpressionEntry& entry = m_vecEntries[i];
                std::string sLabel = !entry.sLabel.empty() ? entry.sLabel :
                                     (!entry.sExpression.empty() ? entry.sExpression : std::string("<all features>"));

                CLegendClass::TSymbolWriter writer = CLegendUtils::MakeSymbolWriter(wSelf, entry.ptrSymbol,
                        [i](CExpressionSymbolSelector& selector) { return i < selector.GetExpressionCount() ? selector.GetSymbol(i) : Display::ISymbolPtr(); },
                        [i](CExpressionSymbolSelector& selector, Display::ISymbolPtr ptrSymbol) { selector.SetSymbol(i, ptrSymbol); });
                ptrGroup->AddClass(std::make_shared<CLegendClass>(sLabel, entry.ptrSymbol, entry.sDescription, writer));
            }

            if(m_bUseDefaultSymbol && m_ptrDefaultSymbol.get())
            {
                CLegendClass::TSymbolWriter writer = CLegendUtils::MakeSymbolWriter(wSelf, m_ptrDefaultSymbol,
                        [](CExpressionSymbolSelector& selector) { return selector.GetDefaultSymbol(); },
                        [](CExpressionSymbolSelector& selector, Display::ISymbolPtr ptrSymbol) { selector.SetDefaultSymbol(ptrSymbol); });
                ptrGroup->AddClass(std::make_shared<CLegendClass>(m_sDefaultLabel.empty() ? std::string("<all other values>") : m_sDefaultLabel,
                                                                  m_ptrDefaultSymbol, std::string(), writer));
            }
            return ptrGroup;
        }

        // ISerialize

        void CExpressionSymbolSelector::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyInt32U("SymbolSelectorID", GetSymbolSelectorID());
                pObj->AddPropertyString("DefaultLabel", m_sDefaultLabel);
                pObj->AddPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                CSymbolSelectorUtils::SaveSymbol(pObj, "DefaultSymbol", m_ptrDefaultSymbol);

                CommonLib::ISerializeObjPtr ptrEntries = pObj->CreateChildNode("Expressions");
                for(size_t i = 0; i < m_vecEntries.size(); ++i)
                {
                    const SExpressionEntry& entry = m_vecEntries[i];
                    CommonLib::ISerializeObjPtr ptrEntry = ptrEntries->CreateChildNode("Expression");
                    ptrEntry->AddPropertyString("Expression", entry.sExpression);
                    ptrEntry->AddPropertyString("Label", entry.sLabel);
                    ptrEntry->AddPropertyString("Description", entry.sDescription);
                    CSymbolSelectorUtils::SaveSymbol(ptrEntry, "Symbol", entry.ptrSymbol);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("ExpressionSymbolSelector: failed to save", exc);
            }
        }

        void CExpressionSymbolSelector::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_sDefaultLabel = pObj->GetPropertyString("DefaultLabel", m_sDefaultLabel);
                m_bUseDefaultSymbol = pObj->GetPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                m_ptrDefaultSymbol = CSymbolSelectorUtils::LoadSymbol(pObj, "DefaultSymbol");

                m_vecEntries.clear();
                if(pObj->IsChildExists("Expressions"))
                {
                    std::vector<CommonLib::ISerializeObjPtr> vecEntries = pObj->GetChild("Expressions")->GetChilds("Expression");
                    for(size_t i = 0; i < vecEntries.size(); ++i)
                    {
                        SExpressionEntry entry;
                        // a wrong expression doesn't break the document: kept as it is, never matches
                        ParseExpression(entry, vecEntries[i]->GetPropertyString("Expression", std::string()), false);
                        entry.sLabel = vecEntries[i]->GetPropertyString("Label", std::string());
                        entry.sDescription = vecEntries[i]->GetPropertyString("Description", std::string());
                        entry.ptrSymbol = CSymbolSelectorUtils::LoadSymbol(vecEntries[i], "Symbol");
                        m_vecEntries.push_back(entry);
                    }
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("ExpressionSymbolSelector: failed to load", exc);
            }
        }

    }
}
