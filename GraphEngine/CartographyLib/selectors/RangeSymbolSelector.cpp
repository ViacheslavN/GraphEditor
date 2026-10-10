#include "RangeSymbolSelector.h"
#include "../legend/Legend.h"
#include <algorithm>

namespace GraphEngine {
    namespace Cartography {

        CRangeSymbolSelector::CRangeSymbolSelector() : m_bUseDefaultSymbol(true), m_nFieldColumn(-1)
        {

        }

        CRangeSymbolSelector::CRangeSymbolSelector(const std::string& sField) : CRangeSymbolSelector()
        {
            m_sField = sField;
        }

        CRangeSymbolSelector::~CRangeSymbolSelector()
        {

        }

        void CRangeSymbolSelector::CheckIndex(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecRanges.size())
                throw CommonLib::CExcBase("RangeSymbolSelector: range index out of range: {0}", nIndex);
        }

        Display::ISymbolPtr CRangeSymbolSelector::DefaultSymbol() const
        {
            return m_bUseDefaultSymbol ? m_ptrDefaultSymbol : Display::ISymbolPtr();
        }

        // ISymbolSelector

        bool CRangeSymbolSelector::CanAssign(GeoDatabase::ITablePtr ptrTable) const
        {
            if(!ptrTable.get() || m_sField.empty())
                return false;
            return ptrTable->GetFields()->FieldExists(m_sField);
        }

        void CRangeSymbolSelector::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            if(!ptrFilter.get() || !CanAssign(ptrTable))
                return;

