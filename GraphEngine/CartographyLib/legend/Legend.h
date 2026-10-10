#pragma once
#include "../Cartography.h"
#include <functional>
#include <map>

namespace GraphEngine {
    namespace Cartography {

        // legend class: a symbol with a label; SetSymbol calls the writer, which sets the symbol to the object
        // the class was made by (see ILegendInfo)
        class CLegendClass : public ILegendClass
        {
        public:
            typedef std::function<void(Display::ISymbolPtr)> TSymbolWriter;

            CLegendClass();
            CLegendClass(const std::string& sLabel, Display::ISymbolPtr ptrSymbol, const std::string& sDescription = std::string(),
                         TSymbolWriter writer = TSymbolWriter());
            virtual ~CLegendClass();

            // ILegendClass
            virtual const std::string&     GetLabel() const;
            virtual void                   SetLabel(const std::string& sLabel);
            virtual const std::string&     GetDescription() const;
            virtual void                   SetDescription(const std::string& sDescription);
            virtual Display::ISymbolPtr    GetSymbol() const;
            virtual void                   SetSymbol(Display::ISymbolPtr ptrSymbol);

        private:
            std::string         m_sLabel;
            std::string         m_sDescription;
            Display::ISymbolPtr m_ptrSymbol;
            TSymbolWriter       m_writer;
        };

        class CLegendGroup : public ILegendGroup
        {
        public:
            CLegendGroup();
            explicit CLegendGroup(const std::string& sHeading);
            virtual ~CLegendGroup();

            // ILegendGroup
            virtual const std::string&     GetHeading() const;
            virtual void                   SetHeading(const std::string& sHeading);
            virtual bool                   GetVisible() const;
            virtual void                   SetVisible(bool bVisible);
            virtual int                    GetClassCount() const;
            virtual ILegendClassPtr        GetClass(int nIndex) const;
            virtual int                    AddClass(ILegendClassPtr ptrClass);
            virtual void                   ClearClasses();

        private:
            std::string                  m_sHeading;
            bool                         m_bVisible;
            std::vector<ILegendClassPtr> m_vecClasses;
        };

        typedef std::shared_ptr<CLegendGroup> CLegendGroupPtr;

        class CLegendUtils
        {
        public:
            // writer of a legend class symbol: sets the symbol to the owner while the owner exists and still
            // has the symbol of the class (get(owner) - the current symbol of the class in the owner, null - no more)
            template<class TOwner, class FGet, class FSet>
            static CLegendClass::TSymbolWriter MakeSymbolWriter(std::weak_ptr<TOwner> wOwner, Display::ISymbolPtr ptrSymbol, FGet get, FSet set)
            {
                if(wOwner.expired())
                    return CLegendClass::TSymbolWriter();   // the owner isn't kept by a shared_ptr: the legend is read only

                std::shared_ptr<Display::ISymbolPtr> ptrCurrent = std::make_shared<Display::ISymbolPtr>(ptrSymbol);
                return [wOwner, ptrCurrent, get, set](Display::ISymbolPtr ptrNewSymbol)
                {
                    std::shared_ptr<TOwner> ptrOwner = wOwner.lock();
                    if(!ptrOwner.get() || get(*ptrOwner) != *ptrCurrent)
                        return;   // the owner is gone or changed after the legend was made

                    set(*ptrOwner, ptrNewSymbol);
                    *ptrCurrent = ptrNewSymbol;
                };
            }

            // all the groups of the object, empty - the object has no legend (doesn't implement ILegendInfo)
            static std::vector<ILegendGroupPtr> GetLegendGroups(const ILegendInfo* pLegendInfo);
            // the groups of the layer (ILegendInfo of the layer)
            static std::vector<ILegendGroupPtr> GetLayerLegend(ILayerPtr ptrLayer);

            // a copy of the group: the line symbol of a class gets the line symbol with the same label from mapUnder
            // under it (road casing + fill in the legend); SetSymbol of such a class sets the symbol of the original class
            static ILegendGroupPtr UnderlayGroup(ILegendGroupPtr ptrGroup, const std::map<std::string, Display::ISymbolPtr>& mapUnder);

            // label of a unique value: text, number (%.15g), "<null>"
            static std::string ValueToLabel(const CommonLib::CVariant& value);
            static std::string NumberToLabel(double dValue);
        };
    }
}
