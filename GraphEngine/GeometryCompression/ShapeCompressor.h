#pragma once
// Compression of the shape geometry into the blob described in ShapeFormat.h (header only).
// The coordinates are rounded to 10^-k (the scale exponent): k = 2 keeps centimeters of a projected
// coordinate system, k = 7 keeps 1e-7 degrees (the precision of OpenStreetMap).
// A compressed blob is read by CGeoShape (Import), CGeoShape::Compress / Decompress switch the shape mode.

#include "ShapeFormat.h"
#include "ValueCoder.h"
#include "../CommonLib/SpatialData/IGeoShape.h"
#include "../CommonLib/SpatialData/Units.h"
#include <cmath>

namespace GraphEngine
{
    namespace GeometryCompression
    {
        struct SShapeCompressParams
        {
            int nScaleExponent = 2;   // -22..22
        };

        // the precision by the units of the coordinate system (as GIS-master CEnvelope::GetCompressParams):
        // degrees 1e-7 (~1 cm, the OSM precision), meters / feet / yards / inches / decimeters 1 cm,
        // kilometers / miles 1 m, millimeters 1 mm, unknown 1e-4
        inline SShapeCompressParams CompressParamsForUnits(CommonLib::Units units)
        {
            SShapeCompressParams params;
            switch(units)
            {
                case CommonLib::UnitsDecimalDegrees:
                    params.nScaleExponent = 7;
                    break;
                case CommonLib::UnitsKilometers:
                case CommonLib::UnitsMiles:
                case CommonLib::UnitsNauticalMiles:
                    params.nScaleExponent = 3;
                    break;
                case CommonLib::UnitsMeters:
                case CommonLib::UnitsYards:
                case CommonLib::UnitsFeet:
                case CommonLib::UnitsDecimeters:
                case CommonLib::UnitsInches:
                    params.nScaleExponent = 2;
                    break;
                case CommonLib::UnitsCentimeters:
                case CommonLib::UnitsMillimeters:
                    params.nScaleExponent = 1;
                    break;
                default:
                    params.nScaleExponent = 4;
                    break;
            }
            return params;
        }

        // the largest absolute coordinate of an extent, 0 - no extent
        inline double MaxAbsCoordinate(const CommonLib::bbox& extent)
        {
            double dMaxCoord = 0.;
            if(extent.type != CommonLib::bbox_type_normal)
                return dMaxCoord;
            for(double d : {extent.xMin, extent.xMax, extent.yMin, extent.yMax})
                dMaxCoord = std::fabs(d) > dMaxCoord ? std::fabs(d) : dMaxCoord;
            return dMaxCoord;
        }

        // the integers up to 2^53 are exact doubles: a coordinate rounded to 10^-k comes back from int64 exactly
        // while |coordinate| * 10^k <= 2^53
        const double MaxExactInteger = 9007199254740992.;   // 2^53

        // the maximum precision for an extent: the largest k (<= 22) which keeps all the coordinates of the extent
        // exact through int64 (|coordinate| * 10^k <= 2^53), it keeps about 15-16 significant digits of the doubles
        inline SShapeCompressParams MaxCompressParamsForExtent(const CommonLib::bbox& extent)
        {
            SShapeCompressParams params;
            double dMaxCoord = MaxAbsCoordinate(extent);
            params.nScaleExponent = 22;
            if(dMaxCoord <= 0.)
                return params;
            while(params.nScaleExponent > -22 && dMaxCoord * Pow10(params.nScaleExponent) > MaxExactInteger)
                --params.nScaleExponent;
            return params;
        }

        // the same for the extent of a dataset: the precision is lowered when the rounded coordinates
        // of the extent wouldn't be exact doubles any more (|coordinate| * 10^k > 2^53)
        inline SShapeCompressParams CompressParamsForExtent(const CommonLib::bbox& extent, CommonLib::Units units)
        {
            SShapeCompressParams params = CompressParamsForUnits(units);
            if(extent.type != CommonLib::bbox_type_normal)
                return params;

            SShapeCompressParams maxParams = MaxCompressParamsForExtent(extent);
            if(params.nScaleExponent > maxParams.nScaleExponent)
                params.nScaleExponent = maxParams.nScaleExponent;
            return params;
        }

