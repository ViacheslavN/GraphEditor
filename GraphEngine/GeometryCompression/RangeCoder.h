#pragma once
// Range coder (carryless, 64-bit, D. Subbotin) for the static models of the geometry compression.
// The algorithm is the one of CommonLib/compress/EntropyCoders/RangeCoder.h (TRangeEncoder64 / TRangeDecoder64),
// it outputs whole bytes (faster than the bitwise arithmetic coder), writes to a byte vector and reads the
// memory directly, so a shape is decoded in place.
// Finish writes only the bytes the decoder needs (it reads zeros after the end of the data).
// The total count of a model must not be greater than MaxTotalCount.

#include "CompressionUtils.h"

namespace GraphEngine
{
    namespace GeometryCompression
    {
        struct SRangeCoderConstants
        {
            static const uint32_t ValueBits = 56;                        // 64 - 8
            static const uint64_t Top = 1ull << ValueBits;
            static const uint64_t Bottom = 1ull << (ValueBits - 8);
            static const uint64_t MaxTotalCount = Bottom;
        };

        class CRangeEncoder : private SRangeCoderConstants
        {
        public:
            explicit CRangeEncoder(std::vector<TByte>& out) : m_out(out), m_nLow(0), m_nRange((uint64_t)-1) {}

            void Encode(uint64_t nLowCount, uint64_t nHighCount, uint64_t nTotalCount)
            {
                m_nLow += nLowCount * (m_nRange /= nTotalCount);
                m_nRange *= nHighCount - nLowCount;
                for(;;)
                {
                    if((m_nLow ^ (m_nLow + m_nRange)) < Top)
                        ;
                    else if(m_nRange < Bottom)
                        m_nRange = (0 - m_nLow) & (Bottom - 1);
                    else
                        break;

                    m_out.push_back((TByte)(m_nLow >> ValueBits));
                    m_nRange <<= 8;
                    m_nLow <<= 8;
                }
            }

            void Finish()
            {
                // the shortest value in [low, low + range) with zero low bytes: the decoder pads the data with zeros
                for(uint32_t nBytes = 1; nBytes <= 8; ++nBytes)
                {
                    uint64_t nMask = nBytes == 8 ? 0 : ((~0ull) >> (nBytes * 8));
                    uint64_t nValue = (m_nLow + nMask) & ~nMask;
                    if(nValue - m_nLow < m_nRange)   // also false when the rounding overflows
                    {
                        for(uint32_t i = 0; i < nBytes; ++i)
                            m_out.push_back((TByte)(nValue >> (56 - 8 * i)));
                        return;
                    }
                }
                for(int i = 0; i < 8; ++i)
                {
                    m_out.push_back((TByte)(m_nLow >> ValueBits));
                    m_nLow <<= 8;
                }
            }

        private:
            std::vector<TByte>& m_out;
            uint64_t m_nLow;
            uint64_t m_nRange;
        };

        class CRangeDecoder : private SRangeCoderConstants
        {
        public:
            CRangeDecoder() : m_pPos(nullptr), m_pEnd(nullptr), m_nLow(0), m_nRange((uint64_t)-1), m_nValue(0) {}

            void Start(const TByte* pData, const TByte* pEnd)
            {
                m_pPos = pData;
                m_pEnd = pEnd;
                m_nLow = 0;
                m_nRange = (uint64_t)-1;
                m_nValue = 0;
                for(int i = 0; i < 8; ++i)
                    m_nValue = (m_nValue << 8) | NextByte();
            }

            // the cumulative count of the next symbol, Decode must follow
            uint64_t GetFreq(uint64_t nTotalCount)
            {
                uint64_t nFreq = (m_nValue - m_nLow) / (m_nRange /= nTotalCount);
                return nFreq < nTotalCount ? nFreq : nTotalCount - 1;   // damaged data
            }

            void Decode(uint64_t nLowCount, uint64_t nHighCount)
            {
                m_nLow += nLowCount * m_nRange;
                m_nRange *= nHighCount - nLowCount;
                for(;;)
                {
                    if((m_nLow ^ (m_nLow + m_nRange)) < Top)
                        ;
                    else if(m_nRange < Bottom)
                        m_nRange = (0 - m_nLow) & (Bottom - 1);
                    else
                        break;

                    m_nValue = (m_nValue << 8) | NextByte();
                    m_nRange <<= 8;
                    m_nLow <<= 8;
                }
            }

        private:
            uint64_t NextByte() { return m_pPos < m_pEnd ? *m_pPos++ : 0; }

        private:
            const TByte* m_pPos;
            const TByte* m_pEnd;
            uint64_t m_nLow;
            uint64_t m_nRange;
            uint64_t m_nValue;
        };
    }
}
