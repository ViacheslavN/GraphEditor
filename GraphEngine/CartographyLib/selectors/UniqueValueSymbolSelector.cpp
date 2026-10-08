#include "UniqueValueSymbolSelector.h"

namespace GraphEngine {
    namespace Cartography {

        CUniqueValueSymbolSelector::CUniqueValueSymbolSelector() : m_bUseDefaultSymbol(true), m_bMapDirty(true)
        {

        }

        CUniqueValueSymbolSelector::CUniqueValueSymbolSelector(const std::string& sField) : CUniqueValueSymbolSelector()
        {
            m_vecFields.push_back(sField);
        }

        CUniqueValueSymbolSelector::~CUniqueValueSymbolSelector()
        {

        }

        void CUniqueValueSymbolSelector::CheckIndex(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecValues.size())
                throw CommonLib::CExcBase("UniqueValueSymbolSelector: value index out of range: {0}", nIndex);
        }

        void CUniqueValueSymbolSelector::CheckFieldIndex(int nFieldIndex) const
        {
            if(nFieldIndex < 0 || nFieldIndex >= (int)m_vecFields.size())
                throw CommonLib::CExcBase("UniqueValueSymbolSelector: field index out of range: {0}", nFieldIndex);
        }

        void CUniqueValueSymbolSelector::SetDirty()
        {
            m_bMapDirty = true;
        }

        Display::ISymbolPtr CUniqueValueSymbolSelector::DefaultSymbol() const
        {
            return m_bUseDefaultSymbol ? m_ptrDefaultSymbol : Display::ISymbolPtr();
        }

        // ISymbolSelector

        bool CUniqueValueSymbolSelector::CanAssign(GeoDatabase::ITablePtr ptrTable) const
        {
            if(!ptrTable.get() || m_vecFields.empty())
                return false;

            GeoDatabase::IFieldsPtr ptrFields = ptrTable->GetFields();
            for(size_t i = 0; i < m_vecFields.size(); ++i)
            {
                if(!ptrFields->FieldExists(m_vecFields[i]))
                    return false;
            }
            return true;
        }

        void CUniqueValueSymbolSelector::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            if(!ptrFilter.get() || !CanAssign(ptrTable))
                return;

