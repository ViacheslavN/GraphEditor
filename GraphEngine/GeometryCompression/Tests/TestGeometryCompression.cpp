// Catch2 tests of GeometryCompression: the coders, the shape compressor, the sequential reader
// and the two modes of CGeoShape (raw / compressed).
#ifdef _WIN32
#define NOMINMAX
#endif
#include <catch2/catch_test_macros.hpp>
#include <catch2/catch_approx.hpp>

#include "../CompressionUtils.h"
#include "../RangeCoder.h"
#include "../NumLenCoder.h"
#include "../ValueCoder.h"
#include "../ShapeCompressor.h"
#include "../CompressedShapeReader.h"
#include "../../CommonLib/SpatialData/GeoShape.h"
#include "../../CommonLib/stream/MemoryStream.h"
#include <catch2/matchers/catch_matchers_string.hpp>

#include <cmath>
#include <random>
#include <vector>

using namespace GraphEngine::GeometryCompression;
using CommonLib::GisXYPoint;

namespace
{
    // a shape from parts of points
    std::shared_ptr<CommonLib::CGeoShape> MakeShape(CommonLib::eShapeType type, const std::vector<std::vector<GisXYPoint> >& parts)
    {
        uint32_t nPoints = 0;
        for(const auto& part : parts)
            nPoints += (uint32_t)part.size();
        std::shared_ptr<CommonLib::CGeoShape> ptrShape = std::make_shared<CommonLib::CGeoShape>();
        ptrShape->Create(type, nPoints, (uint32_t)parts.size());
        uint32_t* pParts = ptrShape->GetParts();
        GisXYPoint* pPoints = ptrShape->GetPoints();
        uint32_t nPos = 0;
        for(size_t i = 0; i < parts.size(); ++i)
        {
            if(pParts)
                pParts[i] = nPos;
            for(const GisXYPoint& pt : parts[i])
                pPoints[nPos++] = pt;
        }
        ptrShape->CalcBB();
        return ptrShape;
    }

    std::vector<GisXYPoint> Ring(double x, double y, double size, int nVertices, bool bClose = true)
    {
        std::vector<GisXYPoint> ring;
        for(int i = 0; i < nVertices; ++i)
        {
            double a = 2. * 3.14159265358979 * i / nVertices;
            ring.push_back({x + size * std::cos(a), y + size * std::sin(a)});
        }
        if(bClose)
            ring.push_back(ring.front());
        return ring;
    }

    double Round(double v, int k)
    {
        double f = Pow10(k);
        return (double)std::llround(v * f) / f;
    }

    // the decoded shape is the source rounded to 10^-k
    void RequireSame(const CommonLib::IGeoShape& source, const CommonLib::IGeoShape& decoded, int k)
    {
        REQUIRE(decoded.Type() == source.Type());
        REQUIRE(decoded.GetPointCnt() == source.GetPointCnt());
        uint32_t nParts = source.GetPartCount();
        REQUIRE(decoded.GetPartCount() == (nParts > 0 ? nParts : decoded.GetPartCount()));
        for(uint32_t i = 0; i < nParts; ++i)
            REQUIRE(decoded.GetPart(i) == source.GetPart(i));
        const GisXYPoint* pSrc = source.GetPoints();
        const GisXYPoint* pDst = decoded.GetPoints();
        for(uint32_t i = 0; i < source.GetPointCnt(); ++i)
        {
            REQUIRE(pDst[i].x == Catch::Approx(Round(pSrc[i].x, k)).margin(1e-9));
            REQUIRE(pDst[i].y == Catch::Approx(Round(pSrc[i].y, k)).margin(1e-9));
        }
    }
}

