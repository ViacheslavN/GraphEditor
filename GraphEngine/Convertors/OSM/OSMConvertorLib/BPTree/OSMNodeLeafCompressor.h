#pragma once

// TNodeLeafCompressor: the cut write leaf compressor of the node tree.

#include "../../../../../CommonLib/CommonLib.h"
#include "../../../../../bptreedb/BPtreeLib/BPTreeContext.h"
#include "../../../../../bptreedb/BPtreeLib/BaseNodeCompressor.h"
#include "../../../../../bptreedb/BPtreeLib/BaseDIffEncoder.h"
#include "../../../../../bptreedb/BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "OSMPointCompressor.h"
#include "OSMNodeLeafEncoder.h"

#include <algorithm>
#include <atomic>

namespace GraphEngine {
    namespace Convertors {
        namespace bptree {

            // the keys of the node tree: sorted ids, the first raw, the differences NumLen coded
            typedef bptreedb::TValueDiffEncoder<int64_t, int64_t, bptreedb::CUnsignedNumLenEncoder> TNodeIdEncoder;

            // The leaf compressor of the node tree: a cut write compressor (NodeCompressorTraits.h, CutWrite).
            //
            // A leaf is kept in the node cache of the tree as the decoded vectors and is compressed only when it is saved,
            // so nothing is computed while the nodes are appended: no statistics (Insert only counts), no size estimate
            // (IsNeedSplit only compares the count with a limit). Write encodes the first elements that fit (CNodeLeafEncoder)
            // and returns the number of the others (the tree moves them to the next leaf: BulkLoad, CheckLeafNode).
            // The count limit: the first leaf - page size * "CompressRatio" / raw entry size, then the capacity of the last
            // written leaf * "CutOverfill" percent (the elements over it are carried to the next leaf). The limit is shared by
            // the leaves of all the node trees of the process (a hint only).
            //
            // The statistics of the encoders of the base aren't kept: the size based decisions of removes (IsHalfEmpty,
            // PossibleUnion) see an empty node. The node tree is a write once index (bulk load + inserts): removes are
            // correct but rebalance badly.
            class TNodeLeafCompressor : public bptreedb::TBaseNodeCompressor<int64_t, SOSMPointE7, TNodeIdEncoder, TPointCompressor>
            {
            public:
                typedef bptreedb::TBaseNodeCompressor<int64_t, SOSMPointE7, TNodeIdEncoder, TPointCompressor> TBase;
                typedef TBase::TKeyMemSet TKeyMemSet;
                typedef TBase::TValueMemSet TValueMemSet;

                static constexpr bool CutWrite = true;          // NodeCompressorTraits.h
                static const int32_t DefaultCompressRatio = 3;
                static const int32_t DefaultCutOverfill = 125;  // percent

                TNodeLeafCompressor(uint32_t nPageSize, bptreedb::TAllocsSetPtr pAllocsSet, bptreedb::TCompressorParamsBasePtr pParams,
                                    bptreedb::ECompressNodeType type) :
                        TBase(nPageSize, pAllocsSet, pParams, type),
                        m_encoder(pAllocsSet, pParams)
                {
                    int32_t nRatio = DefaultCompressRatio;
                    int32_t nOverfill = DefaultCutOverfill;
                    bptreedb::TCompressorParamsPtr pKeyParams = pParams.get() ? pParams->GetCompressParams(bptreedb::eLeafKey) : bptreedb::TCompressorParamsPtr();
                    if (pKeyParams.get())
                    {
                        nRatio = pKeyParams->GetIntParam("CompressRatio", DefaultCompressRatio);
                        nOverfill = pKeyParams->GetIntParam("CutOverfill", DefaultCutOverfill);
                    }
                    m_nOverfill = (uint32_t)(std::max)(100, nOverfill);
                    this->m_ValueEncoder.SetKeepStatistics(false);   // Write computes them (CNodeLeafEncoder)
                    uint32_t nLimit = LimitHint().load(std::memory_order_relaxed);
                    if (nLimit == 0)
                        nLimit = (uint32_t)((uint64_t)nPageSize * (uint64_t)(std::max)(1, nRatio) / this->TupleSize());
                    m_nLimit = (std::max)(CNodeLeafEncoder::CutCheckStep, (std::min)(nLimit, this->m_nMaxCount));
                }

