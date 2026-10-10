#include "LabelDrawer.h"
#include "LabelStrategy.h"
#include "../../DisplayLib/DisplayUtils.h"
#include <cwctype>

namespace GraphEngine {
    namespace Cartography {

        using namespace Labeling;

        namespace
        {
            const double GridCellSize = 32.;

            std::wstring CleanLabelText(const std::wstring& text)
            {
                std::wstring res = text;
                for(size_t i = 0; i < res.length(); ++i)
                {
                    if(res[i] == L'\r' || res[i] == L'\n' || res[i] == L'\t')
                        res[i] = L' ';
                }
                size_t begin = res.find_first_not_of(L' ');
                if(begin == std::wstring::npos)
                    return std::wstring();
                size_t end = res.find_last_not_of(L' ');
                return res.substr(begin, end - begin + 1);
            }
        }

        CLabelDrawer::CLabelDrawer() : m_dDeviceBuffer(0.), m_nLabelCount(0), m_nPlacedCount(0),
                                       m_dLabelBuffer(0.5), m_dSearchStep(2.5), m_nMaxCandidates(400), m_bKeepInsideView(true)
        {

        }

        CLabelDrawer::~CLabelDrawer()
        {

        }

        void CLabelDrawer::BeginLabeling(Display::IDisplayPtr ptrDisplay)
        {
            Clear();
            m_nPlacedCount = 0;
            m_ptrDisplay = ptrDisplay;
        }

        void CLabelDrawer::Clear()
        {
            m_labels.clear();
            m_styles.clear();
            m_grid.Clear();
            m_placedTexts.clear();
            m_duplicates.clear();
            m_nLabelCount = 0;
        }

        void CLabelDrawer::EndLabeling()
        {
            // the counters of the last labeling stay for the statistics
            uint32_t nLabelCount = m_nLabelCount;
            Clear();
            m_nLabelCount = nLabelCount;
            m_ptrDisplay.reset();
        }

        uint32_t CLabelDrawer::GetLabelCount() const
        {
            return m_nLabelCount;
        }

        uint32_t CLabelDrawer::GetPlacedLabelCount() const
        {
            return m_nPlacedCount;
        }

        double CLabelDrawer::MMToDevice(double mm) const
        {
            if(!m_ptrDisplay.get() || mm <= 0.)
                return 0.;
            return Display::CDisplayUtils::SymbolSizeToDeviceSize(m_ptrDisplay->GetTransformation(), mm, false);
        }

        CLabelStylePtr CLabelDrawer::GetStyle(Display::ITextSymbolPtr ptrSymbol)
        {
            auto it = m_styles.find(ptrSymbol.get());
            if(it != m_styles.end())
                return it->second.second;

            CLabelStylePtr ptrStyle = std::make_shared<CLabelStyle>(ptrSymbol, m_ptrDisplay);
            m_styles[ptrSymbol.get()] = std::make_pair(ptrSymbol, ptrStyle);
            return ptrStyle;
        }

