#include "LabelStrategy.h"

namespace GraphEngine {
    namespace Cartography {
        namespace Labeling {

            namespace
            {
                struct SPointPosition
                {
                    int position;
                    int priority;
                };

                // shifts of the label from a line: 0 - on the line, -1 - above, 1 - below
                void LineShifts(eLineLabelPosition position, std::vector<double>& shifts)
                {
                    shifts.clear();
                    switch(position)
                    {
                        case LineLabelPositionAbove:
                            shifts.push_back(-1.);
                            break;
                        case LineLabelPositionBelow:
                            shifts.push_back(1.);
                            break;
                        case LineLabelPositionAboveBelow:
                            shifts.push_back(-1.);
                            shifts.push_back(1.);
                            break;
                        default:
                            shifts.push_back(0.);
                            break;
                    }
                }
            }

            void CLabelStrategy::GenerateCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                if(item.parts.empty() || !item.style.get() || item.width <= 0.)
                    return;

                switch(item.type)
                {
                    case LabelGeometryPoint:
                        PointCandidates(item, params, func);
                        break;
                    case LabelGeometryLine:
                        LineCandidates(item, params, func);
                        break;
                    case LabelGeometryPolygon:
                        PolygonCandidates(item, params, func);
                        break;
                }
            }

            SGlyphRun CLabelStrategy::MakeStraightRun(const SLabelItem& item, double cx, double cy, double angle)
            {
                // u = (c, s) - along the text, v = (-s, c) - down across the text;
                // the box center is at origin + u * width / 2 + v * (descent - ascent) / 2
                double rad = LabelDegToRad(angle);
                double c = cos(rad);
                double s = sin(rad);
                double k = (item.style->Descent() - item.style->Ascent()) / 2.;

                SGlyphRun run;
                run.start = 0;
                run.count = (int)item.text.length();
                run.x = cx - c * item.width / 2. + s * k;
                run.y = cy - s * item.width / 2. - c * k;
                run.angle = angle;
                run.box = SLabelBox::FromCenter(cx, cy, item.width, item.style->Height(), angle);
                return run;
            }

            void CLabelStrategy::PositionsFromMiddle(double from, double to, double step, std::vector<double>& positions)
            {
                positions.clear();
                if(to < from)
                    return;

                double middle = (from + to) / 2.;
                positions.push_back(middle);
                if(step <= 0.)
                    return;

                for(double d = step; middle - d >= from || middle + d <= to; d += step)
                {
                    if(middle - d >= from)
                        positions.push_back(middle - d);
                    if(middle + d <= to)
                        positions.push_back(middle + d);
                }
            }

            // ---------------- point ----------------

            bool CLabelStrategy::PointCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                const TLabelPoints& points = item.parts[0];
                if(points.empty())
                    return false;

                // stable order: positions with the same priority keep their order
                // (the reference sorted them with std::sort, the order of equal priorities was undefined)
                std::vector<SPointPosition> positions;
                for(int i = 0; i < PointLabelPositionCount; ++i)
                {
                    if(item.options.m_pointPriorities[i] > 0)
                        positions.push_back(SPointPosition{i, item.options.m_pointPriorities[i]});
                }
                std::stable_sort(positions.begin(), positions.end(), [](const SPointPosition& a, const SPointPosition& b) { return a.priority < b.priority; });
                if(positions.empty())
                    positions.push_back(SPointPosition{PointLabelRightTop, 1});

                const SLabelPoint& pt = points[0];
                double w2 = item.width / 2.;
                double h2 = item.style->Height() / 2.;
                double g = params.offset;
                double gd = g * 0.7;   // diagonal positions

                SLabelPlacement placement;
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    double cx = pt.x, cy = pt.y;
                    switch(positions[i].position)
                    {
                        case PointLabelLeftTop:      cx -= gd + w2; cy -= gd + h2; break;
                        case PointLabelCenterTop:    cy -= g + h2; break;
                        case PointLabelRightTop:     cx += gd + w2; cy -= gd + h2; break;
                        case PointLabelRightCenter:  cx += g + w2; break;
                        case PointLabelRightBottom:  cx += gd + w2; cy += gd + h2; break;
                        case PointLabelCenterBottom: cy += g + h2; break;
                        case PointLabelLeftBottom:   cx -= gd + w2; cy += gd + h2; break;
                        case PointLabelLeftCenter:   cx -= g + w2; break;
                        default: break;
                    }

