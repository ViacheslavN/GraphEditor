#include "Legend.h"
#include "../selectors/SymbolSelectorUtils.h"
#include "../../DisplayLib/Symbols/MultiLayerSymbol.h"

#include <cstdio>

namespace GraphEngine {
    namespace Cartography {

        CLegendClass::CLegendClass()
        {

        }

        CLegendClass::CLegendClass(const std::string& sLabel, Display::ISymbolPtr ptrSymbol, const std::string& sDescription, TSymbolWriter writer) :
                m_sLabel(sLabel), m_sDescription(sDescription), m_ptrSymbol(ptrSymbol), m_writer(writer)
        {

        }

        CLegendClass::~CLegendClass()
        {

        }

        const std::string& CLegendClass::GetLabel() const
        {
            return m_sLabel;
        }

        void CLegendClass::SetLabel(const std::string& sLabel)
        {
            m_sLabel = sLabel;
        }

        const std::string& CLegendClass::GetDescription() const
        {
            return m_sDescription;
        }

        void CLegendClass::SetDescription(const std::string& sDescription)
        {
            m_sDescription = sDescription;
        }

        Display::ISymbolPtr CLegendClass::GetSymbol() const
        {
            return m_ptrSymbol;
        }

        void CLegendClass::SetSymbol(Display::ISymbolPtr ptrSymbol)
        {
            m_ptrSymbol = ptrSymbol;
            if(m_writer)
                m_writer(ptrSymbol);
        }


        CLegendGroup::CLegendGroup() : m_bVisible(true)
        {

        }

        CLegendGroup::CLegendGroup(const std::string& sHeading) : m_sHeading(sHeading), m_bVisible(true)
        {

        }

        CLegendGroup::~CLegendGroup()
        {

        }

        const std::string& CLegendGroup::GetHeading() const
        {
            return m_sHeading;
        }

        void CLegendGroup::SetHeading(const std::string& sHeading)
        {
            m_sHeading = sHeading;
        }

        bool CLegendGroup::GetVisible() const
        {
            return m_bVisible;
        }

        void CLegendGroup::SetVisible(bool bVisible)
        {
            m_bVisible = bVisible;
        }

        int CLegendGroup::GetClassCount() const
        {
            return (int)m_vecClasses.size();
        }

        ILegendClassPtr CLegendGroup::GetClass(int nIndex) const
        {
            if(nIndex < 0 || nIndex >= (int)m_vecClasses.size())
                throw CommonLib::CExcBase("Legend group: class index out of range: {0}", nIndex);

            return m_vecClasses[nIndex];
        }

        int CLegendGroup::AddClass(ILegendClassPtr ptrClass)
        {
            if(!ptrClass.get())
                throw CommonLib::CExcBase("Legend group: class is null");

            m_vecClasses.push_back(ptrClass);
            return (int)m_vecClasses.size() - 1;
        }

        void CLegendGroup::ClearClasses()
        {
            m_vecClasses.clear();
        }


        std::vector<ILegendGroupPtr> CLegendUtils::GetLegendGroups(const ILegendInfo* pLegendInfo)
        {
            std::vector<ILegendGroupPtr> vecGroups;
            if(!pLegendInfo)
                return vecGroups;

            for(int i = 0, sz = pLegendInfo->GetLegendGroupCount(); i < sz; ++i)
            {
                ILegendGroupPtr ptrGroup = pLegendInfo->GetLegendGroup(i);
                if(ptrGroup.get())
                    vecGroups.push_back(ptrGroup);
            }
            return vecGroups;
        }

        std::vector<ILegendGroupPtr> CLegendUtils::GetLayerLegend(ILayerPtr ptrLayer)
        {
            return GetLegendGroups(dynamic_cast<const ILegendInfo*>(ptrLayer.get()));
        }

        ILegendGroupPtr CLegendUtils::UnderlayGroup(ILegendGroupPtr ptrGroup, const std::map<std::string, Display::ISymbolPtr>& mapUnder)
        {
            CLegendGroupPtr ptrResult = std::make_shared<CLegendGroup>(ptrGroup->GetHeading());
            ptrResult->SetVisible(ptrGroup->GetVisible());
            for(int i = 0, sz = ptrGroup->GetClassCount(); i < sz; ++i)
            {
                ILegendClassPtr ptrClass = ptrGroup->GetClass(i);
                auto it = mapUnder.find(ptrClass->GetLabel());
                Display::ILineSymbolPtr ptrUnder = it != mapUnder.end() ? std::dynamic_pointer_cast<Display::ILineSymbol>(it->second) : Display::ILineSymbolPtr();
                Display::ILineSymbolPtr ptrOver = std::dynamic_pointer_cast<Display::ILineSymbol>(ptrClass->GetSymbol());
                if(!ptrUnder.get() || !ptrOver.get())
                {
                    ptrResult->AddClass(ptrClass);
                    continue;
                }

                std::shared_ptr<Display::CMultiLayerLineSymbol> ptrCombined = std::make_shared<Display::CMultiLayerLineSymbol>();
                ptrCombined->AddLayer(ptrUnder);
                ptrCombined->AddLayer(ptrOver);
                ptrResult->AddClass(std::make_shared<CLegendClass>(ptrClass->GetLabel(), ptrCombined, ptrClass->GetDescription(),
                                                                   [ptrClass](Display::ISymbolPtr ptrSymbol) { ptrClass->SetSymbol(ptrSymbol); }));
            }
            return ptrResult;
        }

        std::string CLegendUtils::NumberToLabel(double dValue)
        {
            char buf[64];
            snprintf(buf, sizeof(buf), "%.15g", dValue);
            return buf;
        }

        std::string CLegendUtils::ValueToLabel(const CommonLib::CVariant& value)
        {
            SValueKey key = CSymbolSelectorUtils::MakeKey(value);
            switch(key.kind)
            {
                case SValueKey::KindNull:   return "<null>";
                case SValueKey::KindNumber: return NumberToLabel(key.dNumber);
                case SValueKey::KindText:   return key.sText;
                default:                    return "<value>";
            }
        }
    }
}