                // the page size of the compressor of a leaf of the tree pages of nPagePayload bytes: the leaf flag, next, prev
                static uint32_t LeafPageSize(uint32_t nPagePayload) { return nPagePayload - (uint32_t)(sizeof(bool) + 2 * sizeof(int64_t)); }
                uint32_t MaxCount() const { return this->m_nMaxCount; }

                // no statistics: the count only
                virtual bool Insert(int, const int64_t&, const SOSMPointE7&, const TKeyMemSet&, const TValueMemSet&) override
                {
                    this->m_nCount++;
                    return true;
                }
                virtual void Remove(uint32_t, const int64_t&, const SOSMPointE7&, const TKeyMemSet&, const TValueMemSet&) override
                {
                    this->m_nCount--;
                }
                virtual void Recalc(const TKeyMemSet& vecKeys, const TValueMemSet&) override
                {
                    this->m_nCount = (uint32_t)vecKeys.size();
                }
                virtual void RecalcKey(const TKeyMemSet& vecKeys) override
                {
                    this->m_nCount = (uint32_t)vecKeys.size();
                }
                virtual void UpdateValue(uint32_t, SOSMPointE7&, const SOSMPointE7&, const TValueMemSet&, const TKeyMemSet&) override {}
                virtual void UpdateKey(uint32_t, const int64_t&, const int64_t&, const TKeyMemSet&, const TValueMemSet&) override {}

                virtual bool IsNeedSplit() const override
                {
                    return this->m_nCount > m_nLimit || this->m_nCount > this->m_nMaxCount;
                }

                // the first elements that fit are encoded; returns the number of the others (0 - all are written)
                virtual uint32_t Write(TKeyMemSet& vecKeys, TValueMemSet& vecValues, CommonLib::IMemoryWriteStream* pStream,
                                       bptreedb::CBPTreeContext*) override
                {
                    uint32_t n = (uint32_t)vecKeys.size();
                    try
                    {
                        if (n != this->m_nCount || vecValues.size() != n)
                            throw CommonLib::CExcBase("wrong size, count: {0}, keys: {1}, values: {2}", this->m_nCount, n, (uint32_t)vecValues.size());
                        if (n == 0)
                        {
                            pStream->Write((uint32_t)0);
                            return 0;
                        }

                        uint32_t nFit = m_encoder.FindFit(vecKeys, vecValues, n, this->m_nPageSize, this->m_nMaxCount);
                        m_encoder.Encode(vecKeys, vecValues, pStream);

                        // the limit of the next leaves: the count which fits in a page (this leaf is cut where the page is full,
                        // or all its elements fit: by the estimate of their size) + the overfill
                        uint64_t nCapacity = nFit < n ? nFit : (uint64_t)nFit * this->m_nPageSize / (std::max)(1u, m_encoder.FitSize());
                        m_nLimit = (uint32_t)(std::max)((uint64_t)CNodeLeafEncoder::CutCheckStep, (std::min)(nCapacity * m_nOverfill / 100, (uint64_t)this->m_nMaxCount));
                        LimitHint().store(m_nLimit, std::memory_order_relaxed);
                        return n - nFit;
                    }
                    catch (std::exception& exc)
                    {
                        CommonLib::CExcBase::RegenExcT("[TNodeLeafCompressor] failed to write the leaf, count: {0}", n, exc);
                        throw;
                    }
                }

            private:
                static std::atomic<uint32_t>& LimitHint()
                {
                    static std::atomic<uint32_t> nLimit{0};
                    return nLimit;
                }

            private:
                CNodeLeafEncoder m_encoder;
                uint32_t m_nLimit = 0;      // IsNeedSplit: the count over which the leaf is cut by Write
                uint32_t m_nOverfill = DefaultCutOverfill;
            };
        }
    }
}
