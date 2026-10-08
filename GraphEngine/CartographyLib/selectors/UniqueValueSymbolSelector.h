#pragma once
#include "SymbolSelectorUtils.h"
#include <map>

namespace GraphEngine {
    namespace Cartography {

        // symbol by the unique values of one or several fields (ported from UniGIS UniqueValueSymbolAssigner, without ILegendInfo)
        class CUniqueValueSymbolSelector : public IUniqueValueSymbolSelector
        {
        public:
            CUniqueValueSymbolSelector();
            explicit CUniqueValueSymbolSelector(const std::string& sField);
            virtual ~CUniqueValueSymbolSelector();

        private:
            CUniqueValueSymbolSelector(const CUniqueValueSymbolSelector&);
            CUniqueValueSymbolSelector& operator=(const CUniqueValueSymbolSelector&);

        public:
            // ISymbolSelector
            virtual uint32_t               GetSymbolSelectorID() const {return UniqueValueSymbolSelectorID;}
            virtual bool                   CanAssign(GeoDatabase::ITablePtr ptrTable) const;
            virtual void                   PrepareFilter(GeoDatabase::ITablePtr ptrTable, GeoDatabase::IQueryFilterPtr ptrFilter) const;
            virtual Display::ISymbolPtr    GetSymbolByFeature(GeoDatabase::IRowPtr ptrRow) const;
            virtual void                   SetupSymbols(Display::IDisplayPtr ptrDisplay);
            virtual void                   ResetSymbols();
            virtual void                   FlushBuffers(Display::IDisplayPtr ptrDisplay, Display::ITrackCancelPtr ptrTrackCancel);

            // IUniqueValueSymbolSelector
            virtual const std::string&     GetHeadingLabel() const;
            virtual void                   SetHeadingLabel(const std::string& sLabel);
            virtual int                    GetFieldCount() const;
            virtual void                   SetFieldCount(int nCount);
            virtual const std::string&     GetField(int nFieldIndex) const;
            virtual void                   SetField(int nFieldIndex, const std::string& sFieldName);
            virtual int                    GetValueCount() const;
            virtual void                   SetValueCount(int nCount);
            virtual int                    AddValue(const std::vector<CommonLib::CVariant>& values, Display::ISymbolPtr ptrSymbol, const std::string& sLabel = std::string());
            virtual void                   RemoveValue(int nIndex);
            virtual CommonLib::CVariant    GetValue(int nIndex, int nFieldIndex) const;
            virtual void                   SetValue(int nIndex, int nFieldIndex, const CommonLib::CVariant& value);
            virtual const std::string&     GetLabel(int nIndex) const;
            virtual void                   SetLabel(int nIndex, const std::string& sLabel);
            virtual const std::string&     GetDescription(int nIndex) const;
            virtual void                   SetDescription(int nIndex, const std::string& sDescription);
            virtual Display::ISymbolPtr    GetSymbol(int nIndex) const;
            virtual void                   SetSymbol(int nIndex, Display::ISymbolPtr ptrSymbol);
            virtual int                    GetGroup(int nIndex) const;
            virtual void                   SetGroup(int nIndex, int nGroup);
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
            struct SValueEntry
            {
                std::vector<CommonLib::CVariant> vecValues;   // one per field
                std::string         sLabel;
                std::string         sDescription;
                Display::ISymbolPtr ptrSymbol;
                int                 nGroup = 0;
            };

            typedef std::vector<SValueKey> TKey;
            typedef std::map<TKey, int> TValueMap;   // key of the value -> index of the first entry with it

            void CheckIndex(int nIndex) const;
            void CheckFieldIndex(int nFieldIndex) const;
            void SetDirty();
            void BuildValueMap() const;
            Display::ISymbolPtr DefaultSymbol() const;

            template<class F>
            void ForEachSymbol(F func) const
            {
                for(size_t i = 0; i < m_vecValues.size(); ++i)
                    if(m_vecValues[i].ptrSymbol.get())
                        func(m_vecValues[i].ptrSymbol);
                if(m_ptrDefaultSymbol.get())
                    func(m_ptrDefaultSymbol);
            }

        private:
            std::vector<std::string> m_vecFields;
            std::vector<SValueEntry> m_vecValues;
            Display::ISymbolPtr      m_ptrDefaultSymbol;
            std::string              m_sDefaultLabel;
            std::string              m_sHeadingLabel;
            bool                     m_bUseDefaultSymbol;

            mutable std::vector<int32_t> m_vecFieldColumns;   // column of every field in the cursor rows, reset by PrepareFilter
            mutable TValueMap            m_valueMap;
            mutable bool                 m_bMapDirty;
        };

    }
}
