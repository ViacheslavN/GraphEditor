#pragma once
#include "../Cartography.h"
#include "LabelTypes.h"
#include "LabelCollisionGrid.h"
#include <atomic>
#include <unordered_map>
#include <unordered_set>

namespace GraphEngine {
    namespace Cartography {

        // Label drawer without the label cache of the reference engine (LabelEngine + LabelCache + LLabel + LMatrix):
        // the labels are collected by AddLabel in device coordinates, DrawLabels sorts them by priority, finds
        // the first position of every label which doesn't overlap the placed labels and draws it at once.
        class CLabelDrawer : public ILabelDrawer




        {
            public:
            CLabelDrawer();
            virtual ~CLabelDrawer();

            // ILabelDrawer
            virtual void           BeginLabeling(Display::IDisplayPtr ptrDisplay);
            virtual void           Clear();
            virtual void           AddLabel(const std::wstring& text,  CommonLib::IGeoShapePtr ptrShape,
                                Display::ITextSymbolPtr ptrSymbol,  int classIndex, const SLabelingOptions& options);
            virtual void           DrawLabels(Display::ITrackCancelPtr ptrTrackCancel);
            virtual void           EndLabeling();
            virtual uint32_t       GetLabelCount() const;
            virtual uint32_t       GetPlacedLabelCount() const;

            // settings, sizes in mm
            double                 GetLabelBuffer() const;          // min gap between the labels
            void                   SetLabelBuffer(double dBuffer);
            double                 GetSearchStep() const;           // step of the positions along lines and across polygons
            void                   SetSearchStep(double dStep);
            uint32_t               GetMaxCandidates() const;        // max positions tried for a label
            void                   SetMaxCandidates(uint32_t nCount);
            bool                   GetKeepInsideView() const;       // labels are not cut by the view border
            void                   SetKeepInsideView(bool bKeep);

            // ISerialize
            virtual void Save(CommonLib::ISerializeObjPtr pObj) const;
            virtual void Load(CommonLib::ISerializeObjPtr pObj);

            private:
            struct SDuplicate
            {
                double x;
                double y;
                double size;
            };

            Labeling::CLabelStylePtr GetStyle(Display::ITextSymbolPtr ptrSymbol);
            bool FillGeometry(CommonLib::IGeoShapePtr ptrShape, Labeling::SLabelItem& item);
            bool PlaceLabel(const Labeling::SLabelItem& item, Labeling::SLabelPlacement& placement);
            bool IsInView(const Labeling::SLabelPlacement& placement) const;
            bool IsTooCloseDuplicate(const Labeling::SLabelItem& item, const Labeling::SLabelPlacement& placement) const;
            void RegisterPlacement(const Labeling::SLabelItem& item, const Labeling::SLabelPlacement& placement);
            void DrawPlacement(Display::IGraphicsPtr ptrGraphics, const Labeling::SLabelItem& item, const Labeling::SLabelPlacement& placement);
            double MMToDevice(double mm) const;

            private:
            Display::IDisplayPtr                m_ptrDisplay;
            std::vector<Labeling::SLabelItem>   m_labels;
            // the symbol is kept with its style, so the pointer isn't reused by another symbol while labeling
            std::unordered_map<Display::ITextSymbol*, std::pair<Display::ITextSymbolPtr, Labeling::CLabelStylePtr> > m_styles;
            Labeling::CLabelCollisionGrid       m_grid;
            std::unordered_set<std::wstring>   m_placedTexts;
            std::unordered_multimap<std::wstring, SDuplicate> m_duplicates;
            Display::GRect                      m_viewRect;
            double                              m_dDeviceBuffer;
            std::atomic<uint32_t>               m_nLabelCount;
            std::atomic<uint32_t>               m_nPlacedCount;

            double                              m_dLabelBuffer;
            double                              m_dSearchStep;
            uint32_t                            m_nMaxCandidates;
            bool                                m_bKeepInsideView;
        };

    }
}
