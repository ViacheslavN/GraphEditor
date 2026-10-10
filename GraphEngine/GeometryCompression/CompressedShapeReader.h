#pragma once
// Sequential reader of a compressed shape blob (ShapeFormat.h): the header is parsed by Open, the parts and
// the points are decoded one by one in place, without a buffer for the coordinates. The parts and the points
// have their own cursors, so NextPart / NextPoint can be mixed as the drawing does.
// Header only: CGeoShape (CommonLib) reads compressed blobs with it.
// A damaged blob throws CommonLib::CExcBase (Open, NextPoint).

#include "ShapeFormat.h"
#include "ValueCoder.h"
#include "../CommonLib/SpatialData/IGeoShape.h"
#include <cstring>

namespace GraphEngine
{
    namespace GeometryCompression
    {
        class CCompressedShapeReader
        {
        public:
            CCompressedShapeReader() { Close(); }

            // the blob must live while the reader is used; throws when the blob isn't a compressed shape or is damaged
            void Open(const TByte* pData, size_t nSize)
            {
                Close();
                try
                {
                    if(!IsCompressedShape(pData, nSize))
                        throw CommonLib::CExcBase("not a compressed shape ({0} bytes)", nSize);

                    int16_t nType = 0;
                    memcpy(&nType, pData + 1, sizeof(nType));
                    m_type = (CommonLib::eShapeType)nType;
                    m_generalType = CommonLib::IGeoShape::GeneralType(m_type);

                    TByte nOptions = pData[3];
                    if((nOptions >> ShapeOptionVersionShift) != ShapeFormatVersion)
                        throw CommonLib::CExcBase("unknown format version {0}", (int)(nOptions >> ShapeOptionVersionShift));
                    m_coding = (eValueCoding)(nOptions & ShapeOptionCodingMask);
                    m_bClosed = (nOptions & ShapeOptionClosedRings) != 0;

                    m_nScaleExp = (int8_t)pData[4];
                    if(m_nScaleExp < -22 || m_nScaleExp > 22)
                        throw CommonLib::CExcBase("wrong scale exponent {0}", m_nScaleExp);
                    m_bDivide = m_nScaleExp >= 0;
                    m_dScale = Pow10(m_nScaleExp >= 0 ? m_nScaleExp : -m_nScaleExp);

                    m_pData = pData;
                    m_nSize = nSize;

                    CByteReader reader(pData + ShapeHeaderSize, nSize - ShapeHeaderSize);
                    switch(m_generalType)
                    {
                        case CommonLib::shape_type_general_point:
                            m_xMin = m_xMax = reader.ReadZigZag();
                            m_yMin = m_yMax = reader.ReadZigZag();
                            reader.CheckError("the point");
                            m_nPoints = 1;
                            m_bOpen = true;
                            RewindParts();
                            RewindPoints();
                            return;

                        case CommonLib::shape_type_general_multipoint:
                            m_nPoints = (uint32_t)reader.ReadVarint();
                            break;

                        case CommonLib::shape_type_general_polyline:
                        case CommonLib::shape_type_general_polygon:
                        {
                            m_nPoints = (uint32_t)reader.ReadVarint();
                            m_nParts = (uint32_t)reader.ReadVarint();
                            m_pPartSizes = reader.Pos();
                            uint64_t nSum = 0;
                            for(uint32_t i = 1; i < m_nParts && !reader.IsError(); ++i)
                            {
                                uint64_t nPart = reader.ReadVarint();
                                if(m_bClosed && nPart < 2)
                                    throw CommonLib::CExcBase("ring {0} has {1} points", i - 1, nPart);
                                nSum += nPart;
                            }
                            reader.CheckError("the parts");
                            m_nPartSizesBytes = (size_t)(reader.Pos() - m_pPartSizes);
                            if(nSum > m_nPoints || (m_bClosed && (m_nParts == 0 || m_nPoints - nSum < 2)))
                                throw CommonLib::CExcBase("the parts have {0} points of {1}", nSum, m_nPoints);
                            break;
                        }

                        default:
                            throw CommonLib::CExcBase("unsupported shape type {0}", (int)m_type);
                    }

                    m_xMin = reader.ReadZigZag();
                    m_yMin = reader.ReadZigZag();
                    m_xMax = m_xMin + (int64_t)reader.ReadVarint();
                    m_yMax = m_yMin + (int64_t)reader.ReadVarint();
                    reader.CheckError("the bounding box");

                    uint64_t nStored = m_nPoints - (m_bClosed ? m_nParts : 0);   // the closing points of the rings aren't stored
                    m_values.Open(m_coding, reader, 2 * nStored);

                    m_bOpen = true;
                    RewindParts();
                    RewindPoints();
                }
                catch (std::exception& exc)
                {
                    Close();
                    CommonLib::CExcBase::RegenExc("Failed to open a compressed shape", exc);
                    throw;
                }
            }