        bool CLabelDrawer::FillGeometry(CommonLib::IGeoShapePtr ptrShape, SLabelItem& item)
        {
            Display::GPoint* pPoints = nullptr;
            int* pParts = nullptr;
            int nParts = 0;
            // device coordinates, clipped by the view (the shape is copied: the buffer belongs to the transformation)
            m_ptrDisplay->GetTransformation()->MapToDevice(ptrShape, &pPoints, &pParts, &nParts);
            if(nParts <= 0 || !pPoints)
                return false;

            std::vector<TLabelPoints> parts;
            for(int p = 0, offset = 0; p < nParts; offset += pParts[p], ++p)
            {
                TLabelPoints part((size_t)pParts[p]);
                for(int i = 0; i < pParts[p]; ++i)
                    part[i] = SLabelPoint(pPoints[offset + i].x, pPoints[offset + i].y);
                parts.push_back(part);
            }

            switch(ptrShape->GeneralType())
            {
                case CommonLib::shape_type_general_point:
                case CommonLib::shape_type_general_multipoint:
                {
                    if(parts[0].empty())
                        return false;
                    item.type = LabelGeometryPoint;
                    item.parts.assign(1, TLabelPoints(1, parts[0][0]));
                    item.weight = 0.;
                    return true;
                }
                case CommonLib::shape_type_general_polyline:
                {
                    item.type = LabelGeometryLine;
                    item.weight = 0.;
                    for(size_t p = 0; p < parts.size(); ++p)
                    {
                        if(parts[p].size() < 2)
                            continue;
                        CLabelPath path(parts[p]);
                        if(!path.IsValid())
                            continue;
                        item.weight += path.Length();
                        item.parts.push_back(parts[p]);
                    }
                    return !item.parts.empty();
                }
                case CommonLib::shape_type_general_polygon:
                {
                    // the biggest ring with its holes, a multipart polygon is labeled on its biggest part
                    int nOuter = -1;
                    double outerArea = 0.;
                    std::vector<double> areas(parts.size(), 0.);
                    for(size_t p = 0; p < parts.size(); ++p)
                    {
                        if(parts[p].size() < 3)
                            continue;
                        areas[p] = CLabelPolygon::SignedArea(parts[p]);
                        if(fabs(areas[p]) > outerArea)
                        {
                            outerArea = fabs(areas[p]);
                            nOuter = (int)p;
                        }
                    }
                    if(nOuter < 0 || outerArea <= 0.)
                        return false;

                    CLabelPolygon outer;
                    outer.AddRing(parts[nOuter]);
                    item.type = LabelGeometryPolygon;
                    item.weight = outerArea;
                    item.parts.push_back(parts[nOuter]);
                    // holes: the rings inside the outer ring (the orientation of the rings isn't reliable in all sources),
                    // the polygon uses the even-odd rule, so an island in a hole is handled too
                    for(size_t p = 0; p < parts.size(); ++p)
                    {
                        if((int)p == nOuter || areas[p] == 0.)
                            continue;
                        const SLabelPoint& pt = parts[p][0];
                        if(pt.x < outer.XMin() || pt.x > outer.XMax() || pt.y < outer.YMin() || pt.y > outer.YMax())
                            continue;
                        if(!outer.Contains(pt.x, pt.y))
                            continue;
                        item.parts.push_back(parts[p]);
                    }
                    return true;
                }
                default:
                    return false;
            }
        }

        void CLabelDrawer::AddLabel(const std::wstring& text,  CommonLib::IGeoShapePtr ptrShape,
                                    Display::ITextSymbolPtr ptrSymbol,  int classIndex, const SLabelingOptions& options)
        {
            try
            {
                if(!m_ptrDisplay.get() || !ptrShape.get() || !ptrSymbol.get())
                    return;

                if(ptrShape->GetPointCnt() == 0 || !ptrSymbol->CanDraw(ptrShape))
                    return;

                SLabelItem item;
                item.text = CleanLabelText(text);
                if(item.text.empty())
                    return;

                item.style = GetStyle(ptrSymbol);
                if(!item.style->IsValid())
                    return;

                if(!FillGeometry(ptrShape, item))
                    return;

                item.width = item.style->TextWidth(item.text);
                if(item.width <= 0.)
                    return;

                item.options = options;
                item.classIndex = classIndex;
                m_labels.push_back(std::move(item));
                m_nLabelCount = (uint32_t)m_labels.size();
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelDrawer: failed to add label", exc);
            }
        }

        bool CLabelDrawer::IsInView(const SLabelPlacement& placement) const
        {
            if(!m_bKeepInsideView)
                return true;

            for(size_t i = 0; i < placement.runs.size(); ++i)
            {
                Display::GRect rc = placement.runs[i].box.Bounds();
                if(rc.xMin < m_viewRect.xMin || rc.yMin < m_viewRect.yMin || rc.xMax > m_viewRect.xMax || rc.yMax > m_viewRect.yMax)
                    return false;
            }
            return true;
        }

        bool CLabelDrawer::IsTooCloseDuplicate(const SLabelItem& item, const SLabelPlacement& placement) const
        {
            if(item.options.m_duplicateStrategy != DuplicateStrategyDistance)
                return false;

            auto range = m_duplicates.equal_range(item.text);
            if(range.first == range.second)
                return false;

            Display::GRect bounds = placement.Bounds();
            double x = (bounds.xMin + bounds.xMax) / 2.;
            double y = (bounds.yMin + bounds.yMax) / 2.;
            double size = (std::max)(bounds.Width(), bounds.Height());
            double minDistance = MMToDevice(item.options.m_dDuplicateDistance);
            for(auto it = range.first; it != range.second; ++it)
            {
                // distance between the labels, not between their centers
                double dist = hypot(x - it->second.x, y - it->second.y) - (size + it->second.size) / 2.;
                if(dist < minDistance)
                    return true;
            }
            return false;
        }

