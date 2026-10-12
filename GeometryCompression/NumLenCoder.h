#pragma once
// NumLen coding of unsigned integers (the idea of TNumLemCompressor from GIS-master CommonLibrary):
// a value is split into its bit length (0..64) and the bits below the highest one.
// The bit lengths are coded by the range coder with a static model (the frequencies are in the header),
// the remaining bits are written as they are into a separate bit area.
//
//   header:   [number of used lengths] { [length] [frequency varint] } ...
//   data:     [low bits area, (MantissaBits() + 7) / 8 bytes] [range coded lengths up to the end]
//
// The decoder reads both areas in place and returns the values one by one (no buffer).

#include "RangeCoder.h"
#include <cmath>

namespace GraphEngine
{
    namespace GeometryCompression
    {
        class CNumLenModel
        {
        public:
            static const uint32_t SymbolCount = 65;   // bit lengths 0..64

            CNumLenModel() { Clear(); }

            void Clear()
            {
                for(uint32_t i = 0; i < SymbolCount; ++i)
                    m_freq[i] = 0;
                m_nTotal = 0;
                m_nMantissaBits = 0;
            }

            void Add(uint64_t v)
            {
                uint32_t nLen = BitLength(v);
                ++m_freq[nLen];
                ++m_nTotal;
                if(nLen > 1)
                    m_nMantissaBits += nLen - 1;
            }

            uint64_t Total() const { return m_nTotal; }
            uint64_t MantissaBits() const { return m_nMantissaBits; }
            bool     IsCodable() const { return m_nTotal > 0 && m_nTotal <= SRangeCoderConstants::MaxTotalCount; }

            uint32_t HeaderSize() const
            {
                uint32_t nSize = 1;
                for(uint32_t i = 0; i < SymbolCount; ++i)
                {
                    if(m_freq[i])
                        nSize += 1 + VarintSize(m_freq[i]);
                }
                return nSize;
            }

            // header + low bits + coded lengths (the coder adds a few bytes when it finishes)
            uint64_t EstimateSize() const
            {
                double dBits = 0.;
                for(uint32_t i = 0; i < SymbolCount; ++i)
                {
                    if(m_freq[i])
                        dBits += (double)m_freq[i] * std::log2((double)m_nTotal / (double)m_freq[i]);
                }
                return HeaderSize() + (m_nMantissaBits + 7) / 8 + (uint64_t)std::ceil(dBits / 8.) + 2;
            }

            void WriteHeader(std::vector<TByte>& out) const
            {
                uint32_t nUsed = 0;
                for(uint32_t i = 0; i < SymbolCount; ++i)
                    nUsed += m_freq[i] ? 1 : 0;
                out.push_back((TByte)nUsed);
                for(uint32_t i = 0; i < SymbolCount; ++i)
                {
                    if(!m_freq[i])
                        continue;
                    out.push_back((TByte)i);
                    WriteVarint(out, m_freq[i]);
                }
            }

            // cumulative counts for coding
            void BuildCumulative(uint64_t* pCum) const
            {
                uint64_t nSum = 0;
                for(uint32_t i = 0; i < SymbolCount; ++i)
                {
                    pCum[i] = nSum;
                    nSum += m_freq[i];
                }
                pCum[SymbolCount] = nSum;
            }

        private:
            uint64_t m_freq[SymbolCount];
            uint64_t m_nTotal;
            uint64_t m_nMantissaBits;
        };

        // encodes the values added to the model (in the same order) after the model header
        class CNumLenEncoder
        {
        public:
            explicit CNumLenEncoder(const CNumLenModel& model) : m_model(model)
            {
                model.BuildCumulative(m_cum);
            }