TEST_CASE("Geometry compression: varints, zigzag, bits", "[geomcompress]")
{
    std::mt19937_64 rnd(1);
    std::vector<TByte> buf;
    std::vector<int64_t> values = {0, 1, -1, 63, -64, 64, 127, 128, INT64_MAX, INT64_MIN, 1234567890123ll};
    for(int i = 0; i < 1000; ++i)
        values.push_back((int64_t)rnd() >> (rnd() % 64));
    for(int64_t v : values)
        WriteVarint(buf, ZigZag(v));
    CByteReader reader(buf.data(), buf.size());
    for(int64_t v : values)
        REQUIRE(reader.ReadZigZag() == v);
    REQUIRE_FALSE(reader.IsError());
    reader.ReadVarint();
    REQUIRE(reader.IsError());

    REQUIRE(BitLength(0) == 0);
    REQUIRE(BitLength(1) == 1);
    REQUIRE(BitLength(5) == 3);
    REQUIRE(BitLength(UINT64_MAX) == 64);

    std::vector<TByte> bits;
    std::vector<std::pair<uint64_t, uint32_t> > written;
    {
        CBitWriter writer(bits);
        for(int i = 0; i < 2000; ++i)
        {
            uint32_t nBits = (uint32_t)(rnd() % 65);
            uint64_t v = nBits == 64 ? rnd() : (rnd() & ((1ull << nBits) - 1));
            writer.Write(v, nBits);
            written.push_back(std::make_pair(v, nBits));
        }
        writer.Flush();
    }
    CBitReader bitReader;
    bitReader.Attach(bits.data(), bits.size());
    for(const auto& w : written)
        REQUIRE(bitReader.Read(w.second) == w.first);
}

TEST_CASE("Geometry compression: range coder", "[geomcompress]")
{
    std::mt19937_64 rnd(3);
    for(int nCase = 0; nCase < 3; ++nCase)
    {
        // a skewed static model: symbol i has the frequency freq[i]
        std::vector<uint64_t> freq = nCase == 0 ? std::vector<uint64_t>{1} :
                                     (nCase == 1 ? std::vector<uint64_t>{1000, 1, 3, 500, 2} : std::vector<uint64_t>{1, 1, 1, 1, 1, 1, 1, 1});
        std::vector<uint64_t> cum(freq.size() + 1, 0);
        for(size_t i = 0; i < freq.size(); ++i)
            cum[i + 1] = cum[i] + freq[i];
        uint64_t nTotal = cum.back();

        std::vector<size_t> symbols;
        for(int i = 0; i < 20000; ++i)
        {
            uint64_t f = rnd() % nTotal;
            size_t s = 0;
            while(cum[s + 1] <= f)
                ++s;
            symbols.push_back(s);
        }
        for(size_t nCount : {(size_t)0, (size_t)1, (size_t)7, symbols.size()})
        {
            std::vector<TByte> out;
            CRangeEncoder encoder(out);
            for(size_t i = 0; i < nCount; ++i)
                encoder.Encode(cum[symbols[i]], cum[symbols[i] + 1], nTotal);
            encoder.Finish();

            CRangeDecoder decoder;
            decoder.Start(out.data(), out.data() + out.size());
            for(size_t i = 0; i < nCount; ++i)
            {
                uint64_t f = decoder.GetFreq(nTotal);
                size_t s = 0;
                while(cum[s + 1] <= f)
                    ++s;
                REQUIRE(s == symbols[i]);
                decoder.Decode(cum[s], cum[s + 1]);
            }
        }
    }
}

TEST_CASE("Geometry compression: value codings round trip", "[geomcompress]")
{
    std::mt19937_64 rnd(7);
    for(int nCase = 0; nCase < 4; ++nCase)
    {
        std::vector<uint64_t> values;
        size_t nCount = nCase == 0 ? 1 : (nCase == 1 ? 17 : (nCase == 2 ? 5000 : 300));
        for(size_t i = 0; i < nCount; ++i)
        {
            // mostly small deltas, a few big jumps (and the full range in the last case)
            uint64_t v = nCase == 3 ? rnd() : (rnd() % 10 == 0 ? rnd() % 1000000 : rnd() % 300);
            values.push_back(v);
        }
        for(eValueCoding coding : {ValueCodingVarint, ValueCodingFixedBits, ValueCodingNumLen})
        {
            std::vector<TByte> out;
            CValueEncoder::Encode(coding, values.data(), values.size(), out);
            CByteReader reader(out.data(), out.size());
            CValueDecoder decoder;
            decoder.Open(coding, reader, values.size());
            for(int nPass = 0; nPass < 2; ++nPass)
            {
                for(uint64_t v : values)
                    REQUIRE(decoder.Next() == v);
                decoder.Rewind();
            }
        }
    }

    // NumLen wins for long sequences of similar values, varint for a few values
    std::vector<uint64_t> longValues;
    for(int i = 0; i < 3000; ++i)
        longValues.push_back(200 + rnd() % 3000);
    REQUIRE(CValueEncoder::ChooseCoding(longValues.data(), longValues.size()) == ValueCodingNumLen);
    std::vector<uint64_t> fewValues = {3, 250, 7, 100};
    REQUIRE(CValueEncoder::ChooseCoding(fewValues.data(), fewValues.size()) == ValueCodingVarint);
}

