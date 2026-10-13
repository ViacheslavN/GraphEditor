#pragma once
#include "DeVector.h"

#include "BPTreeLeafNode.h"
#include "AllocsSet.h"

namespace bptreedb
{
	// Leaf node of a map.
	//
	// Pending changes buffer (optional, leaf compressor param "PendingLimit", 0 - off):
	// insert / remove by key don't touch the sorted key/value vectors, they go to a small sorted
	// buffer of pending changes (insert, delete, replace). The buffer is merged into the vectors in one
	// pass (Materialize) when it is full, before any access by index (Key(i) / Value(i) except Key(0),
	// split, union, alignment, save ...), and the compressor statistics are recalculated once per merge.
	// So a run of changes in one leaf costs O(log n + limit) per change instead of moving the vectors
	// (O(n)) on every change. Split / rebalance decisions are taken when the buffer is merged.
	template<typename _TKey, typename _TValue,
		class _TCompressor>
	class BPTreeLeafNodeMap : public  BPTreeLeafNodeSetBase<_TKey, _TCompressor>
	{
	public:

		typedef BPTreeLeafNodeSetBase<_TKey, _TCompressor> TBase;
		typedef _TValue TValue;
		typedef typename TBase::TLink TLink;
		typedef typename TBase::TKey TKey;
		typedef typename TBase::TCompressor TCompressor;
		typedef typename TBase::TAlloc TAlloc;
		typedef typename TBase::TKeyMemSet TKeyMemSet;
		typedef CommonLib::STLAllocator<TValue> TValueAlloc;
		typedef TDeVector<TValue, TValueAlloc> TValueMemSet;

		enum EPendingOp : uint8_t
		{
			ePendingInsert = 1,   // key isn't in the vectors
			ePendingDelete = 2,   // key is in the vectors, removed
			ePendingReplace = 3   // key is in the vectors, removed and inserted again with a new value
		};

		struct SPending
		{
			TKey m_key;
			TValue m_value;
			uint32_t m_nPos;      // position in m_KeyMemSet: the element itself (delete, replace) or the insert position
			EPendingOp m_op;
		};
		typedef std::vector<SPending> TPendingSet;

		BPTreeLeafNodeMap(TAllocsSetPtr pAllocsSet, uint32_t nPageSize, TCompressorParamsBasePtr pParams) : TBase(pAllocsSet, nPageSize, pParams),
			m_ValueMemSet(TValueAlloc(pAllocsSet->GetCommonAlloc()))
		{
			if (pParams.get())
			{
				TCompressorParamsPtr pLeafKeyParams = pParams->GetCompressParams(eLeafKey);
				if (pLeafKeyParams.get())
				{
					m_nPendingLimit = (uint32_t)pLeafKeyParams->GetIntParam("PendingLimit", 0);
					m_bCountMode = pLeafKeyParams->GetIntParam("CountLimitMode", 0) != 0;
					m_bSplitAtFit = pLeafKeyParams->GetIntParam("SplitAtFit", 0) != 0;
				}
			}
			m_nCountLimit = this->m_Compressor.GetMaxCount();
		}

		virtual ~BPTreeLeafNodeMap()
		{

		}

		virtual  uint32_t Save(CommonLib::IMemoryWriteStream* stream, CBPTreeContext *pContext)
		{
			try
			{
				Materialize();
				EnsureStats();
				stream->Write(this->m_nNext);
				stream->Write(this->m_nPrev);

				size_t nStart = stream->Pos();
				uint32_t nRet = this->m_Compressor.Write(this->m_KeyMemSet, this->m_ValueMemSet, stream, pContext);
				if (nRet == 0)
				{
					UpdateCountLimit(uint32_t(stream->Pos() - nStart));
					return 0;
				}

				// doesn't fit: the caller (SaveNode) splits off nRet last elements and saves again
				if (m_bSplitAtFit)
					return SplitCount(true);

				return nRet;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("Leaf node  failed to save", exc);
				throw;
			}
		}

		virtual void Load(CommonLib::IMemoryReadStream* stream, CBPTreeContext *pContext)
		{
			try
			{
				ClearPending();
				stream->Read(this->m_nNext);
				stream->Read(this->m_nPrev);
				size_t nStart = stream->Pos();
				this->m_Compressor.Load(this->m_KeyMemSet, this->m_ValueMemSet, stream, pContext);
				m_bStatsDirty = false;
				UpdateCountLimit(uint32_t(stream->Pos() - nStart));
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("Leaf node  failed to load", exc);
			}
		}