            void Close()
            {
                m_bOpen = false;
                m_pData = nullptr;
                m_nSize = 0;
                m_type = CommonLib::shape_type_null;
                m_generalType = CommonLib::shape_type_null;
                m_nPoints = 0;
                m_nParts = 0;
                m_nScaleExp = 0;
                m_dScale = 1.;
                m_bDivide = true;
                m_coding = ValueCodingVarint;
                m_bClosed = false;
                m_xMin = m_yMin = m_xMax = m_yMax = 0;
                m_pPartSizes = nullptr;
                m_nPartSizesBytes = 0;
                m_nNextPart = 0;
                m_nNextPoint = 0;
                m_nPartLeft = 0;
                m_x = m_y = m_xFirst = m_yFirst = 0;
            }

            bool IsOpen() const { return m_bOpen; }

            CommonLib::eShapeType Type() const { return m_type; }
            CommonLib::eShapeType GeneralType() const { return m_generalType; }
            uint32_t PointCount() const { return m_nPoints; }
            uint32_t PartCount() const { return m_nParts; }
            int      ScaleExponent() const { return m_nScaleExp; }
            eValueCoding Coding() const { return m_coding; }
            bool     IsClosedRings() const { return m_bClosed; }

            CommonLib::bbox BoundingBox() const
            {
                CommonLib::bbox bb;
                if(!m_bOpen || m_nPoints == 0)
                    return bb;
                bb.type = CommonLib::bbox_type_normal;
                bb.xMin = ToCoord(m_xMin);
                bb.yMin = ToCoord(m_yMin);
                bb.xMax = ToCoord(m_xMax);
                bb.yMax = ToCoord(m_yMax);
                return bb;
            }

            // the number of points of a part (scans the part sizes)
            uint32_t PartSize(uint32_t nIndex) const
            {
                if(nIndex >= m_nParts)
                    return 0;
                SPartCursor cursor;
                StartParts(cursor);
                uint32_t nSize = 0;
                for(uint32_t i = 0; i <= nIndex; ++i)
                    nSize = ReadPart(cursor);
                return nSize;
            }

            // parts in order: the size of the next part, 0 - no more parts
            void RewindParts()
            {
                StartParts(m_parts);
                m_nNextPart = 0;
            }
            uint32_t NextPartIndex() const { return m_nNextPart; }
            uint32_t NextPart()
            {
                if(m_nNextPart >= m_nParts)
                    return 0;
                ++m_nNextPart;
                return ReadPart(m_parts);
            }

            // points in order, false - no more points
            void RewindPoints()
            {
                if(m_bOpen && m_generalType != CommonLib::shape_type_general_point)
                    m_values.Rewind();
                StartParts(m_pointParts);
                m_nNextPoint = 0;
                m_nPartLeft = 0;
                m_x = m_xMin;
                m_y = m_yMin;
                m_xFirst = m_yFirst = 0;
            }
            uint32_t NextPointIndex() const { return m_nNextPoint; }

            bool NextPoint(CommonLib::GisXYPoint& pt)
            {
                int64_t x, y;
                if(!NextGridPoint(x, y))
                    return false;
                pt.x = ToCoord(x);
                pt.y = ToCoord(y);
                return true;
            }

            bool SkipPoints(uint32_t nCount)
            {
                int64_t x, y;
                for(uint32_t i = 0; i < nCount; ++i)
                {
                    if(!NextGridPoint(x, y))
                        return false;
                }
                return true;
            }