TEST_CASE("Geometry compression: shapes round trip", "[geomcompress]")
{
    std::vector<std::shared_ptr<CommonLib::CGeoShape> > shapes;
    // a point, a multipoint, a line, a multi line, a polygon with a hole, a big ring, an open "polygon"
    shapes.push_back(MakeShape(CommonLib::shape_type_point, {{{9240410.123, 7404603.456}}}));
    shapes.push_back(MakeShape(CommonLib::shape_type_multipoint, {{{1., 2.}, {-3.5, 4.25}, {1e6, -1e6}}}));
    shapes.push_back(MakeShape(CommonLib::shape_type_polyline, {{{9240410.12, 7404603.45}, {9240420.5, 7404610.0}, {9240440.0, 7404590.75}}}));
    shapes.push_back(MakeShape(CommonLib::shape_type_polyline, {{{0, 0}, {10, 10}}, {{20, 20}, {30, 25}, {40, 20}}, {{-5, -5}, {-6, -7}}}));
    shapes.push_back(MakeShape(CommonLib::shape_type_polygon, {Ring(9240000, 7400000, 50, 8), Ring(9240000, 7400000, 10, 5)}));
    shapes.push_back(MakeShape(CommonLib::shape_type_polygon, {Ring(-8000000, 1500000, 25000, 2000)}));
    shapes.push_back(MakeShape(CommonLib::shape_type_polygon, {Ring(100, 100, 5, 6, false)}));

    for(int k : {2, 7, -1})
    {
        SShapeCompressParams params;
        params.nScaleExponent = k;
        CShapeCompressor compressor(params);
        for(size_t s = 0; s < shapes.size(); ++s)
        {
            INFO("shape " << s << " scale " << k);
            const CommonLib::CGeoShape& source = *shapes[s];
            std::vector<TByte> blob;
            REQUIRE(compressor.Compress(source, blob));
            REQUIRE(IsCompressedShape(blob.data(), blob.size()));
            REQUIRE(CommonLib::CGeoShape::Type(blob.data()) == source.Type());   // the type is where CGeoShape has it
            if(source.GetPointCnt() > 3 && k == 2)
                REQUIRE(blob.size() * 2 < source.Size());

            // the compressed mode of CGeoShape: the header gives the type, the counts and the box
            CommonLib::CGeoShape shape;
            shape.Import(blob.data(), (uint32_t)blob.size());
            REQUIRE(shape.IsCompressed());
            REQUIRE(shape.Type() == source.Type());
            CommonLib::bbox bbSrc = source.GetBB(), bb = shape.GetBB();
            REQUIRE(bb.xMin == Catch::Approx(Round(bbSrc.xMin, k)).margin(1e-9));
            REQUIRE(bb.yMax == Catch::Approx(Round(bbSrc.yMax, k)).margin(1e-9));
            REQUIRE(shape.GetPointCnt() == source.GetPointCnt());
            REQUIRE(shape.GetPartCount() == source.GetPartCount());
            for(uint32_t i = 0; i < source.GetPartCount(); ++i)
                REQUIRE(shape.GetPart(i) == source.GetPart(i));

            // sequential reading as the drawing does: the parts and their points, twice
            for(int nPass = 0; nPass < 2; ++nPass)
            {
                uint32_t nParts = source.GetPartCount() > 0 ? source.GetPartCount() : 1;
                for(uint32_t part = 0, offset = 0; part < nParts; ++part)
                {
                    uint32_t nPartPoints = source.GetPartCount() > 0 ? shape.NextPart(part) : source.GetPointCnt();
                    REQUIRE(nPartPoints == (source.GetPartCount() > 0 ? source.GetPart(part) : source.GetPointCnt()));
                    for(uint32_t i = 0; i < nPartPoints; ++i)
                    {
                        GisXYPoint pt;
                        REQUIRE(shape.NextPoint(offset + i, pt));
                        REQUIRE(pt.x == Catch::Approx(Round(source.GetPoints()[offset + i].x, k)).margin(1e-9));
                        REQUIRE(pt.y == Catch::Approx(Round(source.GetPoints()[offset + i].y, k)).margin(1e-9));
                    }
                    offset += nPartPoints;
                }
                GisXYPoint pt;
                REQUIRE_FALSE(shape.NextPoint(source.GetPointCnt(), pt));   // past the end
            }
            REQUIRE(shape.IsCompressed());   // no random access, no decompression

            // a jump back / forward
            if(source.GetPointCnt() > 2)
            {
                GisXYPoint pt;
                REQUIRE(shape.NextPoint(source.GetPointCnt() - 1, pt));
                REQUIRE(shape.NextPoint(1, pt));
                REQUIRE(pt.x == Catch::Approx(Round(source.GetPoints()[1].x, k)).margin(1e-9));
            }

            // copies keep the mode (the reader reads their own blob)
            REQUIRE(std::vector<TByte>(shape.Data(), shape.Data() + shape.Size()) == blob);
            CommonLib::CGeoShape copy(shape);
            CommonLib::CGeoShape assigned;
            assigned = shape;
            CommonLib::IGeoShapePtr ptrClone = shape.Clone();
            REQUIRE(copy.IsCompressed());
            REQUIRE(assigned.IsCompressed());
            REQUIRE(ptrClone->Size() == blob.size());
            RequireSame(source, *ptrClone, k);

            // GetPoints switches to the raw mode
            RequireSame(source, shape, k);
            REQUIRE_FALSE(shape.IsCompressed());
            REQUIRE(shape.Data()[0] == 0);
            REQUIRE(shape.Size() == source.Size());

            // and back
            REQUIRE(shape.Compress(k));
            REQUIRE(shape.IsCompressed());
            REQUIRE(shape.Size() == blob.size());
            shape.Decompress();
            REQUIRE_FALSE(shape.IsCompressed());
            RequireSame(source, shape, k);
        }
    }
}

