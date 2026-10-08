#pragma once
#include "../Symbols.h"
#include <functional>

namespace GraphEngine {
    namespace Display {

        // Dash pattern along a line (ported from UniGIS Template).
        // Pattern elements (mark, gap) are multiplied by the interval (symbol units, mm);
        // a template symbol draws its element at the middle of every mark.
        // No elements - one element (1, 0): an element every interval.
        class CLineTemplate
        {
        public:
            struct SPatternElement
            {
                double dMark;
                double dGap;
            };

            // callback: point on the line (device), line direction at the point (degrees, device coordinates)
            typedef std::function<void(const GPoint& point, double dAngle)> TMarkFunc;

            explicit CLineTemplate(double dInterval = 3.);
            ~CLineTemplate();

            void   AddPatternElement(double dMark, double dGap);
            void   RemovePatternElement(int nIndex);
            void   ClearPatternElements();
            int    GetPatternElementCount() const;
            void   GetPatternElement(int nIndex, double* pMark, double* pGap) const;
            double GetInterval() const;
            void   SetInterval(double dInterval);

            // converts the interval to device units
            void Prepare(IDisplayTransformationPtr ptrTrans, bool bScaleDependent);
            bool IsValid() const;   // prepared and has a positive step

            // walks every part of the device polyline, the pattern restarts at the beginning of each part,
            // dOffset - device offset to the left of the line direction
            void ForEachMark(const GPoint* points, const int* polyCounts, int polyCount, GUnits dOffset, const TMarkFunc& func) const;

            void Save(CommonLib::ISerializeObjPtr pObj, const std::string& name) const;
            void Load(CommonLib::ISerializeObjPtr pObj, const std::string& name);

        private:
            void ForEachMarkInPart(const GPoint* points, int count, GUnits dOffset, const TMarkFunc& func) const;

        private:
            std::vector<SPatternElement> m_vecElements;
            double m_dInterval;
            double m_dDeviceInterval;
        };

    }
}
