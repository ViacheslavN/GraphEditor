#pragma once
BPSETBASE_TEMPLATE_PARAMS
template<class TIterator, class TComparator>
TIterator BPSETBASE_DECLARATION::find(const TComparator& comp, const TKey& key, TIterator *pFromIterator, bool bFindNext)
{
	try
	{
		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eFindKey);

		if (pFromIterator)
		{
			TIterator it(this, pFromIterator->m_pCurNode, pFromIterator->m_pCurNode->binary_search(comp, key));
			if (!it.IsNull() || !bFindNext)
				return it;
		}
		
		if (!m_pRoot.get())
			LoadTree();

		if (!m_pRoot.get())
			return TIterator(this, TBPTreeNodePtr(), -1);


		if (m_pRoot->IsLeaf())
		{
			return TIterator(this, m_pRoot, m_pRoot->binary_search(comp, key));
		}

		int32_t nIndex = -1;
		int64_t nNextAddr = m_pRoot->inner_lower_bound(comp, key, nIndex);

		TBPTreeNodePtr pParent = m_pRoot;
		for (;;)
		{
			if (nNextAddr == -1)
			{
				break;
			}
			TBPTreeNodePtr pNode = GetNode(nNextAddr);
			if (!pNode.get())
			{
				return TIterator(this, TBPTreeNodePtr(), -1);
				break;
			}
//			pNode->SetParent(pParent, nIndex);

			if (pNode->IsLeaf())
			{
				return TIterator(this, pNode, pNode->binary_search(comp, key));
			}
			nNextAddr = pNode->inner_lower_bound(comp, key, nIndex);
			pParent = pNode;
		}

		return TIterator(this, TBPTreeNodePtr(), -1);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to find", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
template<class TIterator>
TIterator BPSETBASE_DECLARATION::begin()
{
	try
	{
		TBPTreeNodePtr pFindBTNode;
		if (!m_pRoot.get())
			LoadTree();

		if (!m_pRoot.get())
			return TIterator(this, pFindBTNode, -1);

		if (m_pRoot->IsLeaf())
			return TIterator(this, m_pRoot, m_pRoot->Count() ? 0 : -1);


		int64_t nNextAddr = m_pRoot->Less();

		TBPTreeNodePtr pParent = m_pRoot;
		for (;;)
		{
			if (nNextAddr == -1)
			{
				break;
			}
			TBPTreeNodePtr pNode = GetNode(nNextAddr);
//			pNode->SetParent(pParent, LESS_INDEX);

			if (pNode->IsLeaf())
			{
				pFindBTNode = pNode;
				break;
			}

			nNextAddr = pNode->Less();
			pParent = pNode;
		}

		return TIterator(this, pFindBTNode, 0);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("TBPlusTreeSetBase failed to begin", exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
template<class TIterator>
TIterator BPSETBASE_DECLARATION::last()
{
	try
	{
		TBPTreeNodePtr pFindBTNode;
		if (!m_pRoot.get())
			LoadTree();

		if (!m_pRoot.get())
			return TIterator(this, pFindBTNode, -1);

		if (m_pRoot->IsLeaf())
		{
			return TIterator(this, m_pRoot, m_pRoot->Count() - 1);
		}

		int64_t nNextAddr = m_pRoot->BackLink();
		int32_t nIndex = m_pRoot->Count() - 1;

		TBPTreeNodePtr pParent = m_pRoot;
		for (;;)
		{
			if (nNextAddr == -1)
			{
				break;
			}
			TBPTreeNodePtr pNode = GetNode(nNextAddr);

			if (pNode->IsLeaf())
			{
				pFindBTNode = pNode;
				break;
			}
			nNextAddr = pNode->BackLink();
			pParent = pNode;
			nIndex = pNode->Count() - 1;
		}

		return TIterator(this, pFindBTNode, pFindBTNode.get() ? pFindBTNode->Count() - 1 : -1);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("TBPlusTreeSetBase failed to begin", exc);
	}
}

BPSETBASE_TEMPLATE_PARAMS
template<class TIterator, class TComparator>
TIterator BPSETBASE_DECLARATION::upper_bound(const TComparator& comp, const TKey& key, TIterator *pFromIterator, bool bFindNext)
{
	try
	{
		return bound<TIterator>(key, [&comp](TBPTreeNodePtr& pNode, const TKey& k) { return pNode->leaf_upper_bound(comp, k); },
			[&comp](TBPTreeNodePtr& pNode, const TKey& k, int32_t& nIndex) { return pNode->inner_upper_bound(comp, k, nIndex); });
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("TBPlusTreeSetBase failed upper_bound", exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
template<class TIterator, class TComparator>
TIterator BPSETBASE_DECLARATION::lower_bound(const TComparator& comp, const TKey& key, TIterator *pFromIterator, bool bFindNext)
{
	try
	{
		if (pFromIterator)
		{
			if (pFromIterator->IsNull() && !bFindNext)
				return TIterator(this, TBPTreeNodePtr(), -1);
		}

		return bound<TIterator>(key, [&comp](TBPTreeNodePtr& pNode, const TKey& k) { return pNode->leaf_lower_bound(comp, k); },
			[&comp](TBPTreeNodePtr& pNode, const TKey& k, int32_t& nIndex) { return pNode->inner_lower_bound(comp, k, nIndex); });
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("TBPlusTreeSetBase failed lower_bound", exc);
		throw;
	}
}

// lower_bound / upper_bound: descends to the leaf of the key; if the key is above every key of that
// leaf, the result is the first key of the next leaf (leaves other than the root are never empty)
BPSETBASE_TEMPLATE_PARAMS
template<class TIterator, class TLeafBound, class TInnerBound>
TIterator BPSETBASE_DECLARATION::bound(const TKey& key, TLeafBound leafBound, TInnerBound innerBound)
{
	if (!m_pRoot.get())
		LoadTree();

	if (!m_pRoot.get())
		return TIterator(this, TBPTreeNodePtr(), -1);

	TBPTreeNodePtr pNode = m_pRoot;
	while (!pNode->IsLeaf())
	{
		int32_t nIndex = -1;
		int64_t nNextAddr = innerBound(pNode, key, nIndex);
		if (nNextAddr == -1)
			return TIterator(this, TBPTreeNodePtr(), -1);
		pNode = GetNode(nNextAddr);
	}

	int32_t nLeafIndex = leafBound(pNode, key);
	if (nLeafIndex >= 0 && nLeafIndex < (int32_t)pNode->Count())
		return TIterator(this, pNode, nLeafIndex);

	if (pNode->GetAddr() == m_nRootAddr || pNode->GetNext() == -1)
		return TIterator(this, TBPTreeNodePtr(), -1);

	TBPTreeNodePtr pNodeNext = GetNode(pNode->GetNext());
	if (pNodeNext->Count() == 0)
		return TIterator(this, TBPTreeNodePtr(), -1);
	return TIterator(this, pNodeNext, 0);
}