            void Encode(const uint64_t* pValues, size_t nCount, std::vector<TByte>& out) const
            {
                m_model.WriteHeader(out);

                {
                    CBitWriter bits(out);
                    for(size_t i = 0; i < nCount; ++i)
                    {
                        uint32_t nLen = BitLength(pValues[i]);
                        if(nLen > 1)
                            bits.Write(pValues[i], nLen - 1);   // the highest bit is implied by the length
                    }
                    bits.Flush();
                }

                CRangeEncoder coder(out);
                for(size_t i = 0; i < nCount; ++i)
                {
                    uint32_t nLen = BitLength(pValues[i]);
                    coder.Encode(m_cum[nLen], m_cum[nLen + 1], m_model.Total());
                }
                coder.Finish();
            }

        private:
            const CNumLenModel& m_model;
            uint64_t m_cum[CNumLenModel::SymbolCount + 1];
        };

        class CNumLenDecoder
        {
        public:
            CNumLenDecoder() : m_nUsed(0), m_nTotal(0), m_pData(nullptr), m_pEnd(nullptr), m_nMantissaBytes(0) {}

            // reads the header, the data goes up to the end of the reader; throws on a damaged header
            void Open(CByteReader& reader)
            {
                m_nUsed = reader.ReadByte();
                if(m_nUsed == 0 || m_nUsed > CNumLenModel::SymbolCount)
                    throw CommonLib::CExcBase("NumLen header is damaged: {0} bit lengths", m_nUsed);

                uint64_t nSum = 0, nMantissaBits = 0;
                for(uint32_t i = 0; i < m_nUsed; ++i)
                {
                    uint32_t nLen = reader.ReadByte();
                    uint64_t nFreq = reader.ReadVarint();
                    if(nLen >= CNumLenModel::SymbolCount || nFreq == 0 || (i > 0 && nLen <= m_len[i - 1]))
                        throw CommonLib::CExcBase("NumLen header is damaged: bit length {0}, frequency {1}", nLen, nFreq);
                    m_len[i] = (TByte)nLen;
                    m_cum[i] = nSum;
                    nSum += nFreq;
                    if(nLen > 1)
                        nMantissaBits += nFreq * (nLen - 1);
                }
                reader.CheckError("NumLen header");
                m_cum[m_nUsed] = nSum;
                m_nTotal = nSum;
                if(m_nTotal > SRangeCoderConstants::MaxTotalCount)
                    throw CommonLib::CExcBase("NumLen header is damaged: total count {0}", m_nTotal);

                m_nMantissaBytes = (size_t)((nMantissaBits + 7) / 8);
                m_pData = reader.Pos();
                m_pEnd = reader.End();
                if((size_t)(m_pEnd - m_pData) < m_nMantissaBytes)
                    throw CommonLib::CExcBase("NumLen data are damaged: {0} bytes of the low bits, {1} left", m_nMantissaBytes, (size_t)(m_pEnd - m_pData));
                Rewind();
            }

            void Rewind()
            {
                m_bits.Attach(m_pData, m_nMantissaBytes);
                m_coder.Start(m_pData + m_nMantissaBytes, m_pEnd);
            }

            uint64_t Total() const { return m_nTotal; }

            uint64_t Next()
            {
                uint64_t nFreq = m_coder.GetFreq(m_nTotal);
                uint32_t nSym = 0;
                while(nSym + 1 < m_nUsed && m_cum[nSym + 1] <= nFreq)
                    ++nSym;
                m_coder.Decode(m_cum[nSym], m_cum[nSym + 1]);

                uint32_t nLen = m_len[nSym];
                if(nLen <= 1)
                    return nLen;
                return (1ull << (nLen - 1)) | m_bits.Read(nLen - 1);
            }

        private:
            uint32_t   m_nUsed;
            TByte      m_len[CNumLenModel::SymbolCount];
            uint64_t   m_cum[CNumLenModel::SymbolCount + 1];
            uint64_t   m_nTotal;
            const TByte* m_pData;
            const TByte* m_pEnd;
            size_t     m_nMantissaBytes;
            CBitReader m_bits;
            CRangeDecoder m_coder;
        };
    }
}