        bool CLabelDrawer::PlaceLabel(const SLabelItem& item, SLabelPlacement& placement)
        {
            SLabelStrategyParams params;
            params.step = (std::max)(2., MMToDevice(m_dSearchStep));
            params.offset = MMToDevice(item.options.m_dOffset);
            params.maxCharAngle = item.options.m_dMaxCurvedCharAngle > 0. ? item.options.m_dMaxCurvedCharAngle : 30.;

            double halo = item.style->HaloSize();
            eLabelStrategy strategy = item.options.m_strategy;
            uint32_t nTries = 0;
            bool bFound = false;

            CLabelStrategy::GenerateCandidates(item, params, [&](const SLabelPlacement& candidate) -> bool
            {
                ++nTries;
                bool accept = true;
                if(strategy != LabelStrategyAlone)
                {
                    accept = IsInView(candidate) && !IsTooCloseDuplicate(item, candidate);
                    for(size_t i = 0; accept && i < candidate.runs.size(); ++i)
                    {
                        if(m_grid.Intersects(halo > 0. ? candidate.runs[i].box.Inflated(halo) : candidate.runs[i].box))
                            accept = false;
                    }
                }

                if(accept)
                {
                    placement = candidate;
                    bFound = true;
                    return true;
                }

                // Simple: only the first position
                return strategy != LabelStrategyComplex || nTries >= m_nMaxCandidates;
            });

            return bFound;
        }

        void CLabelDrawer::RegisterPlacement(const SLabelItem& item, const SLabelPlacement& placement)
        {
            double inflate = m_dDeviceBuffer + item.style->HaloSize();
            for(size_t i = 0; i < placement.runs.size(); ++i)
                m_grid.Insert(placement.runs[i].box.Inflated(inflate));

            m_placedTexts.insert(item.text);

            Display::GRect bounds = placement.Bounds();
            SDuplicate dup;
            dup.x = (bounds.xMin + bounds.xMax) / 2.;
            dup.y = (bounds.yMin + bounds.yMax) / 2.;
            dup.size = (std::max)(bounds.Width(), bounds.Height());
            m_duplicates.insert(std::make_pair(item.text, dup));
        }

        void CLabelDrawer::DrawPlacement(Display::IGraphicsPtr ptrGraphics, const SLabelItem& item, const SLabelPlacement& placement)
        {
            const CLabelStyle& style = *item.style;
            const wchar_t* text = item.text.c_str();

            // characters of a curved label: the halos first, otherwise a halo covers the previous character
            if(placement.runs.size() > 1 && style.HaloSize() > 0.)
            {
                for(size_t i = 0; i < placement.runs.size(); ++i)
                {
                    const SGlyphRun& run = placement.runs[i];
                    if(run.count == 1 && iswspace(text[run.start]))
                        continue;
                    style.DrawRun(ptrGraphics, text + run.start, run.count, run.x, run.y, run.angle, Display::TextDrawHaloOnly);
                }
                for(size_t i = 0; i < placement.runs.size(); ++i)
                {
                    const SGlyphRun& run = placement.runs[i];
                    if(run.count == 1 && iswspace(text[run.start]))
                        continue;
                    style.DrawRun(ptrGraphics, text + run.start, run.count, run.x, run.y, run.angle, Display::TextDrawTextOnly);
                }
                return;
            }

            for(size_t i = 0; i < placement.runs.size(); ++i)
            {
                const SGlyphRun& run = placement.runs[i];
                if(run.count == 1 && iswspace(text[run.start]))
                    continue;
                style.DrawRun(ptrGraphics, text + run.start, run.count, run.x, run.y, run.angle, Display::TextDrawAll);
            }
        }