		virtual void Clear()
		{
			ClearPending();
			this->m_KeyMemSet.clear();
			this->m_ValueMemSet.clear();
			this->m_Compressor.Clear();
			m_bStatsDirty = false;
		}

		//------------------------------------------------------------------------------------------
		// size / count

		virtual uint32_t Count() const
		{
			return (uint32_t)this->m_KeyMemSet.size() + m_nPendingInserts - m_nPendingDeletes;
		}

		virtual uint32_t Size() const
		{
			Self()->Materialize();
			Self()->EnsureStats();
			return TBase::Size();
		}

		virtual uint32_t RowSize() const
		{
			Self()->Materialize();
			Self()->EnsureStats();
			return TBase::RowSize();
		}

		virtual bool IsNeedSplit() const
		{
			if (m_bCountMode && m_bStatsDirty && m_Pending.empty())
			{
				// count limit mode: inserts don't update the compressor statistics, only the element
				// count is compared with the limit derived from the measured bits per element. Above it
				// the statistics are rebuilt once, the limit is corrected and the real size decides.
				if (Count() <= m_nCountLimit)
					return false;

				Self()->EnsureStats();
				Self()->UpdateCountLimit(this->m_Compressor.RowSize());
				return this->m_Compressor.IsNeedSplit();
			}

			if (!m_Pending.empty())
			{
				// The compressor statistics are of the vectors only. Without a merge we only know an upper
				// bound of the growth: an inserted entry adds at most two diffs per stream.
				const uint32_t nMaxEntryBytes = 2 * (uint32_t)(sizeof(TKey) + sizeof(TValue) + 2);
				uint64_t nEstimate = (uint64_t)this->m_Compressor.Size() + (uint64_t)m_nPendingInserts * nMaxEntryBytes;
				uint64_t nCount = (uint64_t)this->m_Compressor.Count() + m_nPendingInserts;
				if (nEstimate < this->m_Compressor.GetPageSize() && nCount <= this->m_Compressor.GetMaxCount())
					return false;

				Self()->Materialize();
			}
			return this->m_Compressor.IsNeedSplit();
		}

		virtual bool IsHalfEmpty() const
		{
			// rebalancing is decided when the buffer is merged (a merge happens inside a remove when the
			// buffer is full, the remove's caller asks IsHalfEmpty right after it)
			if (!m_Pending.empty())
			{
				// The vectors don't change while there are pending changes. As long as the node still has
				// more than half of the entries it had then, rebalancing is postponed to the merge; below
				// that the node is merged and checked as usual, so it never gets empty without a check.
				if (Count() * 2 > (uint32_t)this->m_KeyMemSet.size())
					return false;

				Self()->Materialize();
			}

			Self()->EnsureStats();
			return this->m_Compressor.IsHalfEmpty();
		}

		//------------------------------------------------------------------------------------------
		// insert / remove / search

		template<class TComp>
		void insert(TComp& comp, const TKey& key, const TValue& value, int nInsertLeafIndex = -1)
		{
			if (UsePending() && nInsertLeafIndex == -1)
			{
				InsertPending(comp, key, value);
				m_bLastInsertAppend = false;
				return;
			}

			Materialize();
			int32_t nIndex = 0;
			this->insertImp(comp, key, nIndex, nInsertLeafIndex);

			this->m_ValueMemSet.insert(std::next(this->m_ValueMemSet.begin(), nIndex), value);
			m_bLastInsertAppend = (uint32_t)nIndex + 1 == (uint32_t)this->m_KeyMemSet.size();
			if (m_bCountMode || m_bStatsDirty)
				m_bStatsDirty = true;
			else
				this->m_Compressor.Insert(nIndex, this->m_KeyMemSet[nIndex], this->m_ValueMemSet[nIndex], this->m_KeyMemSet, this->m_ValueMemSet);
		}

		// bulk load: key is greater than every key of the node
		void AppendSorted(const TKey& key, const TValue& value)
		{
			Materialize();
			EnsureStats();
			this->m_KeyMemSet.push_back(key);
			this->m_ValueMemSet.push_back(value);
			uint32_t nIndex = (uint32_t)this->m_KeyMemSet.size() - 1;
			this->m_Compressor.Insert(nIndex, this->m_KeyMemSet[nIndex], this->m_ValueMemSet[nIndex], this->m_KeyMemSet, this->m_ValueMemSet);
		}

