#pragma once
// The coordinates of a shape are coded as a sequence of unsigned values (zigzag deltas).
// The encoder takes the smallest of three codings for every shape:
//   ValueCodingVarint    - 7 bits per byte, the best for a few small values (most of the buildings);
//   ValueCodingFixedBits - one bit width for all the values;
//   ValueCodingNumLen    - arithmetic coded bit lengths + the low bits (long lines and polygons).

#include "NumLenCoder.h"

namespace GraphEngine
{
    namespace GeometryCompression
    {
        enum eValueCoding
        {
            ValueCodingVarint    = 0,
            ValueCodingFixedBits = 1,
            ValueCodingNumLen    = 2
        };

        class CValueEncoder
        {
        public:
            // the coding with the smallest size of the values
            static eValueCoding ChooseCoding(const uint64_t* pValues, size_t nCount)
            {
                uint64_t nVarint = 0;
                uint32_t nWidth = 0;
                CNumLenModel model;
                for(size_t i = 0; i < nCount; ++i)
                {
                    nVarint += VarintSize(pValues[i]);
                    uint32_t nLen = BitLength(pValues[i]);
                    if(nLen > nWidth)
                        nWidth = nLen;
                    model.Add(pValues[i]);
                }
                uint64_t nFixed = 1 + (nWidth * (uint64_t)nCount + 7) / 8;

                eValueCoding coding = ValueCodingVarint;
                uint64_t nBest = nVarint;
                if(nFixed < nBest)
                {
                    coding = ValueCodingFixedBits;
                    nBest = nFixed;
                }
                if(model.IsCodable() && model.EstimateSize() < nBest)
                    coding = ValueCodingNumLen;
                return coding;
            }

            static void Encode(eValueCoding coding, const uint64_t* pValues, size_t nCount, std::vector<TByte>& out)
            {
                switch(coding)
                {
                    case ValueCodingFixedBits:
                    {
                        uint32_t nWidth = 0;
                        for(size_t i = 0; i < nCount; ++i)
                        {
                            uint32_t nLen = BitLength(pValues[i]);
                            if(nLen > nWidth)
                                nWidth = nLen;
                        }
                        out.push_back((TByte)nWidth);
                        CBitWriter bits(out);
                        for(size_t i = 0; i < nCount; ++i)
                            bits.Write(pValues[i], nWidth);
                        bits.Flush();
                        break;
                    }
                    case ValueCodingNumLen:
                    {
                        CNumLenModel model;
                        for(size_t i = 0; i < nCount; ++i)
                            model.Add(pValues[i]);
                        CNumLenEncoder encoder(model);
                        encoder.Encode(pValues, nCount, out);
                        break;
                    }
                    default:
                        for(size_t i = 0; i < nCount; ++i)
                            WriteVarint(out, pValues[i]);
                        break;
                }
            }
        };

        // returns the values one by one, reading the blob in place
        class CValueDecoder
        {
        public:
            CValueDecoder() : m_coding(ValueCodingVarint), m_nWidth(0) {}

            // nValues values take the rest of the reader; throws when the data can't have them
            void Open(eValueCoding coding, CByteReader& reader, uint64_t nValues)
            {
                m_coding = coding;
                switch(coding)
                {
                    case ValueCodingVarint:
                        if(reader.Remaining() < nValues)   // at least a byte per value
                            throw CommonLib::CExcBase("Compressed geometry is damaged: {0} values in {1} bytes", nValues, reader.Remaining());
                        m_start = reader;
                        break;
                    case ValueCodingFixedBits:
                        m_nWidth = reader.ReadByte();
                        reader.CheckError("the bit width");
                        if(m_nWidth > 64)
                            throw CommonLib::CExcBase("Compressed geometry is damaged: bit width {0}", m_nWidth);
                        if(reader.Remaining() < (m_nWidth * nValues + 7) / 8)
                            throw CommonLib::CExcBase("Compressed geometry is damaged: {0} values of {1} bits need more than {2} bytes",
                                                      nValues, m_nWidth, reader.Remaining());
                        m_start = reader;
                        break;
                    case ValueCodingNumLen:
                        m_numLen.Open(reader);
                        if(m_numLen.Total() != nValues)
                            throw CommonLib::CExcBase("Compressed geometry is damaged: {0} values are coded, {1} expected", m_numLen.Total(), nValues);
                        break;
                    default:
                        throw CommonLib::CExcBase("Compressed geometry is damaged: unknown coding {0}", (int)coding);
                }
                Rewind();
            }

            void Rewind()
            {
                switch(m_coding)
                {
                    case ValueCodingVarint:
                        m_varints = m_start;
                        break;
                    case ValueCodingFixedBits:
                        m_bits.Attach(m_start.Pos(), m_start.Remaining());
                        break;
                    case ValueCodingNumLen:
                        m_numLen.Rewind();
                        break;
                }
            }

            uint64_t Next()
            {
                switch(m_coding)
                {
                    case ValueCodingFixedBits:
                        return m_bits.Read(m_nWidth);
                    case ValueCodingNumLen:
                        return m_numLen.Next();
                    default:
                    {
                        uint64_t v = m_varints.ReadVarint();
                        m_varints.CheckError("the coordinates");
                        return v;
                    }
                }
            }

        private:
            eValueCoding   m_coding;
            CByteReader    m_start;
            CByteReader    m_varints;
            CBitReader     m_bits;
            uint32_t       m_nWidth;
            CNumLenDecoder m_numLen;
        };
    }
}
