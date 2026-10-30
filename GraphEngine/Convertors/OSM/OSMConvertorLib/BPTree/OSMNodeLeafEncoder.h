#pragma once

// CNodeLeafEncoder: the leaves of the node tree encoded where the page is full (TNodeLeafCompressor, CParallelLeafBuilder).

#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../bptreedb/BPtreeLib/AllocsSet.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/CompressorParams.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/EntropyEncoder/SignedNumLenEncoder.h"
#include "../../../../../CommonLib/stream/stream.h"
#include "OSMPointE7.h"

#include <algorithm>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // Encoding of the leaves of the node tree where the page is full (TNodeLeafCompressor::Write, the threads of the
            // parallel build, COSMBPTreeConvertor):
            //   FindFit - the differences of the ids / latitudes / longitudes are added to the NumLen statistics element by
            //             element, the size is estimated every CutCheckStep elements; above the page the last elements are
            //             removed one by one until it fits (the estimate is an upper bound of the encoded size);
            //   Encode  - the elements FindFit found, in the format of TBaseNodeCompressor (count, key size, value size,
            //             TNodeIdEncoder keys, TPointCompressor values: TBaseNodeCompressor::Load decodes it).
            // TKeys / TPoints: operator[] (TDeVector, pointers). One object per thread.
            class CNodeLeafEncoder
            {
            public:
                static const uint32_t CutCheckStep = 32;   // elements between the size estimates

                CNodeLeafEncoder(bptreedb::TAllocsSetPtr pAllocsSet, bptreedb::TCompressorParamsBasePtr pParams) :
                        m_keyEncoder(pAllocsSet, pParams, bptreedb::eLeafKey),
                        m_latEncoder(pAllocsSet, pParams, bptreedb::eLeafValue),
                        m_lonEncoder(pAllocsSet, pParams, bptreedb::eLeafValue)
                {}

                // the number of the first of n elements that fit in nPageSize bytes (at least 1, up to nMaxCount); the statistics
                // of the encoders are of them (for Encode), FitSize - their estimated size
                template<class TKeys, class TPoints>
                uint32_t FindFit(const TKeys& keys, const TPoints& points, uint32_t n, uint32_t nPageSize, uint32_t nMaxCount)
                {
                    n = (std::min)(n, (std::max)(1u, nMaxCount));
                    if (m_keyDiffs.size() < n)
                    {
                        m_keyDiffs.resize(n);
                        m_latDiffs.resize(n);
                        m_lonDiffs.resize(n);
                    }
                    m_keyEncoder.Clear();
                    m_latEncoder.Clear();
                    m_lonEncoder.Clear();

                    uint32_t nLast = 0;     // the elements [0, nLast] are in the statistics
                    for (uint32_t i = 1; i < n; ++i)
                    {
                        m_keyDiffs[i] = (uint64_t)(keys[i] - keys[i - 1]);
                        m_latDiffs[i] = (int64_t)points[i].lat - points[i - 1].lat;
                        m_lonDiffs[i] = (int64_t)points[i].lon - points[i - 1].lon;
                        AddElement(i);
                        nLast = i;
                        if ((i % CutCheckStep) != 0 && i + 1 != n)
                            continue;
                        if (EstimateSize() <= nPageSize)
                            continue;

                        // over the page: back to the last element that fits (it is after the previous check)
                        while (nLast > 0 && EstimateSize() > nPageSize)
                        {
                            RemoveElement(nLast);
                            --nLast;
                        }
                        break;
                    }
                    m_nFit = nLast + 1;
                    m_nFitSize = EstimateSize();
                    return m_nFit;
                }

                uint32_t FitSize() const { return m_nFitSize; }

                // the elements of the last FindFit
                template<class TKeys, class TPoints>
                void Encode(const TKeys& keys, const TPoints& points, CommonLib::IMemoryWriteStream* pStream)
                {
                    uint32_t nFit = m_nFit;
                    pStream->Write(nFit);
                    size_t nSizePos = pStream->Pos();
                    pStream->Write((uint32_t)0);
                    pStream->Write((uint32_t)0);

                    // keys: TBaseValueDiffEncoder (TNodeIdEncoder::Decode)
                    size_t nKeyStart = pStream->Pos();
                    pStream->Write((int64_t)keys[0]);
                    if (!m_keyEncoder.BeginEncoding(pStream))
                        throw CommonLib::CExcBase("no space for the keys");
                    for (uint32_t i = 1; i < nFit; ++i)
                        if (!m_keyEncoder.EncodeSymbol(m_keyDiffs[i]))
                            throw CommonLib::CExcBase("no space for the key {0}", i);
                    if (!m_keyEncoder.FinishEncoding(pStream))
                        throw CommonLib::CExcBase("no space for the keys");

                    // values: TPointCompressor
                    size_t nValueStart = pStream->Pos();
                    pStream->Write(points[0].lat);
                    pStream->Write(points[0].lon);
                    WriteStream(m_latEncoder, m_latDiffs, nFit, pStream);
                    WriteStream(m_lonEncoder, m_lonDiffs, nFit, pStream);

                    size_t nEnd = pStream->Pos();
                    pStream->Seek(nSizePos, CommonLib::soFromBegin);
                    pStream->Write((uint32_t)(nValueStart - nKeyStart));
                    pStream->Write((uint32_t)(nEnd - nValueStart));
                    pStream->Seek(nEnd, CommonLib::soFromBegin);
                }

            private:
                uint32_t EstimateSize() const
                {
                    // count, key size, value size; the first key; the first point; the sizes of the two coordinate streams
                    return (uint32_t)(3 * sizeof(uint32_t) + sizeof(int64_t) + 2 * sizeof(int32_t) + 2 * sizeof(uint32_t)) +
                           m_keyEncoder.GetCompressSize() + m_latEncoder.GetCompressSize() + m_lonEncoder.GetCompressSize();
                }

                void AddElement(uint32_t i)
                {
                    m_keyEncoder.AddSymbol(m_keyDiffs[i]);
                    m_latEncoder.AddSymbol(m_latDiffs[i]);
                    m_lonEncoder.AddSymbol(m_lonDiffs[i]);
                }

                void RemoveElement(uint32_t i)
                {
                    m_keyEncoder.RemoveSymbol(m_keyDiffs[i]);
                    m_latEncoder.RemoveSymbol(m_latDiffs[i]);
                    m_lonEncoder.RemoveSymbol(m_lonDiffs[i]);
                }

                // uint32 size, the NumLen stream of the differences [1, nCount)
                static void WriteStream(bptreedb::CSignedNumLenEncoder& encoder, const std::vector<int64_t>& vecDiffs, uint32_t nCount,
                                        CommonLib::IMemoryWriteStream* pStream)
                {
                    size_t nSizePos = pStream->Pos();
                    pStream->Write((uint32_t)0);
                    size_t nStart = pStream->Pos();
                    if (nCount > 1)
                    {
                        if (!encoder.BeginEncoding(pStream))
                            throw CommonLib::CExcBase("no space for the coordinates");
                        for (uint32_t i = 1; i < nCount; ++i)
                            if (!encoder.EncodeSymbol(vecDiffs[i]))
                                throw CommonLib::CExcBase("no space for the coordinate {0}", i);
                        if (!encoder.FinishEncoding(pStream))
                            throw CommonLib::CExcBase("no space for the coordinates");
                    }
                    size_t nEnd = pStream->Pos();
                    pStream->Seek(nSizePos, CommonLib::soFromBegin);
                    pStream->Write((uint32_t)(nEnd - nStart));
                    pStream->Seek(nEnd, CommonLib::soFromBegin);
                }

            private:
                bptreedb::CUnsignedNumLenEncoder m_keyEncoder;
                bptreedb::CSignedNumLenEncoder   m_latEncoder;
                bptreedb::CSignedNumLenEncoder   m_lonEncoder;
                std::vector<uint64_t> m_keyDiffs;
                std::vector<int64_t>  m_latDiffs;
                std::vector<int64_t>  m_lonDiffs;
                uint32_t m_nFit = 0;
                uint32_t m_nFitSize = 0;
            };
        }
    }
}