		template<class TComp>
		void RemoveKey(TComp& comp, int32_t nIndex, const TKey& key)
		{
			if (UsePending())
			{
				RemovePending(comp, key);
				return;
			}

			Materialize();
			RemoveByIndex(nIndex);
		}

		void RemoveByIndex(int32_t nIndex)
		{
			Materialize();
			if (!m_bStatsDirty)   // dirty statistics are recalculated before use
				this->m_Compressor.Remove(nIndex, this->m_KeyMemSet[nIndex], this->m_ValueMemSet[nIndex], this->m_KeyMemSet, this->m_ValueMemSet);
			this->m_KeyMemSet.erase(std::next(this->m_KeyMemSet.begin(), nIndex));
			m_ValueMemSet.erase(std::next(m_ValueMemSet.begin(), nIndex));
		}

		// index of key in the merged view, -1 - not found
		template<class TComp>
		int32_t binary_search(TComp& comp, const TKey& key)
		{
			if (m_Pending.empty())
				return TBase::binary_search(comp, key);

			auto pit = PendingLowerBound(comp, key);
			auto bit = std::lower_bound(this->m_KeyMemSet.begin(), this->m_KeyMemSet.end(), key, comp);

			bool bExists;
			if (pit != m_Pending.end() && comp.EQ(pit->m_key, key))
				bExists = pit->m_op != ePendingDelete;
			else
				bExists = bit != this->m_KeyMemSet.end() && comp.EQ(*bit, key);

			if (!bExists)
				return -1;

			// the pending entries before pit are the keys less than key
			int32_t nIndex = (int32_t)std::distance(this->m_KeyMemSet.begin(), bit);
			for (auto it = m_Pending.begin(); it != pit; ++it)
			{
				if (it->m_op == ePendingInsert)
					++nIndex;
				else if (it->m_op == ePendingDelete)
					--nIndex;
			}
			return nIndex;
		}

		template<class TComp>
		int32_t upper_bound(TComp& comp, const TKey& key)
		{
			Materialize();
			return TBase::upper_bound(comp, key);
		}

		template<class TComp>
		int32_t lower_bound(TComp& comp, const TKey& key)
		{
			Materialize();
			return TBase::lower_bound(comp, key);
		}

		template<class TComp>
		bool IsKey(TComp& comp, const TKey& key, uint32_t nIndex)
		{
			Materialize();
			return TBase::IsKey(comp, key, nIndex);
		}

		//------------------------------------------------------------------------------------------
		// access by index (merged view)

		const TKey& Key(uint32_t nIndex) const
		{
			if (!m_Pending.empty())
			{
				if (nIndex == 0)
					return FirstKey();
				Self()->Materialize();
			}
			return TBase::Key(nIndex);
		}

		TKey& Key(uint32_t nIndex)
		{
			// the tree reaches leaves through non-const holders, Key(0) is asked on every remove
			// (GetParentNode): keep it without a merge. Keys are never modified through this reference.
			if (!m_Pending.empty() && nIndex == 0)
				return const_cast<TKey&>(FirstKey());

			Materialize();
			return TBase::Key(nIndex);
		}

		const TValue& Value(uint32_t nIndex) const
		{
			Self()->Materialize();
			return this->m_ValueMemSet[nIndex];
		}

		TValue& Value(uint32_t nIndex)
		{
			Materialize();
			return this->m_ValueMemSet[nIndex];
		}

		//------------------------------------------------------------------------------------------
		// split / union / alignment (always on merged nodes)