TEST_CASE("Geometry compression: closed rings, plain blobs, damaged and unsupported shapes", "[geomcompress]")
{
    CShapeCompressor compressor;

    // the closing point of a ring isn't stored: a building of 5 points takes less than 4 x 2 deltas
    std::shared_ptr<CommonLib::CGeoShape> ptrBuilding = MakeShape(CommonLib::shape_type_polygon,
        {{{9240000.00, 7400000.00}, {9240000.00, 7400012.35}, {9240008.10, 7400012.35}, {9240008.10, 7400000.00}, {9240000.00, 7400000.00}}});
    std::vector<TByte> blob;
    REQUIRE(compressor.Compress(*ptrBuilding, blob));
    CCompressedShapeReader reader;
    REQUIRE_NOTHROW(reader.Open(blob.data(), blob.size()));
    REQUIRE(reader.IsClosedRings());
    REQUIRE(blob.size() < 40);       // the plain buffer has 127 bytes
    REQUIRE(ptrBuilding->Size() > 120);

    // a raw blob is imported in the raw mode
    CommonLib::CGeoShape shape;
    shape.Import(ptrBuilding->Data(), ptrBuilding->Size());
    REQUIRE_FALSE(shape.IsCompressed());
    REQUIRE(shape.Size() == ptrBuilding->Size());
    GisXYPoint pt;
    REQUIRE(shape.NextPoint(2, pt));
    REQUIRE(pt.x == 9240008.10);

    // CGeoShape::Compress
    CommonLib::CGeoShape building(*ptrBuilding);
    REQUIRE(building.Compress(2));
    REQUIRE(building.Size() == blob.size());
    REQUIRE(building.GetBB().xMax == Catch::Approx(9240008.10));

    // write / read through a stream keeps the mode
    {
        CommonLib::CWriteMemoryStream stream;
        building.Write(&stream);
        CommonLib::CReadMemoryStream readStream;
        readStream.AttachBuffer(stream.Buffer(), stream.Size());
        CommonLib::CGeoShape read;
        read.Read(&readStream);
        REQUIRE(read.IsCompressed());
        REQUIRE(read.GetPointCnt() == 5);
    }

    // damaged blobs throw, the shape is left empty
    std::vector<TByte> damaged(blob.begin(), blob.begin() + 8);
    REQUIRE_THROWS_WITH(shape.Import(damaged.data(), (uint32_t)damaged.size()), Catch::Matchers::ContainsSubstring("compressed shape"));
    REQUIRE_FALSE(shape.IsCompressed());
    REQUIRE(shape.GetPointCnt() == 0);
    REQUIRE(shape.Type() == CommonLib::shape_type_null);

    std::vector<TByte> version = blob;
    version[3] |= 0x30;   // unknown format version
    REQUIRE_THROWS_WITH(shape.Import(version.data(), (uint32_t)version.size()), Catch::Matchers::ContainsSubstring("version"));

    // the coordinates are cut: Import checks that the data can have all the values
    std::shared_ptr<CommonLib::CGeoShape> ptrLine = MakeShape(CommonLib::shape_type_polyline, {{{0, 0}, {100, 100}, {250, 50}, {400, 300}}});
    std::vector<TByte> lineBlob;
    REQUIRE(compressor.Compress(*ptrLine, lineBlob));
    lineBlob.resize(lineBlob.size() - 3);
    REQUIRE_THROWS_WITH(shape.Import(lineBlob.data(), (uint32_t)lineBlob.size()), Catch::Matchers::ContainsSubstring("damaged"));
    REQUIRE(shape.GetPointCnt() == 0);

    // Z isn't compressed, the shape stays raw
    CommonLib::CGeoShape shapeZ;
    shapeZ.Create(CommonLib::shape_type_polyline_z, 2, 1);
    REQUIRE_FALSE(CShapeCompressor::IsSupported(CommonLib::shape_type_polyline_z));
    REQUIRE_FALSE(compressor.Compress(shapeZ, blob));
    REQUIRE_FALSE(shapeZ.Compress());
    REQUIRE_FALSE(shapeZ.IsCompressed());

    // a coordinate out of the integer range
    std::shared_ptr<CommonLib::CGeoShape> ptrHuge = MakeShape(CommonLib::shape_type_polyline, {{{1e300, 0.}, {0., 0.}}});
    REQUIRE_FALSE(compressor.Compress(*ptrHuge, blob));

    // a wrong scale exponent
    SShapeCompressParams wrong;
    wrong.nScaleExponent = 40;
    REQUIRE_THROWS(CShapeCompressor(wrong));
}

