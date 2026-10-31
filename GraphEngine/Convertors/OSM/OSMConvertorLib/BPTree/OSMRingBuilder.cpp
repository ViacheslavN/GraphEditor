#include "OSMRingBuilder.h"

#include <algorithm>
#include <cmath>
#include <numeric>
#include <unordered_map>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            namespace
            {
                inline uint64_t PointKey(const SOSMPointE7& point) { return ((uint64_t)(uint32_t)point.lat << 32) | (uint32_t)point.lon; }

                // the consecutive duplicate points dropped
                void Dedupe(std::vector<SOSMPointE7>& ring)
                {
                    ring.erase(std::unique(ring.begin(), ring.end()), ring.end());
                }

                inline bool IsClosedRing(const std::vector<SOSMPointE7>& ring)
                {
                    return ring.size() >= 4 && ring.front() == ring.back();
                }
            }

            void CRingBuilder::Clear()
            {
                m_ways.clear();
                m_rings.clear();
                m_nRings = m_nHoles = m_nUnclosed = 0;
            }

            void CRingBuilder::AddWay(const SOSMPointE7* pPoints, size_t nPoints)
            {
                if (nPoints < 2)
                    return;
                TRing way(pPoints, pPoints + nPoints);
                Dedupe(way);
                if (way.size() < 2)
                    return;
                if (way.front() == way.back())
                {
                    if (IsClosedRing(way))
                        m_rings.push_back(std::move(way));
                    else
                        m_nUnclosed += 1;    // a degenerate ring
                    return;
                }
                m_ways.push_back(std::move(way));
            }

            void CRingBuilder::JoinWays()
            {
                // the ways by their end points
                std::unordered_multimap<uint64_t, uint32_t> ends;
                ends.reserve(m_ways.size() * 2);
                for (uint32_t i = 0; i < (uint32_t)m_ways.size(); ++i)
                {
                    ends.emplace(PointKey(m_ways[i].front()), i);
                    ends.emplace(PointKey(m_ways[i].back()), i);
                }
                std::vector<uint8_t> used(m_ways.size(), 0);
                for (uint32_t nStart = 0; nStart < (uint32_t)m_ways.size(); ++nStart)
                {
                    if (used[nStart])
                        continue;
                    used[nStart] = 1;
                    TRing ring = m_ways[nStart];
                    uint32_t nJoined = 1;
                    bool bClosed = false;
                    for (;;)
                    {
                        if (ring.size() >= 4 && ring.front() == ring.back())
                        {
                            bClosed = true;
                            break;
                        }
                        // an unused way starting or ending at the end of the ring
                        auto range = ends.equal_range(PointKey(ring.back()));
                        uint32_t nNext = (uint32_t)-1;
                        for (auto it = range.first; it != range.second; ++it)
                        {
                            if (!used[it->second])
                            {
                                nNext = it->second;
                                break;
                            }
                        }
                        if (nNext == (uint32_t)-1)
                            break;
                        used[nNext] = 1;
                        ++nJoined;
                        const TRing& next = m_ways[nNext];
                        if (next.front() == ring.back())
                            ring.insert(ring.end(), next.begin() + 1, next.end());
                        else
                            ring.insert(ring.end(), next.rbegin() + 1, next.rend());
                    }
                    if (bClosed)
                    {
                        Dedupe(ring);
                        if (IsClosedRing(ring))
                        {
                            m_rings.push_back(std::move(ring));
                            continue;
                        }
                    }
                    m_nUnclosed += nJoined;
                }
                m_ways.clear();
            }

            double CRingBuilder::SignedArea(const TRing& ring)
            {
                // relative to the first point (the products stay small)
                double dArea = 0.;
                const double x0 = ring[0].lon, y0 = ring[0].lat;
                for (size_t i = 0; i + 1 < ring.size(); ++i)
                {
                    double x1 = ring[i].lon - x0, y1 = ring[i].lat - y0;
                    double x2 = ring[i + 1].lon - x0, y2 = ring[i + 1].lat - y0;
                    dArea += x1 * y2 - x2 * y1;
                }
                return dArea / 2.;
            }

            int CRingBuilder::PointInRing(const SOSMPointE7& point, const TRing& ring)
            {
                bool bInside = false;
                const double px = point.lon, py = point.lat;
                for (size_t i = 0, j = ring.size() - 1; i < ring.size(); j = i++)
                {
                    const SOSMPointE7& a = ring[i];
                    const SOSMPointE7& b = ring[j];
                    if (a == point)
                        return -1;
                    if ((a.lat > point.lat) != (b.lat > point.lat))
                    {
                        // the x of the edge at the y of the point, relative to the point
                        double ax = a.lon - px, ay = a.lat - py, bx = b.lon - px, by = b.lat - py;
                        double x = ax + (0. - ay) * (bx - ax) / (by - ay);
                        if (x > 0.)
                            bInside = !bInside;
                    }
                }
                return bInside ? 1 : 0;
            }

            bool CRingBuilder::Inside(const TRing& a, const TRing& b)
            {
                for (size_t i = 0; i + 1 < a.size(); ++i)
                {
                    int nResult = PointInRing(a[i], b);
                    if (nResult >= 0)
                        return nResult == 1;
                }
                return false;     // all the vertices are shared: the same ring
            }

            bool CRingBuilder::Build(std::vector<int64_t>& xs, std::vector<int64_t>& ys, std::vector<uint32_t>& parts)
            {
                xs.clear();
                ys.clear();
                parts.clear();
                JoinWays();
                m_nRings = (uint32_t)m_rings.size();
                m_nHoles = 0;
                if (m_rings.empty())
                    return false;

                // the rings from the biggest one: the parent of a ring is the last bigger ring containing it
                std::vector<SRingInfo> infos(m_rings.size());
                for (size_t i = 0; i < m_rings.size(); ++i)
                {
                    SRingInfo& info = infos[i];
                    info.dArea = SignedArea(m_rings[i]);
                    info.minLon = info.maxLon = m_rings[i][0].lon;
                    info.minLat = info.maxLat = m_rings[i][0].lat;
                    for (const SOSMPointE7& point : m_rings[i])
                    {
                        info.minLon = (std::min)(info.minLon, point.lon);
                        info.maxLon = (std::max)(info.maxLon, point.lon);
                        info.minLat = (std::min)(info.minLat, point.lat);
                        info.maxLat = (std::max)(info.maxLat, point.lat);
                    }
                }
                std::vector<uint32_t> order(m_rings.size());
                std::iota(order.begin(), order.end(), 0u);
                std::stable_sort(order.begin(), order.end(), [&](uint32_t a, uint32_t b) { return std::fabs(infos[a].dArea) > std::fabs(infos[b].dArea); });
                for (size_t k = 1; k < order.size(); ++k)
                {
                    SRingInfo& info = infos[order[k]];
                    for (size_t m = k; m-- > 0;)
                    {
                        const SRingInfo& candidate = infos[order[m]];
                        if (candidate.minLon > info.minLon || candidate.maxLon < info.maxLon || candidate.minLat > info.minLat || candidate.maxLat < info.maxLat)
                            continue;
                        if (Inside(m_rings[order[k]], m_rings[order[m]]))
                        {
                            info.nParent = (int)order[m];
                            info.nDepth = candidate.nDepth + 1;
                            break;
                        }
                    }
                }

                // the holes of every outer ring
                std::vector<std::vector<uint32_t> > holes(m_rings.size());
                for (uint32_t nRing : order)
                    if (infos[nRing].nDepth % 2 == 1)
                    {
                        holes[infos[nRing].nParent].push_back(nRing);
                        ++m_nHoles;
                    }

                auto emit = [&](uint32_t nRing, bool bOuter)
                {
                    const TRing& ring = m_rings[nRing];
                    // outer: clockwise (a negative area), hole: counterclockwise
                    bool bReverse = (infos[nRing].dArea > 0.) == bOuter;
                    for (size_t p = 0; p < ring.size(); ++p)
                    {
                        const SOSMPointE7& point = bReverse ? ring[ring.size() - 1 - p] : ring[p];
                        xs.push_back(point.lon);
                        ys.push_back(point.lat);
                    }
                    parts.push_back((uint32_t)ring.size());
                };
                for (uint32_t nRing : order)
                {
                    if (infos[nRing].nDepth % 2 != 0)
                        continue;
                    emit(nRing, true);
                    for (uint32_t nHole : holes[nRing])
                        emit(nHole, false);
                }
                m_rings.clear();
                return true;
            }
        }
    }
}