		int32_t  SplitIn(BPTreeLeafNodeMap *pNode, int32_t count, TKey* pSplitKey)
		{
			try
			{
				Materialize();
				pNode->Materialize();

				if (count > (this->m_KeyMemSet.size() - 1))
					throw CommonLib::CExcBase("invalid count {0}", count);

				TKeyMemSet& newNodeMemSet = pNode->m_KeyMemSet;
				TValueMemSet& newNodValueMemSet = pNode->m_ValueMemSet;
				TCompressor& NewNodeComp = pNode->m_Compressor;


				int32_t nBegin = (int32_t)this->m_KeyMemSet.size() - count;

				int32_t nSplitIndex = this->SplitInVec(this->m_KeyMemSet, newNodeMemSet, pSplitKey, nBegin, (int32_t)this->m_KeyMemSet.size());
				this->SplitInVec(this->m_ValueMemSet, newNodValueMemSet, (TValue*)NULL, nBegin, (int32_t)this->m_ValueMemSet.size());

				this->m_Compressor.Recalc(this->m_KeyMemSet, this->m_ValueMemSet);
				NewNodeComp.Recalc(newNodeMemSet, newNodValueMemSet);
				m_bStatsDirty = false;
				pNode->m_bStatsDirty = false;
				pNode->m_nCountLimit = m_nCountLimit;
				return nSplitIndex;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[BPTreeLeafNode] failed to SplitIn", exc);
				throw;
			}

		}

		int32_t  SplitIn(BPTreeLeafNodeMap *pLeftNode, BPTreeLeafNodeMap *pRightNode, TKey* pSplitKey)
		{
			try
			{
				Materialize();
				pLeftNode->Materialize();
				pRightNode->Materialize();

				TKeyMemSet& leftNodeMemSet = pLeftNode->m_KeyMemSet;
				TValueMemSet& leftValueMemSet = pLeftNode->m_ValueMemSet;
				TCompressor& pleftNodeComp = pLeftNode->m_Compressor;

				TKeyMemSet& rightNodeMemSet = pRightNode->m_KeyMemSet;
				TValueMemSet& rightValueMemSet = pRightNode->m_ValueMemSet;
				TCompressor& pRightNodeComp = pRightNode->m_Compressor;


				uint32_t nSize = (uint32_t)this->m_KeyMemSet.size() / 2;

				if (pSplitKey)
					*pSplitKey = this->m_KeyMemSet[nSize];

				this->SplitInVec(this->m_KeyMemSet, leftNodeMemSet, 0, nSize);
				this->SplitInVec(this->m_KeyMemSet, rightNodeMemSet, nSize, (uint32_t)this->m_KeyMemSet.size() - nSize);


				this->SplitInVec(m_ValueMemSet, leftValueMemSet, 0, nSize);
				this->SplitInVec(m_ValueMemSet, rightValueMemSet, nSize, (uint32_t)m_ValueMemSet.size() - nSize);

				pleftNodeComp.Recalc(leftNodeMemSet, leftValueMemSet);
				pRightNodeComp.Recalc(rightNodeMemSet, rightValueMemSet);
				pLeftNode->m_bStatsDirty = false;
				pRightNode->m_bStatsDirty = false;
				pLeftNode->m_nCountLimit = m_nCountLimit;
				pRightNode->m_nCountLimit = m_nCountLimit;

				return nSize;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[BPTreeLeafNode] failed to SplitIn", exc);
				throw;
			}

		}

		bool UnionWith(BPTreeLeafNodeMap* pNode, bool bLeft, int *nCheckIndex = 0)
		{
			Materialize();
			pNode->Materialize();

			this->UnionVec(this->m_KeyMemSet, pNode->m_KeyMemSet, bLeft, nCheckIndex);
			this->UnionVec(this->m_ValueMemSet, pNode->m_ValueMemSet, bLeft);

			this->m_Compressor.Recalc(this->m_KeyMemSet, this->m_ValueMemSet);
			m_bStatsDirty = false;

			return true;
		}

		bool AlignmentOf(BPTreeLeafNodeMap* pNode, bool bFromLeft)
		{
			Materialize();
			pNode->Materialize();

			if (!this->AlignmentOfVec(this->m_KeyMemSet, pNode->m_KeyMemSet, bFromLeft))
				return false;

			this->AlignmentOfVec(this->m_ValueMemSet, pNode->m_ValueMemSet, bFromLeft);

			pNode->Recalc();
			Recalc();

			return true;
		}

		bool PossibleUnion(BPTreeLeafNodeMap *pNode)
		{
			Materialize();
			pNode->Materialize();
			EnsureStats();
			pNode->EnsureStats();
			return TBase::PossibleUnion(pNode);
		}

		bool PossibleAlignment(BPTreeLeafNodeMap *pNode)
		{
			Materialize();
			pNode->Materialize();
			EnsureStats();
			pNode->EnsureStats();
			return TBase::PossibleAlignment(pNode);
		}

