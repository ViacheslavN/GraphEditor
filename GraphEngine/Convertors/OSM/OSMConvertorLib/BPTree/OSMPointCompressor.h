#pragma once

// TPointCompressor: the value encoder of the node tree leaves (the points of the node ids in the key order).

#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../bptreedb/BPtreeLib/DeVector.h"
#include "../../../../../bptreedb/BPtreeLib/BPTreeContext.h"
#include "../../../../../bptreedb/BPtreeLib/AllocsSet.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/CompressorParams.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/EntropyEncoder/SignedNumLenEncoder.h"
#include "../../../../../CommonLib/stream/MemoryStream.h"
#include "OSMPointE7.h"

#include <algorithm>
#include <vector>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // Values of the leaves: the points of the node ids in the key order. The interface of the value encoders of
            // TBaseNodeCompressor, the statistics follow TBaseValueDiffEncoder (AddSymbol / RemoveSymbol with the neighbours).
            //
            //   first point (lat, lon: int32, int32)
            //   uint32 size, latitude differences  (CSignedNumLenEncoder)
            //   uint32 size, longitude differences (CSignedNumLenEncoder)
            //
            // The differences are int64: two longitudes differ by up to 3.6e9 (more than int32).
            class TPointCompressor
            {
            public:
                typedef SOSMPointE7 TValue;
                typedef CommonLib::STLAllocator<TValue> TAlloc;
                typedef bptreedb::TDeVector<TValue, TAlloc> TValueMemSet;

                TPointCompressor(bptreedb::TAllocsSetPtr pAllocsSet, bptreedb::TCompressorParamsBasePtr pParams, bptreedb::ECompressParams type) :
                        m_latEncoder(pAllocsSet, pParams, type),
                        m_lonEncoder(pAllocsSet, pParams, type)
                {}

                void AddSymbol(uint32_t nSize, uint32_t nIndex, const TValue& value, const TValueMemSet& vecValues)
                {
                    if (nSize > 1)
                    {
                        if (nIndex == 0)
                            AddDiff(vecValues[nIndex + 1], value);
                        else
                        {
                            const TValue& prev = vecValues[nIndex - 1];
                            if (nIndex == nSize - 1)
                                AddDiff(value, prev);
                            else
                            {
                                const TValue& next = vecValues[nIndex + 1];
                                RemoveDiff(next, prev);
                                AddDiff(value, prev);
                                AddDiff(next, value);
                            }
                        }
                    }
                }

                void RemoveSymbol(uint32_t nSize, uint32_t nIndex, const TValue& value, const TValueMemSet& vecValues)
                {
                    if (vecValues.size() > 1)
                    {
                        if (nIndex == 0)
                            RemoveDiff(vecValues[nIndex + 1], value);
                        else if (nIndex == vecValues.size() - 1)
                            RemoveDiff(value, vecValues[nIndex - 1]);
                        else
                        {
                            const TValue& prev = vecValues[nIndex - 1];
                            const TValue& next = vecValues[nIndex + 1];
                            AddDiff(next, prev);
                            RemoveDiff(value, prev);
                            RemoveDiff(next, value);
                        }
                    }
                }

                void UpdateSymbol(uint32_t nIndex, TValue& newValue, const TValue& oldValue, const TValueMemSet& vecValues)
                {
                    RemoveSymbol((uint32_t)vecValues.size(), nIndex, oldValue, vecValues);
                    AddSymbol((uint32_t)vecValues.size(), nIndex, newValue, vecValues);
                }

                uint32_t GetCompressSize() const
                {
                    return (uint32_t)(sizeof(int32_t) * 2 + sizeof(uint32_t) * 2) + m_latEncoder.GetCompressSize() + m_lonEncoder.GetCompressSize();
                }

                void BeginEncoding(const TValueMemSet&) {}

                // 0 - written, otherwise the number of the values to split off (the node doesn't fit)
                uint32_t Encode(const TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream, uint32_t maxCompSize, bptreedb::CBPTreeContext*)
                {
                    uint32_t n = (uint32_t)vecValues.size();
                    uint32_t nSplit = (std::max)(1u, n / 2);
                    try
                    {
                        if (n == 0)
                            return 0;

                        size_t nStart = pStream->Pos();
                        if (!pStream->WriteSafe(vecValues[0].lat) || !pStream->WriteSafe(vecValues[0].lon))
                            return nSplit;

                        // the statistics from scratch: the streams must have exactly the symbols that are encoded
                        m_latDiffs.resize(n);
                        m_lonDiffs.resize(n);
                        Clear();
                        for (uint32_t i = 1; i < n; ++i)
                        {
                            m_latDiffs[i] = (int64_t)vecValues[i].lat - vecValues[i - 1].lat;
                            m_lonDiffs[i] = (int64_t)vecValues[i].lon - vecValues[i - 1].lon;
                            m_latEncoder.AddSymbol(m_latDiffs[i]);
                            m_lonEncoder.AddSymbol(m_lonDiffs[i]);
                        }

                        // the estimate is an upper bound of the encoded size: the encoders throw at the end of the page
                        size_t nNeed = 2 * sizeof(uint32_t) + m_latEncoder.GetCompressSize() + m_lonEncoder.GetCompressSize();
                        if (pStream->Pos() + nNeed > pStream->Size() || pStream->Pos() + nNeed - nStart > maxCompSize)
                            return nSplit;

                        if (!EncodeStream(m_latEncoder, m_latDiffs, pStream) || !EncodeStream(m_lonEncoder, m_lonDiffs, pStream))
                            return nSplit;

                        return pStream->Pos() - nStart > maxCompSize ? nSplit : 0;
                    }
                    catch (std::exception& exc)
                    {
                        CommonLib::CExcBase::RegenExcT("[TPointCompressor] failed to encode, count: {0}", n, exc);
                        throw;
                    }
                }

                void Decode(uint32_t nCount, TValueMemSet& vecValues, CommonLib::IMemoryReadStream* pStream, uint32_t nCompSize, bptreedb::CBPTreeContext*)
                {
                    try
                    {
                        if (nCount == 0)
                            return;

                        TValue first;
                        first.lat = pStream->ReadInt32();
                        first.lon = pStream->ReadInt32();

                        m_latDiffs.assign(nCount, 0);
                        m_lonDiffs.assign(nCount, 0);
                        DecodeStream(m_latEncoder, m_latDiffs, nCount, pStream);
                        DecodeStream(m_lonEncoder, m_lonDiffs, nCount, pStream);

                        vecValues.push_back(first);
                        for (uint32_t i = 1; i < nCount; ++i)
                        {
                            const TValue& prev = vecValues[i - 1];
                            vecValues.push_back(TValue((int32_t)(prev.lat + m_latDiffs[i]), (int32_t)(prev.lon + m_lonDiffs[i])));
                        }

                        // the statistics of the decoded values (as TDiffStreamsEncoder), not needed by a compressor which
                        // computes them when it writes (TNodeLeafCompressor: SetKeepStatistics(false))
                        Clear();
                        if (m_bKeepStatistics)
                        {
                            for (uint32_t i = 1; i < nCount; ++i)
                            {
                                m_latEncoder.AddSymbol(m_latDiffs[i]);
                                m_lonEncoder.AddSymbol(m_lonDiffs[i]);
                            }
                        }
                    }
                    catch (std::exception& exc)
                    {
                        CommonLib::CExcBase::RegenExcT("[TPointCompressor] failed to decode, count: {0}", nCount, exc);
                        throw;
                    }
                }

                void Clear()
                {
                    m_latEncoder.Clear();
                    m_lonEncoder.Clear();
                }

                // false: Decode doesn't rebuild the statistics (the leaves are only read or written by a cut compressor)
                void SetKeepStatistics(bool bKeep) { m_bKeepStatistics = bKeep; }

            private:
                void AddDiff(const TValue& cur, const TValue& prev)
                {
                    m_latEncoder.AddSymbol((int64_t)cur.lat - prev.lat);
                    m_lonEncoder.AddSymbol((int64_t)cur.lon - prev.lon);
                }

                void RemoveDiff(const TValue& cur, const TValue& prev)
                {
                    m_latEncoder.RemoveSymbol((int64_t)cur.lat - prev.lat);
                    m_lonEncoder.RemoveSymbol((int64_t)cur.lon - prev.lon);
                }

                // uint32 size, the stream of the symbols [1, n)
                static bool EncodeStream(bptreedb::CSignedNumLenEncoder& encoder, const std::vector<int64_t>& vecDiffs, CommonLib::IMemoryWriteStream* pStream)
                {
                    size_t nSizePos = pStream->Pos();
                    if (!pStream->WriteSafe((uint32_t)0))
                        return false;
                    size_t nStreamStart = pStream->Pos();
                    if (vecDiffs.size() > 1)
                    {
                        if (!encoder.BeginEncoding(pStream))
                            return false;
                        for (size_t i = 1; i < vecDiffs.size(); ++i)
                            if (!encoder.EncodeSymbol(vecDiffs[i]))
                                return false;
                        if (!encoder.FinishEncoding(pStream))
                            return false;
                    }
                    size_t nEnd = pStream->Pos();
                    pStream->Seek(nSizePos, CommonLib::soFromBegin);
                    pStream->Write((uint32_t)(nEnd - nStreamStart));
                    pStream->Seek(nEnd, CommonLib::soFromBegin);
                    return true;
                }

                static void DecodeStream(bptreedb::CSignedNumLenEncoder& decoder, std::vector<int64_t>& vecDiffs, uint32_t nCount, CommonLib::IMemoryReadStream* pStream)
                {
                    uint32_t nSize = pStream->ReadIntu32();
                    if (nCount > 1)
                    {
                        CommonLib::CReadMemoryStream stream;
                        stream.AttachBuffer((byte_t*)pStream->Buffer() + pStream->Pos(), nSize);
                        decoder.BeginDecoding(&stream);
                        for (uint32_t i = 1; i < nCount; ++i)
                        {
                            int64_t v = 0;
                            decoder.DecodeSymbol(v);
                            vecDiffs[i] = v;
                        }
                        decoder.FinishDecoding(&stream);
                    }
                    pStream->Seek(nSize, CommonLib::soFromCurrent);
                }

            private:
                bptreedb::CSignedNumLenEncoder m_latEncoder;
                bptreedb::CSignedNumLenEncoder m_lonEncoder;
                std::vector<int64_t> m_latDiffs;
                std::vector<int64_t> m_lonDiffs;
                bool m_bKeepStatistics = true;
            };
        }
    }
}