        class CShapeCompressor
        {
        public:
            explicit CShapeCompressor(const SShapeCompressParams& params = SShapeCompressParams()) : m_params(params)
            {
                if(m_params.nScaleExponent < -22 || m_params.nScaleExponent > 22)
                    throw CommonLib::CExcBase("Shape compressor: wrong scale exponent {0}, expected -22..22", m_params.nScaleExponent);
                m_bMultiply = m_params.nScaleExponent >= 0;
                m_dFactor = Pow10(m_params.nScaleExponent >= 0 ? m_params.nScaleExponent : -m_params.nScaleExponent);
            }

            const SShapeCompressParams& GetParams() const { return m_params; }

            // points, multipoints, polylines, polygons without Z, M, curves, ids
            static bool IsSupported(CommonLib::eShapeType shapeType)
            {
                CommonLib::eShapeType genType = CommonLib::shape_type_null;
                bool bZ = false, bM = false, bCurve = false, bId = false;
                CommonLib::IGeoShape::GetTypeParams(shapeType, &genType, &bZ, &bM, &bCurve, &bId);
                if(bZ || bM || bCurve || bId)
                    return false;
                return genType == CommonLib::shape_type_general_point || genType == CommonLib::shape_type_general_multipoint ||
                       genType == CommonLib::shape_type_general_polyline || genType == CommonLib::shape_type_general_polygon;
            }

            // false - the shape isn't compressed: an unsupported type or a coordinate out of the integer range
            // (the caller keeps the shape as it is), out is unchanged; throws on a broken shape
            bool Compress(const CommonLib::IGeoShape& shape, std::vector<TByte>& out)
            {
                try
                {
                    return CompressImpl(shape, out);
                }
                catch (std::exception& exc)
                {
                    CommonLib::CExcBase::RegenExcT("Failed to compress a shape of type {0}", (int)shape.Type(), exc);
                    throw;
                }
            }

        private:
            bool Quantize(double dValue, int64_t& nValue) const
            {
                double d = m_bMultiply ? dValue * m_dFactor : dValue / m_dFactor;
                if(!std::isfinite(d) || std::fabs(d) > 4.0e18)
                    return false;
                nValue = (int64_t)std::llround(d);
                return true;
            }