		void Recalc()
		{
			Materialize();
			this->m_Compressor.Recalc(this->m_KeyMemSet, this->m_ValueMemSet);
			m_bStatsDirty = false;
		}

		bool IsUnion(BPTreeLeafNodeMap *pNode) {

			Materialize();
			pNode->Materialize();
			return this->m_Compressor.IsUnion(pNode->m_Compressor);
		}

		bool IsAlignment(BPTreeLeafNodeMap *pNode)
		{
			Materialize();
			pNode->Materialize();
			return this->m_Compressor.IsAlignment(pNode->m_Compressor);
		}

		//------------------------------------------------------------------------------------------
		// pending changes

		bool UsePending() const { return m_nPendingLimit != 0; }

		// elements a split moves to the new right node: from the element that doesn't fit (SplitAtFit)
		// or half of the node
		// bForSave: the node doesn't fit in the page at save time - always at the fit point.
		// On insert: at the fit point only when the last insert was an append at the end of the node
		// (sequential load, the left node stays full, as SQLite's balance_quick); otherwise in half,
		// random inserts would leave a full node and a one element node at every split.
		uint32_t SplitCount(bool bForSave = false)
		{
			Materialize();
			uint32_t nCount = (uint32_t)this->m_KeyMemSet.size();
			if (!m_bSplitAtFit || nCount < 2 || (!bForSave && !m_bLastInsertAppend))
				return nCount / 2;

			uint32_t nFit = this->m_Compressor.CalcFitCount(this->m_KeyMemSet, this->m_ValueMemSet);
			m_bStatsDirty = true;    // the statistics are of the prefix now
			if (nFit == 0)
				return nCount / 2;
			if (nFit >= nCount)
				return std::max<uint32_t>(1, nCount / 64);   // the estimate fits, the real encoding doesn't
			return nCount - nFit;
		}

		void EnsureStats()
		{
			if (!m_bStatsDirty)
				return;

			this->m_Compressor.Recalc(this->m_KeyMemSet, this->m_ValueMemSet);
			m_bStatsDirty = false;
		}

		// count limit from the compressed size of this node: page bits / bits per element + 10%
		void UpdateCountLimit(uint32_t nCompressedBytes)
		{
			uint32_t nCount = (uint32_t)this->m_KeyMemSet.size();
			if (nCount < 32 || nCompressedBytes == 0)
				return;

			double dBitsPerElement = (double)nCompressedBytes * 8.0 / nCount;
			double dLimit = (double)this->m_Compressor.GetPageSize() * 8.0 / dBitsPerElement * 1.1;
			uint32_t nMax = this->m_Compressor.GetMaxCount();
			m_nCountLimit = dLimit > nMax ? nMax : std::max<uint32_t>(32, (uint32_t)dLimit);
		}

		// merges the pending changes into the vectors: one pass, one compressor recalculation
		void Materialize()
		{
			if (m_Pending.empty())
				return;

			TKeyMemSet newKeys(this->m_KeyMemSet.get_allocator());
			TValueMemSet newValues(this->m_ValueMemSet.get_allocator());
			newKeys.reserve(Count());
			newValues.reserve(Count());

			uint32_t nBase = 0;
			for (const SPending& pending : m_Pending)
			{
				newKeys.insert(newKeys.end(), std::next(this->m_KeyMemSet.begin(), nBase), std::next(this->m_KeyMemSet.begin(), pending.m_nPos));
				newValues.insert(newValues.end(), std::next(this->m_ValueMemSet.begin(), nBase), std::next(this->m_ValueMemSet.begin(), pending.m_nPos));
				nBase = pending.m_nPos;

				switch (pending.m_op)
				{
				case ePendingInsert:
					newKeys.push_back(pending.m_key);
					newValues.push_back(pending.m_value);
					break;
				case ePendingDelete:
					++nBase;
					break;
				case ePendingReplace:
					newKeys.push_back(pending.m_key);
					newValues.push_back(pending.m_value);
					++nBase;
					break;
				}
			}
			newKeys.insert(newKeys.end(), std::next(this->m_KeyMemSet.begin(), nBase), this->m_KeyMemSet.end());
			newValues.insert(newValues.end(), std::next(this->m_ValueMemSet.begin(), nBase), this->m_ValueMemSet.end());

			this->m_KeyMemSet.swap(newKeys);
			this->m_ValueMemSet.swap(newValues);
			ClearPending();
			this->m_Compressor.Recalc(this->m_KeyMemSet, this->m_ValueMemSet);
			m_bStatsDirty = false;
		}

