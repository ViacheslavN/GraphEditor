#include "SimpleSymbolSelector.h"
#include "../../DisplayLib/Symbols/SymbolsLoader.h"

namespace GraphEngine {
    namespace Cartography {

        CSimpleSymbolSelector::CSimpleSymbolSelector()
        {

        }

        CSimpleSymbolSelector::CSimpleSymbolSelector(Display::ISymbolPtr ptrSymbol) : m_ptrSymbol(ptrSymbol)
        {

        }

        CSimpleSymbolSelector::~CSimpleSymbolSelector()
        {

        }

        bool CSimpleSymbolSelector::CanAssign(GeoDatabase::ITablePtr ptrTable) const
        {
            return ptrTable.get() != nullptr;
        }

        void CSimpleSymbolSelector::PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const
        {
            // one symbol for all rows, no attribute fields are required
        }

        Display::ISymbolPtr CSimpleSymbolSelector::GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const
        {
            return m_ptrSymbol;
        }

        void CSimpleSymbolSelector::SetupSymbols(Display::IDisplayPtr ptrDisplay)
        {
            if(ptrDisplay.get() && m_ptrSymbol.get())
                m_ptrSymbol->Prepare(ptrDisplay);
        }

        void CSimpleSymbolSelector::ResetSymbols()
        {
            if(m_ptrSymbol.get())
                m_ptrSymbol->Reset();
        }

        void CSimpleSymbolSelector::FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel)
        {
            if(m_ptrSymbol.get())
                m_ptrSymbol->FlushBuffers(ptrDisplay, ptrTrackCancel);
        }

        const std::string& CSimpleSymbolSelector::GetDescription() const
        {
            return m_sDescription;
        }

        void CSimpleSymbolSelector::SetDescription(const std::string& sDesc)
        {
            m_sDescription = sDesc;
        }

        const std::string& CSimpleSymbolSelector::GetLabel() const
        {
            return m_sLabel;
        }

        void CSimpleSymbolSelector::SetLabel(const std::string& sLabel)
        {
            m_sLabel = sLabel;
        }

        Display::ISymbolPtr CSimpleSymbolSelector::GetSymbol() const
        {
            return m_ptrSymbol;
        }

        void CSimpleSymbolSelector::SetSymbol(Display::ISymbolPtr ptrSymbol)
        {
            m_ptrSymbol = ptrSymbol;
        }

        int CSimpleSymbolSelector::GetSymbolCount() const
        {
            return 1;
        }

        Display::ISymbolPtr CSimpleSymbolSelector::GetSymbolByIndex(int index) const
        {
            if(index == 0)
                return m_ptrSymbol;

            return Display::ISymbolPtr();
        }

        void CSimpleSymbolSelector::SetSymbolByIndex(int index, Display::ISymbolPtr ptrSymbol)
        {
            if(index == 0)
                m_ptrSymbol = ptrSymbol;
        }

        void CSimpleSymbolSelector::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            try
            {
                pObj->AddPropertyInt32U("SymbolSelectorID", GetSymbolSelectorID());
                pObj->AddPropertyString("Label", m_sLabel);
                pObj->AddPropertyString("Description", m_sDescription);

                if(m_ptrSymbol.get())
                {
                    CommonLib::ISerializeObjPtr ptrSymbolNode = pObj->CreateChildNode("Symbol");
                    m_ptrSymbol->Save(ptrSymbolNode);
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("SimpleSymbolSelector: failed to save", exc);
            }
        }

        void CSimpleSymbolSelector::Load(CommonLib::ISerializeObjPtr pObj)
        {
            try
            {
                m_sLabel = pObj->GetPropertyString("Label", m_sLabel);
                m_sDescription = pObj->GetPropertyString("Description", m_sDescription);

                m_ptrSymbol.reset();
                if(pObj->IsChildExists("Symbol"))
                    m_ptrSymbol = Display::CSymbolsLoader::LoadSymbol(pObj->GetChild("Symbol"));
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("SimpleSymbolSelector: failed to load", exc);
            }
        }

    }
}
