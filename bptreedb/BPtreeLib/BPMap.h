#pragma once

#include "BPBaseTreeSet.h"
#include "BPIteratorMap.h"
#include "BPTreeNodeMapHolder.h"
#include "BPTreeStatistics.h"


namespace bptreedb
{
	template <	class _TKey, class _TValue, class _TComp, class _TStorage,
		class _TInnerNode,
		class _TLeafNode,
		class _TNodeHolder = TBPMapNodeHolder<_TInnerNode, _TLeafNode>
	>
	
	class TBPMap : public TBPlusTreeSetBase<_TKey, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder>
	{
	public:

		typedef TBPlusTreeSetBase<_TKey, _TComp, _TStorage, _TInnerNode, _TLeafNode, _TNodeHolder> TBase;
		typedef _TValue TValue;
		typedef typename TBase::TKey  TKey;
		typedef typename TBase::TComp	   TComp;
		typedef typename TBase::TLink  TLink;
		typedef typename TBase::TStorage  TStorage;
		typedef typename TBase::TInnerNode TInnerNode;
		typedef typename TBase::TLeafNode	TLeafNode;
		typedef typename TBase::TInnerCompess TInnerCompess;
		typedef typename TBase::TLeafCompess TLeafCompess;
		typedef typename TBase::TBPTreeNode TBPTreeNode;
		typedef typename TBase::TBPTreeNodePtr TBPTreeNodePtr;


		template <class _TK, class _TV, class _TNode, class _TBTree>
		friend class TBPMapIterator;

		typedef TBPMapIterator<TKey, TValue, TBPTreeNode, TBase> iterator;

		template <class _TKey2, class _TNodeHolder2, class _TBTree>
		friend class TBPTreeStatistics;

		typedef TBPTreeStatistics<TKey, TBPTreeNode, TBase> TreeStatistics;
		typedef std::shared_ptr< TreeStatistics> TreeStatisticsPtr;



		TBPMap(int64_t nPageBTreeInfo, std::shared_ptr<TStorage> pStorage, CommonLib::IAllocPtr pAlloc, uint32_t nChacheSize, uint32_t nNodesPageSize, bool bMinSplit = false) :
			TBase(nPageBTreeInfo, pStorage, pAlloc, nChacheSize, nNodesPageSize, bMinSplit)
		{

		}

		void insert(const TKey& key, const TValue& value)
		{
			try
			{
				CommonLib::CPrefCounterHolder holder(this->m_pBPTreePerfCounter, eInsertValue);

				TBPTreeNodePtr pNode = this->findLeafNodeForInsert(key);
				pNode->InsertInLeaf(this->m_comp, key, value);
				pNode->SetFlags(CHANGE_NODE, true);

				this->CheckLeafNode(pNode);
				this->CheckCache();
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed insert", exc);
			}
		}

		virtual TBPTreeNodePtr AllocateNewNode( int64_t nAddr, bool bLeaf)
		{
			try
			{
				
				TBPTreeNodePtr pNode(new TBPTreeNode(this->m_pAllocsSet, this->GetNodePageSize(), bLeaf, nAddr, this->m_pCompressParams));
				return pNode;

			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("TBPNodeHolder failed to load ", exc);
				throw;
			}

		}
		
		iterator begin()
		{
			return TBase::template begin<iterator>();
		}

		iterator find(const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template find<iterator, TComp>(this->m_comp, key, pFromIterator, bFindNext);
		}

		template<class _TCustComp>
		iterator find(_TCustComp& comp, const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template find<iterator, _TCustComp>(comp, key, pFromIterator, bFindNext);
		}

		iterator upper_bound(const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template upper_bound<iterator>(this->m_comp, key, pFromIterator, bFindNext);
		}
		iterator lower_bound(const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template lower_bound<iterator>(this->m_comp, key, pFromIterator, bFindNext);
		}

		template<class _Comp>
		iterator upper_bound(const _Comp& comp, const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template upper_bound<iterator, _Comp>(comp, key, pFromIterator, bFindNext);
		}

		template<class _Comp>
		iterator lower_bound(const _Comp& comp, const TKey& key, iterator *pFromIterator = NULL, bool bFindNext = true)
		{
			return TBase::template lower_bound<iterator, _Comp>(comp, key, pFromIterator, bFindNext);
		}

		TreeStatisticsPtr GetStatistics()
		{
			return TreeStatisticsPtr(new TreeStatistics(this));
		}