	private:

		BPTreeLeafNodeMap* Self() const { return const_cast<BPTreeLeafNodeMap*>(this); }

		void ClearPending()
		{
			m_Pending.clear();
			m_nPendingInserts = 0;
			m_nPendingDeletes = 0;
		}

		template<class TComp>
		typename TPendingSet::iterator PendingLowerBound(TComp& comp, const TKey& key)
		{
			return std::lower_bound(m_Pending.begin(), m_Pending.end(), key,
				[&comp](const SPending& pending, const TKey& k) { return comp(pending.m_key, k); });
		}

		template<class TComp>
		void InsertPending(TComp& comp, const TKey& key, const TValue& value)
		{
			auto pit = PendingLowerBound(comp, key);
			if (pit != m_Pending.end() && comp.EQ(pit->m_key, key))
			{
				if (pit->m_op != ePendingDelete)
					throw CommonLib::CExcBase("Failed to insert: key exists");

				pit->m_op = ePendingReplace;
				pit->m_value = value;
				--m_nPendingDeletes;
			}
			else
			{
				auto bit = std::lower_bound(this->m_KeyMemSet.begin(), this->m_KeyMemSet.end(), key, comp);
				if (bit != this->m_KeyMemSet.end() && comp.EQ(*bit, key))
					throw CommonLib::CExcBase("Failed to insert: key exists");

				SPending pending{ key, value, (uint32_t)std::distance(this->m_KeyMemSet.begin(), bit), ePendingInsert };
				m_Pending.insert(pit, pending);
				++m_nPendingInserts;
			}

			if (m_Pending.size() >= m_nPendingLimit)
				Materialize();
		}

		template<class TComp>
		void RemovePending(TComp& comp, const TKey& key)
		{
			auto pit = PendingLowerBound(comp, key);
			if (pit != m_Pending.end() && comp.EQ(pit->m_key, key))
			{
				if (pit->m_op == ePendingInsert)
				{
					m_Pending.erase(pit);
					--m_nPendingInserts;
				}
				else if (pit->m_op == ePendingReplace)
				{
					pit->m_op = ePendingDelete;
					++m_nPendingDeletes;
				}
				else
					throw CommonLib::CExcBase("Failed to remove: key not found");
			}
			else
			{
				auto bit = std::lower_bound(this->m_KeyMemSet.begin(), this->m_KeyMemSet.end(), key, comp);
				if (bit == this->m_KeyMemSet.end() || !comp.EQ(*bit, key))
					throw CommonLib::CExcBase("Failed to remove: key not found");

				uint32_t nPos = (uint32_t)std::distance(this->m_KeyMemSet.begin(), bit);
				SPending pending{ key, this->m_ValueMemSet[nPos], nPos, ePendingDelete };
				m_Pending.insert(pit, pending);
				++m_nPendingDeletes;
			}

			if (m_Pending.size() >= m_nPendingLimit)
				Materialize();
		}

		// the first key of the merged view without merging
		const TKey& FirstKey() const
		{
			uint32_t nBase = 0;
			for (const SPending& pending : m_Pending)
			{
				if (pending.m_nPos != nBase)
					break;                       // the vector element nBase is before this pending entry

				if (pending.m_op != ePendingDelete)
					return pending.m_key;        // insert before nBase or replace of nBase

				++nBase;                         // nBase is deleted
			}

			if (nBase < this->m_KeyMemSet.size())
				return this->m_KeyMemSet[nBase];

			throw CommonLib::CExcBase("Leaf node FirstKey: node is empty");
		}

	public:
		TValueMemSet m_ValueMemSet;

	private:
		TPendingSet m_Pending;
		uint32_t m_nPendingLimit{ 0 };
		uint32_t m_nPendingInserts{ 0 };
		uint32_t m_nPendingDeletes{ 0 };

		bool m_bCountMode{ false };      // param CountLimitMode
		bool m_bSplitAtFit{ false };     // param SplitAtFit
		bool m_bStatsDirty{ false };     // compressor statistics don't match the vectors
		uint32_t m_nCountLimit{ 0 };
		bool m_bLastInsertAppend{ false };
	};
}