            // all the points / the starts of the parts (pPartStarts can be null) into the arrays of the caller
            void ReadAll(CommonLib::GisXYPoint* pPoints, uint32_t* pPartStarts)
            {
                if(!m_bOpen)
                    throw CommonLib::CExcBase("Compressed shape isn't opened");

                if(pPartStarts)
                {
                    SPartCursor cursor;
                    StartParts(cursor);
                    uint32_t nStart = 0;
                    for(uint32_t i = 0; i < m_nParts; ++i)
                    {
                        pPartStarts[i] = nStart;
                        nStart += ReadPart(cursor);
                    }
                }

                RewindPoints();
                for(uint32_t i = 0; i < m_nPoints; ++i)
                {
                    if(!NextPoint(pPoints[i]))
                        throw CommonLib::CExcBase("Compressed shape is damaged: {0} points of {1} are read", i, m_nPoints);
                }
                RewindPoints();
            }

        private:
            // a cursor over the part sizes
            struct SPartCursor
            {
                CByteReader reader;
                uint32_t    nIndex = 0;
                uint32_t    nUsed = 0;    // points of the parts read
            };

            void StartParts(SPartCursor& cursor) const
            {
                cursor.reader.Attach(m_pPartSizes, m_nPartSizesBytes);
                cursor.nIndex = 0;
                cursor.nUsed = 0;
            }

            uint32_t ReadPart(SPartCursor& cursor) const
            {
                if(cursor.nIndex >= m_nParts)
                    return 0;
                uint32_t nSize = cursor.nIndex + 1 < m_nParts ? (uint32_t)cursor.reader.ReadVarint() : m_nPoints - cursor.nUsed;
                ++cursor.nIndex;
                cursor.nUsed += nSize;
                return nSize;
            }

            bool NextGridPoint(int64_t& x, int64_t& y)
            {
                if(!m_bOpen || m_nNextPoint >= m_nPoints)
                    return false;

                if(m_generalType == CommonLib::shape_type_general_point)
                {
                    x = m_xMin;
                    y = m_yMin;
                    ++m_nNextPoint;
                    return true;
                }

                bool bPartStart = false;
                if(m_nPartLeft == 0)
                {
                    if(m_nParts > 0)
                    {
                        while(m_nPartLeft == 0 && m_pointParts.nIndex < m_nParts)
                            m_nPartLeft = ReadPart(m_pointParts);   // empty parts are skipped
                        if(m_nPartLeft == 0)
                            return false;
                    }
                    else
                        m_nPartLeft = m_nPoints - m_nNextPoint;
                    bPartStart = true;
                }

                if(m_bClosed && m_nPartLeft == 1)
                {
                    // the closing point of a ring isn't stored
                    x = m_xFirst;
                    y = m_yFirst;
                }
                else
                {
                    m_x += UnZigZag(m_values.Next());
                    m_y += UnZigZag(m_values.Next());
                    x = m_x;
                    y = m_y;
                    if(bPartStart)
                    {
                        m_xFirst = x;
                        m_yFirst = y;
                    }
                }

                --m_nPartLeft;
                ++m_nNextPoint;
                return true;
            }

            double ToCoord(int64_t v) const { return m_bDivide ? (double)v / m_dScale : (double)v * m_dScale; }

        private:
            bool        m_bOpen;
            const TByte* m_pData;
            size_t      m_nSize;
            CommonLib::eShapeType m_type;
            CommonLib::eShapeType m_generalType;
            uint32_t    m_nPoints;
            uint32_t    m_nParts;
            int         m_nScaleExp;
            double      m_dScale;
            bool        m_bDivide;
            eValueCoding m_coding;
            bool        m_bClosed;
            int64_t     m_xMin, m_yMin, m_xMax, m_yMax;
            const TByte* m_pPartSizes;   // nParts - 1 varints
            size_t      m_nPartSizesBytes;

            SPartCursor m_parts;         // NextPart
            uint32_t    m_nNextPart;

            CValueDecoder m_values;      // NextPoint
            SPartCursor m_pointParts;
            uint32_t    m_nNextPoint;
            uint32_t    m_nPartLeft;     // points left in the current part
            int64_t     m_x, m_y;        // the last decoded point
            int64_t     m_xFirst, m_yFirst;   // the first point of the current part
        };
    }
}