		// Builds the tree bottom-up from keys in ascending order (no duplicates). The tree must be empty.
		// Leaves are filled until the compressor estimate says the next element doesn't fit, then written;
		// if the real encoding still doesn't fit, the last elements move to the next leaf. Inner levels
		// are built the same way. No splits, no element moves, pages are filled up to the page size.
		// TSource: bool Next(TKey& key, TValue& value)
		template<class TSource>
		uint64_t BulkLoad(TSource& source)
		{
			try
			{
				if (this->m_pRoot.get() == nullptr)
					this->LoadTree();

				if (!this->m_pRoot->IsLeaf() || this->m_pRoot->Count() != 0)
					throw CommonLib::CExcBase("BulkLoad: the tree isn't empty");

				typedef std::pair<TKey, TLink> TLevelEntry;          // first key, node address
				std::vector<TLevelEntry> level;

				uint64_t nCount = 0;
				TBPTreeNodePtr pLeaf = this->NewNode(true, false);
				std::vector<std::pair<TKey, TValue>> carry;

				// writes pNode; the elements that don't fit after all go to carry (ascending).
				// A leaf of a cut compressor (NodeCompressorTraits.h) is filled over the page by its estimate: the
				// encoding says exactly which elements don't fit, the page of the others is written as it is.
				auto saveLeaf = [&](TBPTreeNodePtr& pNode)
				{
					uint32_t nRet = 0;
					while ((nRet = this->TrySaveNode(pNode)) != 0)
					{
						uint32_t nNodeCount = pNode->Count();
						if (nNodeCount < 2)
							throw CommonLib::CExcBase("BulkLoad: one element doesn't fit in a leaf");

						constexpr bool bCut = TLeafCutWrite<TLeafNode>::value;
						uint32_t nMove = bCut ? nRet : std::max<uint32_t>(1, nNodeCount / 64);
						if (nMove >= nNodeCount)
							throw CommonLib::CExcBase("BulkLoad: wrong count {0} of the elements that don't fit, the leaf has {1}", nMove, nNodeCount);
						std::vector<std::pair<TKey, TValue>> moved;
						for (uint32_t i = nNodeCount - nMove; i < nNodeCount; ++i)
							moved.emplace_back(pNode->Key(i), pNode->Value(i));
						for (uint32_t i = 0; i < nMove; ++i)
							pNode->RemoveByIndex(pNode->Count() - 1);

						carry.insert(carry.begin(), moved.begin(), moved.end());

						if constexpr (bCut)
						{
							// the cache page has the encoded page of the elements the leaf has now
							this->m_pStorage->SavePage(this->m_cachePage);
							pNode->SetFlags(CHANGE_NODE, false);
							break;
						}
					}
				};

				auto closeLeaf = [&]()
				{
					TBPTreeNodePtr pNext = this->NewNode(true, false);
					pLeaf->SetNext(pNext->GetAddr());
					pNext->SetPrev(pLeaf->GetAddr());

					carry.clear();
					saveLeaf(pLeaf);
					level.emplace_back(pLeaf->Key(0), pLeaf->GetAddr());

					pLeaf = pNext;
					for (auto& kv : carry)
						pLeaf->AppendInLeaf(kv.first, kv.second);
				};

				TKey key;
				TValue value;
				while (source.Next(key, value))
				{
					pLeaf->AppendInLeaf(key, value);
					if (pLeaf->IsNeedSplit())
					{
						pLeaf->RemoveByIndex(pLeaf->Count() - 1);
						closeLeaf();
						pLeaf->AppendInLeaf(key, value);
					}
					++nCount;
				}

				if (level.empty())
				{
					// everything fits in one leaf: it is the root (fixed address)
					for (uint32_t i = 0, sz = pLeaf->Count(); i < sz; ++i)
						this->m_pRoot->AppendInLeaf(pLeaf->Key(i), pLeaf->Value(i));
					this->m_pStorage->DropFilePage(pLeaf->GetAddr());
					this->m_pRoot->SetFlags(CHANGE_NODE, true);
					this->Flush();
					return nCount;
				}

				// the last leaf (and more leaves if it didn't fit after all)
				for (;;)
				{
					carry.clear();
					saveLeaf(pLeaf);
					level.emplace_back(pLeaf->Key(0), pLeaf->GetAddr());
					if (carry.empty())
						break;

					TBPTreeNodePtr pNext = this->NewNode(true, false);
					pLeaf->SetNext(pNext->GetAddr());
					pNext->SetPrev(pLeaf->GetAddr());
					if constexpr (TLeafCutWrite<TLeafNode>::value)
					{
						// the cache page has the page of the leaf (saveLeaf): only the link changes (encoding the
						// leaf again could cut it at another element)
						CommonLib::CFxMemoryWriteStream stream;
						stream.AttachBuffer(this->m_cachePage->GetPayLoadData(), this->m_cachePage->GetSize());
						pLeaf->WriteLeafLinks(&stream);
						this->m_pStorage->SavePage(this->m_cachePage);
						pLeaf->SetFlags(CHANGE_NODE, false);
					}
					else
						this->TrySaveNode(pLeaf);       // rewrite with the new next link (same content, fits)
					pLeaf = pNext;
					for (auto& kv : carry)
						pLeaf->AppendInLeaf(kv.first, kv.second);
				}

				// inner levels
				for (;;)
				{
					if (FitsInOneInnerNode(level))
					{
						this->m_pRoot->Clear();
						this->m_pRoot->TransformToInner(this->m_pAllocsSet, this->m_nNodePageSize, this->m_pCompressParams);
						this->m_pRoot->SetLess(level[0].second);
						for (size_t i = 1; i < level.size(); ++i)
							this->m_pRoot->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
						this->m_pRoot->SetFlags(CHANGE_NODE, true);
						break;
					}

					std::vector<TLevelEntry> upper;
					TBPTreeNodePtr pPrevInner;
					size_t i = 0;
					while (i < level.size())
					{
						TBPTreeNodePtr pInner = this->NewNode(false, false);
						TKey firstKey = level[i].first;
						pInner->SetLess(level[i].second);
						size_t nStart = ++i;

						while (i < level.size())
						{
							pInner->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
							if (pInner->IsNeedSplit())
							{
								pInner->RemoveByIndex(pInner->Count() - 1);
								break;
							}
							++i;
						}

						while (this->TrySaveNode(pInner) != 0)
						{
							if (i <= nStart + 1)
								throw CommonLib::CExcBase("BulkLoad: inner node doesn't fit");
							pInner->RemoveByIndex(pInner->Count() - 1);
							--i;
						}

						if (pInner->Count() == 0)
						{
							// the last group got a single child: an inner node without keys is not allowed.
							// Give it the last child of the previous node (or move the child there).
							if (pPrevInner.get() == nullptr)
								throw CommonLib::CExcBase("BulkLoad: inner node without keys");
							TBPTreeNodePtr& pPrev = pPrevInner;
							if (pPrev->Count() >= 2)
							{
								uint32_t nLast = pPrev->Count() - 1;
								TKey lastKey = pPrev->Key(nLast);
								TLink lastLink = pPrev->Link(nLast);
								pPrev->RemoveByIndex(nLast);
								pInner->InsertInInnerNode(this->m_comp, firstKey, pInner->Less());
								pInner->SetLess(lastLink);
								firstKey = lastKey;
								if (this->TrySaveNode(pPrev) != 0 || this->TrySaveNode(pInner) != 0)
									throw CommonLib::CExcBase("BulkLoad: failed to rebalance the last inner node");
							}
							else
							{
								pPrev->InsertInInnerNode(this->m_comp, firstKey, pInner->Less());
								if (this->TrySaveNode(pPrev) != 0)
									throw CommonLib::CExcBase("BulkLoad: failed to merge the last inner node");
								this->m_pStorage->DropFilePage(pInner->GetAddr());
								break;   // pPrev is already in upper
							}
						}

						upper.emplace_back(firstKey, pInner->GetAddr());
						pPrevInner = pInner;
					}
					level.swap(upper);
				}

				this->Flush();
				return nCount;
			}
			catch (std::exception& exc)
			{
				CommonLib::CExcBase::RegenExcT("[TBPMap] BulkLoad failed", exc);
				throw;
			}
		}

	private:

		template<class TLevel>
		bool FitsInOneInnerNode(const TLevel& level)
		{
			TBPTreeNodePtr pTest = this->AllocateNewNode(-1, false);
			pTest->SetLess(level[0].second);
			for (size_t i = 1; i < level.size(); ++i)
			{
				pTest->InsertInInnerNode(this->m_comp, level[i].first, level[i].second);
				if (pTest->IsNeedSplit())
					return false;
			}
			return true;
		}

	private:

	

	};
}