TEST_CASE("Geometry compression: precision by the units and the extent", "[geomcompress]")
{
    REQUIRE(CompressParamsForUnits(CommonLib::UnitsDecimalDegrees).nScaleExponent == 7);
    REQUIRE(CompressParamsForUnits(CommonLib::UnitsMeters).nScaleExponent == 2);
    REQUIRE(CompressParamsForUnits(CommonLib::UnitsKilometers).nScaleExponent == 3);
    REQUIRE(CompressParamsForUnits(CommonLib::UnitsUnknown).nScaleExponent == 4);

    CommonLib::bbox mercator;
    mercator.type = CommonLib::bbox_type_normal;
    mercator.xMin = -20037508.34; mercator.xMax = 20037508.34;
    mercator.yMin = -20037508.34; mercator.yMax = 20037508.34;
    REQUIRE(CompressParamsForExtent(mercator, CommonLib::UnitsMeters).nScaleExponent == 2);

    // huge coordinates: centimeters wouldn't be exact doubles
    CommonLib::bbox huge = mercator;
    huge.xMax = 1e15;
    int k = CompressParamsForExtent(huge, CommonLib::UnitsMeters).nScaleExponent;
    REQUIRE(k < 2);
    REQUIRE(1e15 * Pow10(k) <= MaxExactInteger);

    // the maximum precision: Web Mercator keeps 8 decimals (2e7 * 1e8 < 2^53), degrees 13
    REQUIRE(MaxCompressParamsForExtent(mercator).nScaleExponent == 8);
    CommonLib::bbox degrees = mercator;
    degrees.xMin = -180; degrees.xMax = 180; degrees.yMin = -90; degrees.yMax = 90;
    REQUIRE(MaxCompressParamsForExtent(degrees).nScaleExponent == 13);
}