        void CLabelDrawer::DrawLabels(Display::ITrackCancelPtr ptrTrackCancel)
        {
            m_nPlacedCount = 0;
            if(!m_ptrDisplay.get() || m_labels.empty())
                return;

            try
            {
                Display::IDisplayTransformationPtr ptrTrans = m_ptrDisplay->GetTransformation();
                Display::IGraphicsPtr ptrGraphics = m_ptrDisplay->GetGraphics();
                if(!ptrTrans.get() || !ptrGraphics.get())
                    return;

                m_viewRect = ptrTrans->GetDeviceRect();
                Display::GRect gridArea = m_viewRect;
                gridArea.Inflate(GridCellSize * 2, GridCellSize * 2);
                m_grid.Setup(gridArea, GridCellSize);
                m_placedTexts.clear();
                m_duplicates.clear();
                m_dDeviceBuffer = MMToDevice(m_dLabelBuffer);

                // priority (smaller first), class index (bigger first), bigger features first;
                // stable: the rest keeps the order of adding
                std::vector<size_t> order(m_labels.size());
                for(size_t i = 0; i < order.size(); ++i)
                    order[i] = i;
                std::stable_sort(order.begin(), order.end(), [this](size_t a, size_t b)
                {
                    const SLabelItem& la = m_labels[a];
                    const SLabelItem& lb = m_labels[b];
                    if(la.options.m_nPriority != lb.options.m_nPriority)
                        return la.options.m_nPriority < lb.options.m_nPriority;
                    if(la.classIndex != lb.classIndex)
                        return la.classIndex > lb.classIndex;
                    return la.weight > lb.weight;
                });

                const size_t nCheckCancelStep = 16;
                SLabelPlacement placement;
                for(size_t i = 0; i < order.size(); ++i)
                {
                    if(!(i % nCheckCancelStep) && ptrTrackCancel.get() && !ptrTrackCancel->Continue())
                        break;

                    const SLabelItem& item = m_labels[order[i]];
                    if(item.options.m_duplicateStrategy == DuplicateStrategyRemove && m_placedTexts.find(item.text) != m_placedTexts.end())
                        continue;

                    if(!PlaceLabel(item, placement))
                        continue;

                    RegisterPlacement(item, placement);

                    m_ptrDisplay->Lock();
                    try
                    {
                        DrawPlacement(ptrGraphics, item, placement);
                    }
                    catch (...)
                    {
                        m_ptrDisplay->UnLock();
                        throw;
                    }
                    m_ptrDisplay->UnLock();
                    ++m_nPlacedCount;
                }
            }
            catch (std::exception& exc)
            {
                CommonLib::CExcBase::RegenExc("LabelDrawer: failed to draw labels", exc);
            }
        }

        double CLabelDrawer::GetLabelBuffer() const
        {
            return m_dLabelBuffer;
        }

        void CLabelDrawer::SetLabelBuffer(double dBuffer)
        {
            m_dLabelBuffer = dBuffer > 0. ? dBuffer : 0.;
        }

        double CLabelDrawer::GetSearchStep() const
        {
            return m_dSearchStep;
        }

        void CLabelDrawer::SetSearchStep(double dStep)
        {
            m_dSearchStep = dStep;
        }

        uint32_t CLabelDrawer::GetMaxCandidates() const
        {
            return m_nMaxCandidates;
        }

        void CLabelDrawer::SetMaxCandidates(uint32_t nCount)
        {
            m_nMaxCandidates = nCount > 0 ? nCount : 1;
        }

        bool CLabelDrawer::GetKeepInsideView() const
        {
            return m_bKeepInsideView;
        }

        void CLabelDrawer::SetKeepInsideView(bool bKeep)
        {
            m_bKeepInsideView = bKeep;
        }

        void CLabelDrawer::Save(CommonLib::ISerializeObjPtr pObj) const
        {
            pObj->AddPropertyDouble("LabelBuffer", m_dLabelBuffer);
            pObj->AddPropertyDouble("SearchStep", m_dSearchStep);
            pObj->AddPropertyInt32U("MaxCandidates", m_nMaxCandidates);
            pObj->AddPropertyBool("KeepInsideView", m_bKeepInsideView);
        }

        void CLabelDrawer::Load(CommonLib::ISerializeObjPtr pObj)
        {
            SetLabelBuffer(pObj->GetPropertyDouble("LabelBuffer", m_dLabelBuffer));
            SetSearchStep(pObj->GetPropertyDouble("SearchStep", m_dSearchStep));
            SetMaxCandidates(pObj->GetPropertyInt32U("MaxCandidates", m_nMaxCandidates));
            SetKeepInsideView(pObj->GetPropertyBool("KeepInsideView", m_bKeepInsideView));
        }
    }
}
