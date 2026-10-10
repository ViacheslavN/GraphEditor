#pragma once
// Small building blocks of the geometry compression: zigzag, bit length, varints,
// a byte reader and LSB-first bit writer / reader working directly on memory
// (no virtual streams: the decoder reads the blob of a shape in place, without copying).

#include <cstdint>
#include <cstddef>
#include <vector>
#include "../CommonLib/CommonLib.h"
#include "../CommonLib/exception/exc_base.h"

#if defined(_MSC_VER)
#include <intrin.h>
#endif

namespace GraphEngine
{
    namespace GeometryCompression
    {
        typedef uint8_t TByte;

        inline uint64_t ZigZag(int64_t v)
        {
            return ((uint64_t)v << 1) ^ (uint64_t)(v >> 63);
        }

        inline int64_t UnZigZag(uint64_t u)
        {
            return (int64_t)(u >> 1) ^ -(int64_t)(u & 1);
        }

        // the number of significant bits: 0 -> 0, 1 -> 1, 2..3 -> 2, 4..7 -> 3 ...
        inline uint32_t BitLength(uint64_t v)
        {
            if(v == 0)
                return 0;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_ARM64))
            unsigned long nIndex = 0;
            _BitScanReverse64(&nIndex, v);
            return (uint32_t)nIndex + 1;
#elif defined(__GNUC__) || defined(__clang__)
            return 64 - (uint32_t)__builtin_clzll(v);
#else
            uint32_t n = 0;
            while(v)
            {
                ++n;
                v >>= 1;
            }
            return n;
#endif
        }

        inline uint32_t VarintSize(uint64_t v)
        {
            uint32_t n = 1;
            while(v >= 0x80)
            {
                v >>= 7;
                ++n;
            }
            return n;
        }

        inline void WriteVarint(std::vector<TByte>& out, uint64_t v)
        {
            while(v >= 0x80)
            {
                out.push_back((TByte)(v | 0x80));
                v >>= 7;
            }
            out.push_back((TByte)v);
        }

        // reads a buffer in place; reading past the end sets the error flag and returns zeros
        class CByteReader
        {
        public:
            CByteReader() : m_pBegin(nullptr), m_pPos(nullptr), m_pEnd(nullptr), m_bError(false) {}
            CByteReader(const TByte* pData, size_t nSize) { Attach(pData, nSize); }

            void Attach(const TByte* pData, size_t nSize)
            {
                m_pBegin = m_pPos = pData;
                m_pEnd = pData + nSize;
                m_bError = false;
            }

            TByte ReadByte()
            {
                if(m_pPos >= m_pEnd)
                {
                    m_bError = true;
                    return 0;
                }
                return *m_pPos++;
            }

            uint64_t ReadVarint()
            {
                uint64_t v = 0;
                for(uint32_t nShift = 0; nShift < 64; nShift += 7)
                {
                    if(m_pPos >= m_pEnd)
                    {
                        m_bError = true;
                        return 0;
                    }
                    TByte b = *m_pPos++;
                    v |= (uint64_t)(b & 0x7F) << nShift;
                    if((b & 0x80) == 0)
                        return v;
                }
                m_bError = true;
                return v;
            }

            int64_t ReadZigZag() { return UnZigZag(ReadVarint()); }

            // throws when the data were read past the end
            void CheckError(const char* pszWhat) const
            {
                if(m_bError)
                    throw CommonLib::CExcBase("Compressed geometry is damaged: unexpected end of {0}", std::string(pszWhat));
            }

            bool Skip(size_t nBytes)
            {
                if((size_t)(m_pEnd - m_pPos) < nBytes)
                {
                    m_bError = true;
                    m_pPos = m_pEnd;
                    return false;
                }
                m_pPos += nBytes;
                return true;
            }

            const TByte* Pos() const { return m_pPos; }
            const TByte* End() const { return m_pEnd; }
            size_t       Offset() const { return (size_t)(m_pPos - m_pBegin); }
            size_t       Remaining() const { return (size_t)(m_pEnd - m_pPos); }
            bool         IsError() const { return m_bError; }

        private:
            const TByte* m_pBegin;
            const TByte* m_pPos;
            const TByte* m_pEnd;
            bool         m_bError;
        };

        // bits are written from the lowest bit of a byte
        class CBitWriter
        {
        public:
            explicit CBitWriter(std::vector<TByte>& out) : m_out(out), m_nAcc(0), m_nBits(0) {}

            void Write(uint64_t v, uint32_t nBits)
            {
                if(nBits > 32)
                {
                    Write32((uint32_t)v, 32);
                    v >>= 32;
                    nBits -= 32;
                }
                Write32((uint32_t)v, nBits);
            }

            void Flush()
            {
                if(m_nBits > 0)
                    m_out.push_back((TByte)m_nAcc);
                m_nAcc = 0;
                m_nBits = 0;
            }

        private:
            void Write32(uint32_t v, uint32_t nBits)
            {
                if(nBits == 0)
                    return;
                uint64_t nMask = nBits == 32 ? 0xFFFFFFFFull : ((1ull << nBits) - 1);
                m_nAcc |= ((uint64_t)v & nMask) << m_nBits;
                m_nBits += nBits;
                while(m_nBits >= 8)
                {
                    m_out.push_back((TByte)m_nAcc);
                    m_nAcc >>= 8;
                    m_nBits -= 8;
                }
            }

        private:
            std::vector<TByte>& m_out;
            uint64_t m_nAcc;
            uint32_t m_nBits;
        };

        class CBitReader
        {
        public:
            CBitReader() : m_pPos(nullptr), m_pEnd(nullptr), m_nAcc(0), m_nBits(0) {}

            void Attach(const TByte* pData, size_t nSize)
            {
                m_pPos = pData;
                m_pEnd = pData + nSize;
                m_nAcc = 0;
                m_nBits = 0;
            }

            uint64_t Read(uint32_t nBits)
            {
                if(nBits > 32)
                {
                    uint64_t nLow = Read32(32);
                    return nLow | ((uint64_t)Read32(nBits - 32) << 32);
                }
                return Read32(nBits);
            }

        private:
            uint32_t Read32(uint32_t nBits)
            {
                if(nBits == 0)
                    return 0;
                while(m_nBits < nBits)
                {
                    uint64_t b = m_pPos < m_pEnd ? *m_pPos++ : 0;
                    m_nAcc |= b << m_nBits;
                    m_nBits += 8;
                }
                uint64_t nMask = nBits == 32 ? 0xFFFFFFFFull : ((1ull << nBits) - 1);
                uint32_t v = (uint32_t)(m_nAcc & nMask);
                m_nAcc >>= nBits;
                m_nBits -= nBits;
                return v;
            }

        private:
            const TByte* m_pPos;
            const TByte* m_pEnd;
            uint64_t m_nAcc;
            uint32_t m_nBits;
        };
    }
}