            bool CompressImpl(const CommonLib::IGeoShape& shape, std::vector<TByte>& out)
            {
                CommonLib::eShapeType type = shape.Type();
                if(!IsSupported(type))
                    return false;
                CommonLib::eShapeType genType = CommonLib::IGeoShape::GeneralType(type);

                uint32_t nPoints = shape.GetPointCnt();
                const CommonLib::GisXYPoint* pPoints = nPoints > 0 ? shape.GetPoints() : nullptr;
                if(nPoints > 0 && !pPoints)
                    throw CommonLib::CExcBase("the shape has {0} points, but no coordinates", nPoints);

                m_vecX.resize(nPoints);
                m_vecY.resize(nPoints);
                for(uint32_t i = 0; i < nPoints; ++i)
                {
                    if(!Quantize(pPoints[i].x, m_vecX[i]) || !Quantize(pPoints[i].y, m_vecY[i]))
                        return false;
                }

                m_vecBlob.clear();
                m_vecBlob.push_back(ShapeCompressedFlag);
                int16_t nType = (int16_t)type;
                m_vecBlob.push_back((TByte)(nType & 0xFF));
                m_vecBlob.push_back((TByte)((nType >> 8) & 0xFF));
                size_t nOptionsPos = m_vecBlob.size();
                m_vecBlob.push_back(0);   // options
                m_vecBlob.push_back((TByte)(int8_t)m_params.nScaleExponent);

                if(genType == CommonLib::shape_type_general_point)
                {
                    if(nPoints != 1)
                        throw CommonLib::CExcBase("a point shape has {0} points", nPoints);
                    WriteVarint(m_vecBlob, ZigZag(m_vecX[0]));
                    WriteVarint(m_vecBlob, ZigZag(m_vecY[0]));
                    m_vecBlob[nOptionsPos] = (TByte)(ShapeFormatVersion << ShapeOptionVersionShift);
                    out.swap(m_vecBlob);
                    return true;
                }

                // parts
                m_vecParts.clear();
                uint32_t nParts = 0;
                if(genType != CommonLib::shape_type_general_multipoint)
                {
                    nParts = shape.GetPartCount();
                    uint64_t nSum = 0;
                    for(uint32_t i = 0; i < nParts; ++i)
                    {
                        uint32_t nPart = shape.GetPart(i);
                        m_vecParts.push_back(nPart);
                        nSum += nPart;
                    }
                    if(nParts > 0 && nSum != nPoints)
                        throw CommonLib::CExcBase("the parts have {0} points, the shape has {1}", nSum, nPoints);
                }

                // rings: the closing point is the first one again
                bool bClosed = genType == CommonLib::shape_type_general_polygon && nParts > 0;
                for(uint32_t i = 0, nStart = 0; i < nParts && bClosed; ++i)
                {
                    uint32_t nPart = m_vecParts[i];
                    uint32_t nLast = nStart + nPart - 1;
                    if(nPart < 4 || m_vecX[nStart] != m_vecX[nLast] || m_vecY[nStart] != m_vecY[nLast])
                        bClosed = false;
                    nStart += nPart;
                }

                int64_t xMin = 0, yMin = 0, xMax = 0, yMax = 0;
                for(uint32_t i = 0; i < nPoints; ++i)
                {
                    if(i == 0 || m_vecX[i] < xMin) xMin = m_vecX[i];
                    if(i == 0 || m_vecX[i] > xMax) xMax = m_vecX[i];
                    if(i == 0 || m_vecY[i] < yMin) yMin = m_vecY[i];
                    if(i == 0 || m_vecY[i] > yMax) yMax = m_vecY[i];
                }

                WriteVarint(m_vecBlob, nPoints);
                if(genType != CommonLib::shape_type_general_multipoint)
                {
                    WriteVarint(m_vecBlob, nParts);
                    for(uint32_t i = 0; i + 1 < nParts; ++i)
                        WriteVarint(m_vecBlob, m_vecParts[i]);
                }
                WriteVarint(m_vecBlob, ZigZag(xMin));
                WriteVarint(m_vecBlob, ZigZag(yMin));
                WriteVarint(m_vecBlob, (uint64_t)(xMax - xMin));
                WriteVarint(m_vecBlob, (uint64_t)(yMax - yMin));

                // deltas from the previous point
                m_vecValues.clear();
                m_vecValues.reserve((size_t)nPoints * 2);
                int64_t xPrev = xMin, yPrev = yMin;
                uint32_t nPart = 0, nPartLeft = nParts > 0 ? m_vecParts[0] : nPoints;
                for(uint32_t i = 0; i < nPoints; ++i)
                {
                    while(nPartLeft == 0 && nPart + 1 < nParts)
                        nPartLeft = m_vecParts[++nPart];

                    bool bSkip = bClosed && nPartLeft == 1;   // the closing point of the ring
                    --nPartLeft;
                    if(bSkip)
                        continue;

                    m_vecValues.push_back(ZigZag(m_vecX[i] - xPrev));
                    m_vecValues.push_back(ZigZag(m_vecY[i] - yPrev));
                    xPrev = m_vecX[i];
                    yPrev = m_vecY[i];
                }

                eValueCoding coding = CValueEncoder::ChooseCoding(m_vecValues.data(), m_vecValues.size());
                CValueEncoder::Encode(coding, m_vecValues.data(), m_vecValues.size(), m_vecBlob);

                m_vecBlob[nOptionsPos] = (TByte)((ShapeFormatVersion << ShapeOptionVersionShift) | (coding & ShapeOptionCodingMask) |
                                                 (bClosed ? ShapeOptionClosedRings : 0));
                out.swap(m_vecBlob);
                return true;
            }

        private:
            SShapeCompressParams m_params;
            double m_dFactor;
            bool   m_bMultiply;
            std::vector<int64_t>  m_vecX;
            std::vector<int64_t>  m_vecY;
            std::vector<uint32_t> m_vecParts;
            std::vector<uint64_t> m_vecValues;
            std::vector<TByte>    m_vecBlob;
        };
    }
}