            for(size_t i = 0; i < m_vecFields.size(); ++i)
            {
                if(ptrFilter->GetFieldSet()->Find(m_vecFields[i]) < 0)
                    ptrFilter->GetFieldSet()->Add(m_vecFields[i]);
            }
            m_vecFieldColumns.assign(m_vecFields.size(), -1);   // resolved on the cursor row
        }

        void CUniqueValueSymbolSelector::BuildValueMap() const
        {
            m_valueMap.clear();
            for(size_t i = 0; i < m_vecValues.size(); ++i)
            {
                TKey key;
                for(size_t f = 0; f < m_vecFields.size(); ++f)
                {
                    const std::vector<CommonLib::CVariant>& values = m_vecValues[i].vecValues;
                    key.push_back(f < values.size() ? CSymbolSelectorUtils::MakeKey(values[f]) : SValueKey());
                }
                m_valueMap.insert(TValueMap::value_type(key, (int)i));   // the first entry wins
            }
            m_bMapDirty = false;
        }

        Display::ISymbolPtr CUniqueValueSymbolSelector::GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const
        {
            if(!ptrRow.get() || m_vecFields.empty())
                return DefaultSymbol();

            if(m_bMapDirty)
                BuildValueMap();
            if(m_valueMap.empty())
                return DefaultSymbol();

            if(m_vecFieldColumns.size() != m_vecFields.size())
                m_vecFieldColumns.assign(m_vecFields.size(), -1);

            TKey key;
            key.reserve(m_vecFields.size());
            for(size_t f = 0; f < m_vecFields.size(); ++f)
            {
                int32_t nColumn = CSymbolSelectorUtils::FindColumn(ptrRow, m_vecFields[f], m_vecFieldColumns[f]);
                key.push_back(CSymbolSelectorUtils::MakeKey(ptrRow, nColumn));
            }

            TValueMap::const_iterator it = m_valueMap.find(key);
            if(it == m_valueMap.end())
                return DefaultSymbol();

            return m_vecValues[it->second].ptrSymbol;
        }

        void CUniqueValueSymbolSelector::SetupSymbols(Display::IDisplayPtr ptrDisplay)
        {
            if(!ptrDisplay.get())
                return;
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Prepare(ptrDisplay); });
        }

        void CUniqueValueSymbolSelector::ResetSymbols()
        {
            ForEachSymbol([](Display::ISymbolPtr ptrSymbol) { ptrSymbol->Reset(); });
        }

        void CUniqueValueSymbolSelector::FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            ForEachSymbol([&](Display::ISymbolPtr ptrSymbol) { ptrSymbol->FlushBuffers(ptrDisplay, ptrTrackCancel); });
        }

        // IUniqueValueSymbolSelector

        const std::string& CUniqueValueSymbolSelector::GetHeadingLabel() const { return m_sHeadingLabel; }
        void CUniqueValueSymbolSelector::SetHeadingLabel(const std::string& sLabel) { m_sHeadingLabel = sLabel; }

        int CUniqueValueSymbolSelector::GetFieldCount() const
        {
            return (int)m_vecFields.size();
        }

        void CUniqueValueSymbolSelector::SetFieldCount(int nCount)
        {
            if(nCount < 0)
                throw CommonLib::CExcBase("UniqueValueSymbolSelector: wrong field count: {0}", nCount);

            m_vecFields.resize(nCount);
            for(size_t i = 0; i < m_vecValues.size(); ++i)
                m_vecValues[i].vecValues.resize(nCount);
            SetDirty();
        }

        const std::string& CUniqueValueSymbolSelector::GetField(int nFieldIndex) const
        {
            CheckFieldIndex(nFieldIndex);
            return m_vecFields[nFieldIndex];
        }

        void CUniqueValueSymbolSelector::SetField(int nFieldIndex, const std::string& sFieldName)
        {
            CheckFieldIndex(nFieldIndex);
            m_vecFields[nFieldIndex] = sFieldName;
            m_vecFieldColumns.clear();
        }

        int CUniqueValueSymbolSelector::GetValueCount() const
        {
            return (int)m_vecValues.size();
        }

        void CUniqueValueSymbolSelector::SetValueCount(int nCount)
        {
            if(nCount < 0)
                throw CommonLib::CExcBase("UniqueValueSymbolSelector: wrong value count: {0}", nCount);

            size_t nOld = m_vecValues.size();
            m_vecValues.resize(nCount);
            for(size_t i = nOld; i < m_vecValues.size(); ++i)
                m_vecValues[i].vecValues.resize(m_vecFields.size());
            SetDirty();
        }

        int CUniqueValueSymbolSelector::AddValue(const std::vector<CommonLib::CVariant>& values, Display::ISymbolPtr ptrSymbol, const std::string& sLabel)
        {
            if(values.size() != m_vecFields.size())
                throw CommonLib::CExcBase("UniqueValueSymbolSelector: {0} values for {1} fields", (int)values.size(), (int)m_vecFields.size());

            SValueEntry entry;
            entry.vecValues = values;
            entry.ptrSymbol = ptrSymbol;
            entry.sLabel = sLabel;
            m_vecValues.push_back(entry);
            SetDirty();
            return (int)m_vecValues.size() - 1;
        }

        void CUniqueValueSymbolSelector::RemoveValue(int nIndex)
        {
            CheckIndex(nIndex);
            m_vecValues.erase(m_vecValues.begin() + nIndex);
            SetDirty();
        }

        CommonLib::CVariant CUniqueValueSymbolSelector::GetValue(int nIndex, int nFieldIndex) const
        {
            CheckIndex(nIndex);
            CheckFieldIndex(nFieldIndex);
            const std::vector<CommonLib::CVariant>& values = m_vecValues[nIndex].vecValues;
            return nFieldIndex < (int)values.size() ? values[nFieldIndex] : CommonLib::CVariant();
        }

        void CUniqueValueSymbolSelector::SetValue(int nIndex, int nFieldIndex, const CommonLib::CVariant& value)
        {
            CheckIndex(nIndex);
            CheckFieldIndex(nFieldIndex);
            std::vector<CommonLib::CVariant>& values = m_vecValues[nIndex].vecValues;
            if((int)values.size() <= nFieldIndex)
                values.resize(m_vecFields.size());
            values[nFieldIndex] = value;
            SetDirty();
        }

        const std::string& CUniqueValueSymbolSelector::GetLabel(int nIndex) const { CheckIndex(nIndex); return m_vecValues[nIndex].sLabel; }
        void CUniqueValueSymbolSelector::SetLabel(int nIndex, const std::string& sLabel) { CheckIndex(nIndex); m_vecValues[nIndex].sLabel = sLabel; }
        const std::string& CUniqueValueSymbolSelector::GetDescription(int nIndex) const { CheckIndex(nIndex); return m_vecValues[nIndex].sDescription; }
        void CUniqueValueSymbolSelector::SetDescription(int nIndex, const std::string& sDescription) { CheckIndex(nIndex); m_vecValues[nIndex].sDescription = sDescription; }
        Display::ISymbolPtr CUniqueValueSymbolSelector::GetSymbol(int nIndex) const { CheckIndex(nIndex); return m_vecValues[nIndex].ptrSymbol; }
        void CUniqueValueSymbolSelector::SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol) { CheckIndex(nIndex); m_vecValues[nIndex].ptrSymbol = ptrSymbol; }
        int CUniqueValueSymbolSelector::GetGroup(int nIndex) const { CheckIndex(nIndex); return m_vecValues[nIndex].nGroup; }
        void CUniqueValueSymbolSelector::SetGroup(int nIndex, int nGroup) { CheckIndex(nIndex); m_vecValues[nIndex].nGroup = nGroup; }

        Display::ISymbolPtr CUniqueValueSymbolSelector::GetDefaultSymbol() const { return m_ptrDefaultSymbol; }
        void CUniqueValueSymbolSelector::SetDefaultSymbol(Display::ISymbolPtr ptrSymbol) { m_ptrDefaultSymbol = ptrSymbol; }
        const std::string& CUniqueValueSymbolSelector::GetDefaultLabel() const { return m_sDefaultLabel; }
        void CUniqueValueSymbolSelector::SetDefaultLabel(const std::string& sLabel) { m_sDefaultLabel = sLabel; }
        bool CUniqueValueSymbolSelector::GetUseDefaultSymbol() const { return m_bUseDefaultSymbol; }
        void CUniqueValueSymbolSelector::SetUseDefaultSymbol(bool bUse) { m_bUseDefaultSymbol = bUse; }

        // ISerialize

        void CUniqueValueSymbolSelector::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyInt32U("SymbolSelectorID", GetSymbolSelectorID());
                pObj->AddPropertyString("HeadingLabel", m_sHeadingLabel);
                pObj->AddPropertyString("DefaultLabel", m_sDefaultLabel);
                pObj->AddPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                CSymbolSelectorUtils::SaveSymbol(pObj, "DefaultSymbol", m_ptrDefaultSymbol);

                CommonLib::ISerializeObjPtr ptrFields = pObj->CreateChildNode("Fields");
                for(size_t i = 0; i < m_vecFields.size(); ++i)
                    ptrFields->CreateChildNode("Field")->AddPropertyString("Name", m_vecFields[i]);

                CommonLib::ISerializeObjPtr ptrValues = pObj->CreateChildNode("Values");
                for(size_t i = 0; i < m_vecValues.size(); ++i)
                {
                    const SValueEntry& entry = m_vecValues[i];
                    CommonLib::ISerializeObjPtr ptrEntry = ptrValues->CreateChildNode("Entry");
                    ptrEntry->AddPropertyString("Label", entry.sLabel);
                    ptrEntry->AddPropertyString("Description", entry.sDescription);
                    ptrEntry->AddPropertyInt32("Group", entry.nGroup);
                    for(size_t f = 0; f < entry.vecValues.size(); ++f)
                        CSymbolSelectorUtils::SaveValue(ptrEntry->CreateChildNode("Value"), entry.vecValues[f]);
                    CSymbolSelectorUtils::SaveSymbol(ptrEntry, "Symbol", entry.ptrSymbol);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("UniqueValueSymbolSelector: failed to save", exc);
            }
        }

        void CUniqueValueSymbolSelector::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_sHeadingLabel = pObj->GetPropertyString("HeadingLabel", m_sHeadingLabel);
                m_sDefaultLabel = pObj->GetPropertyString("DefaultLabel", m_sDefaultLabel);
                m_bUseDefaultSymbol = pObj->GetPropertyBool("UseDefaultSymbol", m_bUseDefaultSymbol);
                m_ptrDefaultSymbol = CSymbolSelectorUtils::LoadSymbol(pObj, "DefaultSymbol");

                m_vecFields.clear();
                if(pObj->IsChildExists("Fields"))
                {
                    std::vector<CommonLib::ISerializeObjPtr> vecFields = pObj->GetChild("Fields")->GetChilds("Field");
                    for(size_t i = 0; i < vecFields.size(); ++i)
                        m_vecFields.push_back(vecFields[i]->GetPropertyString("Name", std::string()));
                }

                m_vecValues.clear();
                if(pObj->IsChildExists("Values"))
                {
                    std::vector<CommonLib::ISerializeObjPtr> vecEntries = pObj->GetChild("Values")->GetChilds("Entry");
                    for(size_t i = 0; i < vecEntries.size(); ++i)
                    {
                        SValueEntry entry;
                        entry.sLabel = vecEntries[i]->GetPropertyString("Label", std::string());
                        entry.sDescription = vecEntries[i]->GetPropertyString("Description", std::string());
                        entry.nGroup = vecEntries[i]->GetPropertyInt32("Group", 0);
                        std::vector<CommonLib::ISerializeObjPtr> vecValues = vecEntries[i]->GetChilds("Value");
                        for(size_t f = 0; f < vecValues.size(); ++f)
                            entry.vecValues.push_back(CSymbolSelectorUtils::LoadValue(vecValues[f]));
                        entry.vecValues.resize(m_vecFields.size());
                        entry.ptrSymbol = CSymbolSelectorUtils::LoadSymbol(vecEntries[i], "Symbol");
                        m_vecValues.push_back(entry);
                    }
                }

                m_vecFieldColumns.clear();
                SetDirty();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("UniqueValueSymbolSelector: failed to load", exc);
            }
        }

    }
}