                    placement.runs.assign(1, MakeStraightRun(item, cx, cy, 0.));
                    if(func(placement))
                        return true;
                }
                return false;
            }

            // ---------------- line ----------------

            bool CLabelStrategy::LineCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                std::vector<CLabelPath> paths;
                for(size_t i = 0; i < item.parts.size(); ++i)
                {
                    CLabelPath path(item.parts[i]);
                    if(path.IsValid())
                        paths.push_back(path);
                }
                // the longest visible part first (the reference used only the longest one)
                std::stable_sort(paths.begin(), paths.end(), [](const CLabelPath& a, const CLabelPath& b) { return a.Length() > b.Length(); });

                std::vector<double> shifts;
                LineShifts(item.options.m_linePosition, shifts);

                for(size_t i = 0; i < paths.size(); ++i)
                {
                    bool stop = false;
                    switch(item.options.m_lineOrientation)
                    {
                        case LineLabelOrientationHorizontal:
                            stop = HorizontalLine(item, paths[i], params, shifts, func);
                            break;
                        case LineLabelOrientationParallel:
                            stop = ParallelLine(item, paths[i], params, shifts, func);
                            break;
                        case LineLabelOrientationCurved:
                            stop = CurvedLine(item, paths[i], params, shifts, func);
                            break;
                        case LineLabelOrientationPerpendicular:
                            stop = PerpendicularLine(item, paths[i], params, func);
                            break;
                    }
                    if(stop)
                        return true;
                }
                return false;
            }

            bool CLabelStrategy::HorizontalLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params,
                                                const std::vector<double>& shifts, const TLabelCandidateFunc& func)
            {
                if(path.Length() < item.width * 0.7)  // too short for the text
                    return false;

                std::vector<double> positions;
                PositionsFromMiddle(0., path.Length(), params.step, positions);

                double shiftSize = item.style->Height() / 2. + params.offset;
                SLabelPlacement placement;
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    SLabelPoint pt = path.PointAt(positions[i]);
                    for(size_t s = 0; s < shifts.size(); ++s)
                    {
                        placement.runs.assign(1, MakeStraightRun(item, pt.x, pt.y + shifts[s] * shiftSize, 0.));
                        if(func(placement))
                            return true;
                    }
                }
                return false;
            }

            bool CLabelStrategy::ParallelLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params,
                                              const std::vector<double>& shifts, const TLabelCandidateFunc& func)
            {
                double half = item.width / 2.;
                if(path.Length() < item.width)
                    return false;

                std::vector<double> positions;
                PositionsFromMiddle(half, path.Length() - half, params.step, positions);

                double height = item.style->Height();
                double maxDeviation = (std::max)(2., height / 2.);
                double shiftSize = height / 2. + params.offset;
                SLabelPlacement placement;
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    double d = positions[i];
                    // the text is straight: the line under it must be nearly straight too
                    if(path.Deviation(d - half, d + half) > maxDeviation)
                        continue;

                    SLabelPoint a = path.PointAt(d - half);
                    SLabelPoint b = path.PointAt(d + half);
                    double angle = NormalizeReadableAngle(LabelRadToDeg(atan2(b.y - a.y, b.x - a.x)));
                    double rad = LabelDegToRad(angle);
                    double vx = -sin(rad), vy = cos(rad);
                    double mx = (a.x + b.x) / 2., my = (a.y + b.y) / 2.;
                    for(size_t s = 0; s < shifts.size(); ++s)
                    {
                        double shift = shifts[s] * shiftSize;
                        placement.runs.assign(1, MakeStraightRun(item, mx + vx * shift, my + vy * shift, angle));
                        if(func(placement))
                            return true;
                    }
                }
                return false;
            }

            bool CLabelStrategy::MakeCurvedPlacement(const SLabelItem& item, const CLabelPath& path, double start, double shift,
                                                     double maxCharAngle, SLabelPlacement& placement)
            {
                placement.runs.clear();

                const CLabelStyle& style = *item.style;
                double height = style.Height();
                double k = (style.Descent() - style.Ascent()) / 2.;

                // the text is a bit wider than the sum of the characters (no kerning), it is stretched to the text width
                double sumWidth = 0.;
                for(size_t i = 0; i < item.text.length(); ++i)
                    sumWidth += style.CharWidth(item.text[i]);
                if(sumWidth <= 0.)
                    return false;
                double scale = item.width / sumWidth;

                double pos = start;
                double prevAngle = 0.;
                for(size_t i = 0; i < item.text.length(); ++i)
                {
                    double cw = style.CharWidth(item.text[i]);
                    double adv = cw * scale;
                    double angle = 0.;
                    SLabelPoint a = path.PointAt(pos);
                    SLabelPoint b = path.PointAt(pos + adv);
                    if(adv > 0.)
                        angle = LabelRadToDeg(atan2(b.y - a.y, b.x - a.x));
                    else
                        path.PointAt(pos, &angle);

                    if(i > 0 && fabs(AngleDifference(angle, prevAngle)) > maxCharAngle)
                        return false;
                    prevAngle = angle;

                    double rad = LabelDegToRad(angle);
                    double c = cos(rad), s = sin(rad);
                    // center of the character box: in the middle of the chord, moved across the line by the shift
                    double cx = a.x + c * cw / 2. - s * shift;
                    double cy = a.y + s * cw / 2. + c * shift;

                    SGlyphRun run;
                    run.start = (int)i;
                    run.count = 1;
                    run.x = cx - c * cw / 2. + s * k;
                    run.y = cy - s * cw / 2. - c * k;
                    run.angle = angle;
                    run.box = SLabelBox::FromCenter(cx, cy, cw, height, angle);
                    placement.runs.push_back(run);

                    pos += adv;
                }
                return true;
            }

            bool CLabelStrategy::CurvedLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params,
                                            const std::vector<double>& shifts, const TLabelCandidateFunc& func)
            {
                double length = path.Length();
                double half = item.width / 2.;
                if(length < item.width)
                    return false;

                CLabelPath reversed = path.Reversed();
                std::vector<double> positions;
                PositionsFromMiddle(half, length - half, params.step, positions);

                double shiftSize = item.style->Height() / 2. + params.offset;
                SLabelPlacement placement;
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    double start = positions[i] - half;

                    // the text goes along the line in the direction which keeps it readable (left to right)
                    SLabelPoint a = path.PointAt(start);
                    SLabelPoint b = path.PointAt(start + item.width);
                    double chordAngle = LabelRadToDeg(atan2(b.y - a.y, b.x - a.x));
                    bool reverse = chordAngle > 90. || chordAngle <= -90.;
                    const CLabelPath& textPath = reverse ? reversed : path;
                    double textStart = reverse ? length - start - item.width : start;

                    for(size_t s = 0; s < shifts.size(); ++s)
                    {
                        if(!MakeCurvedPlacement(item, textPath, textStart, shifts[s] * shiftSize, params.maxCharAngle, placement))
                            continue;
                        if(func(placement))
                            return true;
                    }
                }
                return false;
            }

            bool CLabelStrategy::PerpendicularLine(const SLabelItem& item, const CLabelPath& path, const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                std::vector<double> positions;
                PositionsFromMiddle(0., path.Length(), params.step, positions);

                double dist = item.width / 2. + params.offset;
                SLabelPlacement placement;
                for(size_t i = 0; i < positions.size(); ++i)
                {
                    double lineAngle = 0.;
                    SLabelPoint pt = path.PointAt(positions[i], &lineAngle);
                    double angle = NormalizeReadableAngle(lineAngle + 90.);
                    double rad = LabelDegToRad(angle);
                    double ux = cos(rad), uy = sin(rad);

                    // the text starts next to the line, on one side or the other
                    const double sides[2] = {1., -1.};
                    for(int side = 0; side < 2; ++side)
                    {
                        placement.runs.assign(1, MakeStraightRun(item, pt.x + ux * dist * sides[side], pt.y + uy * dist * sides[side], angle));
                        if(func(placement))
                            return true;
                    }
                }
                return false;
            }

            // ---------------- polygon ----------------

            bool CLabelStrategy::PolygonCandidates(const SLabelItem& item, const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                CLabelPolygon polygon;
                for(size_t i = 0; i < item.parts.size(); ++i)
                    polygon.AddRing(item.parts[i]);
                if(polygon.IsEmpty())
                    return false;

                double cx = (polygon.XMin() + polygon.XMax()) / 2.;
                double cy = (polygon.YMin() + polygon.YMax()) / 2.;

                ePolygonLabelPlacement placementType = item.options.m_polygonPlacement;
                if(placementType != PolygonLabelPlacementHorizontal)
                {
                    double axisAngle = 0., elongation = 0.;
                    polygon.MainAxis(&axisAngle, &elongation);
                    axisAngle = NormalizeReadableAngle(axisAngle);

                    // a roundish polygon has no main direction, the text is horizontal
                    const double minElongation = 0.3;
                    const double minAngle = 3.;
                    if(elongation >= minElongation && fabs(axisAngle) >= minAngle)
                    {
                        if(PolygonInFrame(item, polygon, axisAngle, cx, cy, params, func))
                            return true;
                        if(placementType == PolygonLabelPlacementStraight)
                            return false;
                    }
                }

                return PolygonInFrame(item, polygon, 0., cx, cy, params, func);
            }

            bool CLabelStrategy::PolygonInFrame(const SLabelItem& item, const CLabelPolygon& devPolygon, double angle, double cx, double cy,
                                                const SLabelStrategyParams& params, const TLabelCandidateFunc& func)
            {
                // the polygon is rotated so that the text is horizontal in the frame
                CLabelPolygon polygon = angle != 0. ? devPolygon.Rotated(cx, cy, -angle) : devPolygon;

                double width = item.width;
                double height = item.style->Height();
                double margin = item.style->HaloSize() + 2.;   // the text doesn't touch the border
                bool allowOutside = item.options.m_bPolygonAllowOutside;

                if(!allowOutside && (polygon.XMax() - polygon.XMin() < width || polygon.YMax() - polygon.YMin() < height))
                    return false;

                double rad = LabelDegToRad(angle);
                double c = cos(rad), s = sin(rad);
                SLabelPlacement placement;
                auto tryCenter = [&](double fx, double fy, bool checkInside) -> int
                {
                    // 1 - stop, 0 - continue, -1 - doesn't fit
                    if(checkInside && !polygon.ContainsBox(SLabelBox::FromCenter(fx, fy, width + 2. * margin, height + 2. * margin, 0.)))
                        return -1;
                    double dx = fx - cx, dy = fy - cy;
                    placement.runs.assign(1, MakeStraightRun(item, cx + dx * c - dy * s, cy + dx * s + dy * c, angle));
                    return func(placement) ? 1 : 0;
                };

                // the pole of inaccessibility is the most "inside" point, the best position for the label
                SLabelPoint pole = polygon.PoleOfInaccessibility((std::max)(1., height / 4.));
                if(tryCenter(pole.x, pole.y, true) == 1)
                    return true;

                // horizontal bands from the pole to the borders, the whole band of the text height
                // must be inside (the reference checked only the base line, so the text could cross the border)
                double halfH = height / 2. + margin;
                double halfW = width / 2. + margin;
                std::vector<double> bandsY;
                std::vector<double> ys;
                PositionsFromMiddle(polygon.YMin() + halfH, polygon.YMax() - halfH, params.step, ys);
                for(size_t i = 0; i < ys.size(); ++i)
                    bandsY.push_back(ys[i]);
                // nearest to the pole first
                std::stable_sort(bandsY.begin(), bandsY.end(), [&](double a, double b) { return fabs(a - pole.y) < fabs(b - pole.y); });

                std::vector<std::pair<double, double> > band, line, merged;
                const double lineOffsets[3] = {-halfH + 0.5, 0., halfH - 0.5};
                for(size_t i = 0; i < bandsY.size(); ++i)
                {
                    double y = bandsY[i];
                    polygon.ScanLine(y + lineOffsets[0], band);
                    for(int l = 1; l < 3 && !band.empty(); ++l)
                    {
                        polygon.ScanLine(y + lineOffsets[l], line);
                        merged.clear();
                        for(size_t a = 0; a < band.size(); ++a)
                        {
                            for(size_t b = 0; b < line.size(); ++b)
                            {
                                double from = (std::max)(band[a].first, line[b].first);
                                double to = (std::min)(band[a].second, line[b].second);
                                if(to - from >= 2. * halfW)
                                    merged.push_back(std::make_pair(from, to));
                            }
                        }
                        band.swap(merged);
                    }

                    // the intervals nearest to the pole first, in the interval the position nearest to the pole
                    std::stable_sort(band.begin(), band.end(), [&](const std::pair<double, double>& a, const std::pair<double, double>& b)
                    {
                        double da = pole.x < a.first ? a.first - pole.x : (pole.x > a.second ? pole.x - a.second : 0.);
                        double db = pole.x < b.first ? b.first - pole.x : (pole.x > b.second ? pole.x - b.second : 0.);
                        return da < db;
                    });

                    // a small gap from the interval ends: a box touching the border is rejected by the rounding errors
                    const double eps = 0.5;
                    for(size_t b = 0; b < band.size(); ++b)
                    {
                        if(band[b].second - band[b].first < 2. * (halfW + eps))
                            continue;
                        double x = (std::max)(band[b].first + halfW + eps, (std::min)(band[b].second - halfW - eps, pole.x));
                        if(tryCenter(x, y, true) == 1)
                            return true;
                    }
                }

                // the label doesn't fit: at the pole, crossing the border
                if(allowOutside && tryCenter(pole.x, pole.y, false) == 1)
                    return true;

                return false;
            }
        }
    }
}