            if(ptrFilter->GetFieldSet()->Find(m_sField) < 0)
                ptrFilter->GetFieldSet()->Add(m_sField);
            m_nFieldColumn = -1;   // resolved on the cursor row
        }

        Display::ISymbolPtr CRangeSymbolSelector::GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!ptrRow.get() || m_sField.empty() || m_vecRanges.empty())
                return DefaultSymbol();

            int32_t nColumn = CSymbolSelectorUtils::FindColumn(ptrRow, m_sField, m_nFieldColumn);
            double dValue = 0.;
            if(!CSymbolSelectorUtils::ToDouble(ptrRow, nColumn, dValue))
                return DefaultSymbol();

            for(size_t i = 0; i < m_vecRanges.size(); ++i)
            {
                if(m_vecRanges[i].dFrom <= dValue && dValue <= m_vecRanges[i].dTo)
                    return m_vecRanges[i].ptrSymbol;
            }
            return DefaultSymbol();
        }

        void CRangeSymbolSelector::SetupSymbols(Display::IDisplayPtr ptrDisplay)
        {
            if(!ptrDisplay.get())
                return;
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Prepare(ptrDisplay); });
        }

        void CRangeSymbolSelector::ResetSymbols()
        {
            ForEachSymbol([](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Reset(); });
        }

        void CRangeSymbolSelector::FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->FlushBuffers(ptrDisplay, ptrTrackCancel); });
        }

        // IRangeSymbolSelector

        const std::string& CRangeSymbolSelector::GetField() const
        {
            return m_sField;
        }

        void CRangeSymbolSelector::SetField(const std::string& sFieldName)
        {
            m_sField = sFieldName;
            m_nFieldColumn = -1;
        }

        int CRangeSymbolSelector::GetRangeCount() const
        {
            return (int)m_vecRanges.size();
        }

        void CRangeSymbolSelector::SetRangeCount(int nCount)
        {
            if(nCount < 0)
                throw CommonLib::CExcBase("RangeSymbolSelector: wrong range count: {0}", nCount);
            m_vecRanges.resize(nCount);
        }

        int CRangeSymbolSelector::AddRange(double dFrom, double dTo, Display::ISymbolPtr ptrSymbol, const std::string& sLabel)
        {
            SRangeEntry entry;
            entry.dFrom = (std::min)(dFrom, dTo);
            entry.dTo = (std::max)(dFrom, dTo);
            entry.ptrSymbol = ptrSymbol;
            entry.sLabel = sLabel;
            m_vecRanges.push_back(entry);
            return (int)m_vecRanges.size() - 1;
        }

        void CRangeSymbolSelector::RemoveRange(int nIndex)
        {
            CheckIndex(nIndex);
            m_vecRanges.erase(m_vecRanges.begin() + nIndex);
        }

        void CRangeSymbolSelector::GetRange(int nIndex, double* pFrom, double* pTo) const
        {
            CheckIndex(nIndex);
            if(pFrom)
                *pFrom = m_vecRanges[nIndex].dFrom;
            if(pTo)
                *pTo = m_vecRanges[nIndex].dTo;
        }

        void CRangeSymbolSelector::SetRange(int nIndex, double dFrom, double dTo)
        {
            CheckIndex(nIndex);
            m_vecRanges[nIndex].dFrom = (std::min)(dFrom, dTo);
            m_vecRanges[nIndex].dTo = (std::max)(dFrom, dTo);
        }

        const std::string& CRangeSymbolSelector::GetLabel(int nIndex) const { CheckIndex(nIndex); return m_vecRanges[nIndex].sLabel; }
        void CRangeSymbolSelector::SetLabel(int nIndex, const std::string& sLabel) { CheckIndex(nIndex); m_vecRanges[nIndex].sLabel = sLabel; }
        const std::string& CRangeSymbolSelector::GetDescription(int nIndex) const { CheckIndex(nIndex); return m_vecRanges[nIndex].sDescription; }
        void CRangeSymbolSelector::SetDescription(int nIndex, const std::string& sDescription) { CheckIndex(nIndex); m_vecRanges[nIndex].sDescription = sDescription; }
        Display::ISymbolPtr CRangeSymbolSelector::GetSymbol(int nIndex) const { CheckIndex(nIndex); return m_vecRanges[nIndex].ptrSymbol; }
        void CRangeSymbolSelector::SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol) { CheckIndex(nIndex); m_vecRanges[nIndex].ptrSymbol = ptrSymbol; }

        void CRangeSymbolSelector::SortRanges()
        {
            std::stable_sort(m_vecRanges.begin(), m_vecRanges.end(),
                [](const SRangeEntry& a, const SRangeEntry& b) { return a.dFrom < b.dFrom; });
        }

        Display::ISymbolPtr CRangeSymbolSelector::GetDefaultSymbol() const { return m_ptrDefaultSymbol; }
        void CRangeSymbolSelector::SetDefaultSymbol(Display::ISymbolPtr ptrSymbol) { m_ptrDefaultSymbol = ptrSymbol; }
        const std::string& CRangeSymbolSelector::GetDefaultLabel() const { return m_sDefaultLabel; }
        void CRangeSymbolSelector::SetDefaultLabel(const std::string& sLabel) { m_sDefaultLabel = sLabel; }
        bool CRangeSymbolSelector::GetUseDefaultSymbol() const { return m_bUseDefaultSymbol; }
        void CRangeSymbolSelector::SetUseDefaultSymbol(bool bUse) { m_bUseDefaultSymbol = bUse; }

        // ILegendInfo

        int CRangeSymbolSelector::GetLegendGroupCount() const
        {
            return 1;
        }

        ILegendGroupPtr CRangeSymbolSelector::GetLegendGroup(int nIndex) const
        {
            if(nIndex != 0)
                throw CommonLib::CExcBase("RangeSymbolSelector: legend group index out of range: {0}", nIndex);

            std::weak_ptr<CRangeSymbolSelector> wSelf = std::const_pointer_cast<CRangeSymbolSelector>(weak_from_this().lock());
            CLegendGroupPtr ptrGroup = std::make_shared<CLegendGroup>(m_sField);
            for(int i = 0; i < (int)m_vecRanges.size(); ++i)
            {
                const SRangeEntry& entry = m_vecRanges[i];
                std::string sLabel = entry.sLabel.empty() ? CLegendUtils::NumberToLabel(entry.dFrom) + " - " + CLegendUtils::NumberToLabel(entry.dTo) : entry.sLabel;

                CLegendClass::TSymbolWriter writer = CLegendUtils::MakeSymbolWriter(wSelf, entry.ptrSymbol,
                        [i](CRangeSymbolSelector& selector) { return i < selector.GetRangeCount() ? selector.GetSymbol(i) : Display::ISymbolPtr(); },
                        [i](CRangeSymbolSelector& selector, Display::ISymbolPtr ptrSymbol) { selector.SetSymbol(i, ptrSymbol); });
                ptrGroup->AddClass(std::make_shared<CLegendClass>(sLabel, entry.ptrSymbol, entry.sDescription, writer));
            }

            if(m_bUseDefaultSymbol && m_ptrDefaultSymbol.get())
            {
                CLegendClass::TSymbolWriter writer = CLegendUtils::MakeSymbolWriter(wSelf, m_ptrDefaultSymbol,
                        [](CRangeSymbolSelector& selector) { return selector.GetDefaultSymbol(); },
                        [](CRangeSymbolSelector& selector, Display::ISymbolPtr ptrSymbol) { selector.SetDefaultSymbol(ptrSymbol); });
                ptrGroup->AddClass(std::make_shared<CLegendClass>(m_sDefaultLabel.empty() ? std::string("<all other values>") : m_sDefaultLabel,
                                                                  m_ptrDefaultSymbol, std::string(), writer));
            }
            return ptrGroup;
        }

        // ISerialize

        void CRangeSymbolSelector::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyInt32U("SymbolSelectorID", GetSymbolSelectorID());
                pObj->AddPropertyString("Field", m_sField);
                pObj->AddPropertyString("DefaultLabel", m_sDefaultLabel);
                pObj->AddPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                CSymbolSelectorUtils::SaveSymbol(pObj, "DefaultSymbol", m_ptrDefaultSymbol);

                CommonLib::ISerializeObjPtr ptrRanges = pObj->CreateChildNode("Ranges");
                for(size_t i = 0; i < m_vecRanges.size(); ++i)
                {
                    const SRangeEntry& entry = m_vecRanges[i];
                    CommonLib::ISerializeObjPtr ptrEntry = ptrRanges->CreateChildNode("Range");
                    ptrEntry->AddPropertyDouble("From", entry.dFrom);
                    ptrEntry->AddPropertyDouble("To", entry.dTo);
                    ptrEntry->AddPropertyString("Label", entry.sLabel);
                    ptrEntry->AddPropertyString("Description", entry.sDescription);
                    CSymbolSelectorUtils::SaveSymbol(ptrEntry, "Symbol", entry.ptrSymbol);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("RangeSymbolSelector: failed to save", exc);
            }
        }

        void CRangeSymbolSelector::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_sField = pObj->GetPropertyString("Field", m_sField);
                m_sDefaultLabel = pObj->GetPropertyString("DefaultLabel", m_sDefaultLabel);
                m_bUseDefaultSymbol = pObj->GetPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                m_ptrDefaultSymbol = CSymbolSelectorUtils::LoadSymbol(pObj, "DefaultSymbol");

                m_vecRanges.clear();
                if(pObj->IsChildExists("Ranges"))
                {
                    std::vector<CommonLib::ISerializeObjPtr> vecRanges = pObj->GetChild("Ranges")->GetChilds("Range");
                    for(size_t i = 0; i < vecRanges.size(); ++i)
                    {
                        SRangeEntry entry;
                        entry.dFrom = vecRanges[i]->GetPropertyDouble("From", 0.);
                        entry.dTo = vecRanges[i]->GetPropertyDouble("To", 0.);
                        entry.sLabel = vecRanges[i]->GetPropertyString("Label", std::string());
                        entry.sDescription = vecRanges[i]->GetPropertyString("Description", std::string());
                        entry.ptrSymbol = CSymbolSelectorUtils::LoadSymbol(vecRanges[i], "Symbol");
                        m_vecRanges.push_back(entry);
                    }
                }
                m_nFieldColumn = -1;
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("RangeSymbolSelector: failed to load", exc);
            }
        }

    }
}
