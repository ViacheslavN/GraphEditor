#pragma once


BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::FindLeafNodeForDelete(const TKey& key)
{
	try
	{

		if (m_pRoot.get() == nullptr)
			LoadTree();

		if (m_pRoot->IsLeaf())
		{
			return m_pRoot;
		}

		int32_t nIndex = -1;
		int64_t nNextAddr = m_pRoot->FindNodeRemove(m_comp, key, nIndex);
		TBPTreeNodePtr pParent = m_pRoot;
		while (nNextAddr != -1)
		{

			TBPTreeNodePtr pNode = GetNode(nNextAddr);
			if (pNode->IsLeaf())
			{
				return pNode;
			}

			pParent = pNode;
			nNextAddr = pNode->FindNodeRemove(m_comp, key, nIndex);
		}

		return TBPTreeNodePtr();
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed FindLeafNodeForDelete", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
bool BPSETBASE_DECLARATION::remove(const TKey& key)
{
	try
	{
		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eRemoveValue);

		TBPTreeNodePtr pNode = FindLeafNodeForDelete(key);
		if (!pNode.get())
			return false;

		if(!pNode->IsLeaf())
			throw CommonLib::CExcBase("Node {0} isn't leaf", pNode->GetAddr());

		int32_t nIndex = pNode->binary_search(m_comp, key);

		if (nIndex == -1)
			return false;


		RemoveFromLeafNode(pNode, nIndex, key);
		this->CheckCache();

	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed remove", exc);
		throw;
	}

	return true;
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::RemoveFromLeafNode(TBPTreeNodePtr pNode, int32_t nIndex, const TKey& key)
{
	try
	{

		int32_t nFoundIndex;
		TBPTreeNodePtr pParentNode = GetParentNode(pNode, &nFoundIndex);

		pNode->RemoveLeafKey(m_comp, nIndex, key);
		pNode->SetFlags(CHANGE_NODE, true);

		if (pNode->GetAddr() == m_nRootAddr || !pParentNode.get())
			return;

		if (nFoundIndex != LESS_INDEX)
		{
			if (pNode->Count() == 0)
			{
				pParentNode->RemoveByIndex(nFoundIndex);
				pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
			}
			else if (nIndex == 0)
			{
				pParentNode->UpdateKey(nFoundIndex, pNode->Key(0));
				pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
			}

		}
		else
		{
			if (pNode->Count() == 0)
			{
				if (pParentNode->Count() > 0)
				{
					TLink link = pParentNode->Link(0);
					pParentNode->SetLess(link);
					pParentNode->RemoveByIndex(0);
									   
					pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
				}
				else
				{
					// the leaf was the parent's only child: the parent has no children now,
					// RemoveFromInnerNode removes it from its own parent (or empties the root)
					pParentNode->SetLess(EMPTY_PAGE_ADDR);
					pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
				}
			}
		}

	 

		if (!pNode->IsHalfEmpty())
		{
			return 	RemoveFromInnerNode(pParentNode, key);
		}


		if (pNode->Count() == 0)
		{
			TBPTreeNodePtr pNextNode = GetNode(pNode->GetNext());
			TBPTreeNodePtr pPrevNode = GetNode(pNode->GetPrev());
			 
			if (pNextNode.get() != nullptr)
			{
				pNextNode->SetPrev(pNode->GetPrev());
				pNextNode->SetFlags(CHANGE_NODE, true);
			}

			if (pPrevNode.get() != nullptr)
			{
				pPrevNode->SetNext(pNode->GetNext());
				pPrevNode->SetFlags(CHANGE_NODE, true);
			}

			DeleteNode(pNode);   // unlinked from the parent and the leaf chain: free the page (DropNode saved it)
			return 	RemoveFromInnerNode(pParentNode, key);
		}


		TBPTreeNodePtr pDonorNode;
		bool bLeft = false; //The position of the donor node relative to the node
		bool bUnion = false;
		bool bAlignment = false;
		int32_t nDonorFoundIndex = 0;

		if (pParentNode->Less() == pNode->GetAddr())
		{
			if (pParentNode->Count() == 0)     // the only child: no sibling to merge with
				return RemoveFromInnerNode(pParentNode, key);

			pDonorNode = GetNode(pParentNode->Link(0));
			bUnion = pNode->PossibleUnion(pDonorNode);
			if (!bUnion)
				bAlignment = pNode->PossibleAlignment(pDonorNode);
		}
		else
		{

			TBPTreeNodePtr pLeafNodeRight;
			int32_t nRightFoundIndex = 0;

			TBPTreeNodePtr pLeafNodeLeft;
			int32_t nLeftFoundIndex = 0;

			if (nFoundIndex == 0)
			{
				pLeafNodeLeft = GetNode(pParentNode->Less());
				nLeftFoundIndex = -1;
				if (pParentNode->Count() > 1)
				{
					pLeafNodeRight = GetNode(pParentNode->Link(1));
					nRightFoundIndex = 1;
				}
			}
			else
			{
				pLeafNodeLeft = GetNode(pParentNode->Link(nFoundIndex - 1));
				nLeftFoundIndex = nFoundIndex - 1;
				if ((int32_t)pParentNode->Count() > nFoundIndex + 1)
				{
					pLeafNodeRight = GetNode(pParentNode->Link(nFoundIndex + 1));
					nRightFoundIndex = nFoundIndex + 1;
				}
			}

			if (pLeafNodeLeft) // left is preferable
			{
				bUnion = pNode->PossibleUnion(pLeafNodeLeft);
				if (bUnion)
				{
					pDonorNode = pLeafNodeLeft;
					nDonorFoundIndex = nLeftFoundIndex;
					bLeft = true;
				}
				else
				{
					bAlignment = pNode->PossibleAlignment(pLeafNodeLeft);
					pDonorNode = pLeafNodeLeft;
					nDonorFoundIndex = nLeftFoundIndex;
					bLeft = true;
				}
			}

			if (pDonorNode.get() == nullptr && pLeafNodeRight.get() != nullptr)
			{
				bUnion = pNode->PossibleUnion(pLeafNodeRight);
				if (bUnion)
				{
					pDonorNode = pLeafNodeRight;
					nDonorFoundIndex = nRightFoundIndex;
					bLeft = false;
				}
				else
				{
					bAlignment = pNode->PossibleAlignment(pLeafNodeRight);
					pDonorNode = pLeafNodeRight;
					nDonorFoundIndex = nRightFoundIndex;
					bLeft = false;
				}
			}
		}


		TBPTreeNodePtr pResultNode = pNode;
		if (bUnion)
		{

			if (nDonorFoundIndex == -1 || bLeft)
			{
				UnionLeafNode(pParentNode, pDonorNode, pNode, nFoundIndex);
			}
			else
			{
				UnionLeafNode(pParentNode, pNode, pDonorNode, nDonorFoundIndex);
			}

		}
		else if (bAlignment)
		{
			AlignmentLeafNode(pParentNode, pNode, nFoundIndex, pDonorNode, nDonorFoundIndex, bLeft);
		}

	
		RemoveFromInnerNode(pParentNode, key);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed remove from leaf node", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::UnionLeafNode(TBPTreeNodePtr pParentNode, TBPTreeNodePtr pLeafNode, TBPTreeNodePtr pDonorNode, int32_t nDonorFoundIndex)
{
	try
	{
		pLeafNode->UnionWithLeafNode(pDonorNode, false, nullptr);
		pLeafNode->SetFlags(CHANGE_NODE, true);
		TBPTreeNodePtr pNextNode = GetNode(pDonorNode->GetNext());
		if (pNextNode.get())
		{
			pLeafNode->SetNext(pNextNode->GetAddr());
			pNextNode->SetPrev(pLeafNode->GetAddr());
			pNextNode->SetFlags(CHANGE_NODE, true);
		}
		else
			pLeafNode->SetNext(EMPTY_PAGE_ADDR);

		pParentNode->RemoveByIndex(nDonorFoundIndex);				

		DeleteNode(pDonorNode);
		pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to union from leaf nodes", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::DeleteNode(TBPTreeNodePtr pNode)
{
	m_pStorage->DropFilePage(pNode->GetAddr());
	pNode->SetFlags(REMOVE_NODE, true);
	m_NodeCache.Remove(pNode->GetAddr());
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::AlignmentLeafNode(TBPTreeNodePtr pParentNode, TBPTreeNodePtr pLeafNode, int32_t nFoundIndex, TBPTreeNodePtr pDonorNode, int32_t nDonorIndex, bool bLeft)
{
	try
	{
		if (!pLeafNode->AlignmenLeftNodeOf(pDonorNode, bLeft))
			return;

		if (bLeft)
			pParentNode->UpdateKey(nFoundIndex, pLeafNode->Key(0));
		else
			pParentNode->UpdateKey(nDonorIndex, pDonorNode->Key(0));

		pLeafNode->SetFlags(CHANGE_NODE, true);
		pDonorNode->SetFlags(CHANGE_NODE, true);
		pParentNode->SetFlags(CHANGE_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to alignment leaf nodes", exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::RemoveFromInnerNode(TBPTreeNodePtr pCheckNode, const TKey& key)
{
	try
	{
		while (pCheckNode.get())
		{
			int32_t nFoundIndex = 0;
			TBPTreeNodePtr  pParentNode = pCheckNode->Count() != 0 ? GetParentNode(pCheckNode, &nFoundIndex) : GetParentNodeByKey(pCheckNode, key, &nFoundIndex);
			 
			if (!pParentNode.get())
			{
				if (!pCheckNode->Count() && pCheckNode->Less() == EMPTY_PAGE_ADDR)
				{
					// the last child is gone: the tree is empty, the root becomes an empty leaf
					pCheckNode->Clear();
					pCheckNode->TransformToLeaf(m_pAllocsSet, m_bMulti, m_nNodePageSize, m_pCompressParams);
					pCheckNode->SetFlags(ROOT_NODE | CHANGE_NODE, true);
				}
				else if (!pCheckNode->Count())
				{
					TBPTreeNodePtr pNode = GetNode(pCheckNode->Less());

					if (pNode->IsLeaf())
					{
						pCheckNode->Clear();
						pCheckNode->TransformToLeaf(m_pAllocsSet, m_bMulti, m_nNodePageSize, m_pCompressParams);
						pCheckNode->UnionWithLeafNode(pNode, false);
						pCheckNode->SetFlags(ROOT_NODE | CHANGE_NODE, true);
					}
					else
					{
						pCheckNode->SetLess(EMPTY_PAGE_ADDR);
						pCheckNode->UnionWithInnerNode(pNode, NULL, true);
						pCheckNode->SetFlags(CHANGE_NODE, true);
					}
					m_NodeCache.Remove(pNode->GetAddr());
					DeleteNode(pNode);
				}
				return;
			}


			if (nFoundIndex != -1 && pParentNode->IsKey(m_comp, key, nFoundIndex))
			{
				TBPTreeNodePtr pIndexNode = GetNode(pParentNode->Link(nFoundIndex));
				TBPTreeNodePtr pMinNode = GetMinimumNode(pIndexNode);
				if (pMinNode.get() && pMinNode->Count() != 0)   // an empty subtree is removed below
				{
					pParentNode->UpdateKey(nFoundIndex, pMinNode->Key(0));
					pParentNode->SetFlags(CHANGE_NODE, true);
				}
 
			}
			if (!(pCheckNode->GetFlags() & CHECK_REM_NODE))
			{
				pCheckNode = pParentNode;   // GetParentNode(pCheckNode) needs Key(0), pCheckNode may have no keys
				continue;
			}
			pCheckNode->SetFlags(CHECK_REM_NODE, false);

			if (!pCheckNode->IsHalfEmpty())
			{
				pCheckNode = pParentNode;//GetParentNode(pCheckNode);
				continue;
			}

			if (pCheckNode->Count() == 0 && pCheckNode->Less() == EMPTY_PAGE_ADDR)
			{
				{
					// no children at all: remove the node from its parent
					if (nFoundIndex == LESS_INDEX)
					{
						if (pParentNode->Count() > 0)
						{
							pParentNode->SetLess(pParentNode->Link(0));
							pParentNode->RemoveByIndex(0);
						}
						else
							pParentNode->SetLess(EMPTY_PAGE_ADDR);
					}
					else
						pParentNode->RemoveByIndex(nFoundIndex);

					pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
				}
				DeleteNode(pCheckNode);

				pCheckNode = pParentNode;
				continue;
			}

			// A node with one child left (Count() == 0, only Less) must not be replaced by that child in the
			// parent: the subtree would move a level up (leaves at different depths). It merges with or
			// borrows from a sibling below like any half empty node; only the root may lose a level.


			TBPTreeNodePtr pDonorNode;
			bool bLeft = false; //The position of the donor node relative to the node
			bool bUnion = false;
			bool bAlignment = false;
			int32_t nDonorFoundIndex = 0;

			if (pParentNode->Less() == pCheckNode->GetAddr())
			{
				if (pParentNode->Count() != 0)     // otherwise no sibling: pDonorNode stays null
					pDonorNode = GetNode(pParentNode->Link(0));
			}
			else
			{

				TBPTreeNodePtr pLeafNodeRight;
				TBPTreeNodePtr pLeafNodeLeft;
				int32_t nLeftFoundIndex = 0;
				int32_t nRightFoundIndex = 0;

				if (nFoundIndex == 0)
				{
					pLeafNodeLeft = GetNode(pParentNode->Less());
					nLeftFoundIndex = -1;
					if (pParentNode->Count() > 1)
					{
						pLeafNodeRight = GetNode(pParentNode->Link(1));
						nRightFoundIndex = 1;
					}
				}
				else
				{
					pLeafNodeLeft = GetNode(pParentNode->Link(nFoundIndex - 1));
					nLeftFoundIndex = nFoundIndex - 1;

					if ((int32_t)pParentNode->Count() > nFoundIndex + 1)
					{
						pLeafNodeRight = GetNode(pParentNode->Link(nFoundIndex + 1));
						nRightFoundIndex = nFoundIndex + 1;
					}
				}
 

				uint32_t nLeftCount = pLeafNodeLeft.get() ? pLeafNodeLeft->Count() : 0;
				uint32_t nRightCount = pLeafNodeRight.get() ? pLeafNodeRight->Count() : 0;
				if (nLeftCount < nRightCount || pLeafNodeRight.get() == nullptr)
				{
					pDonorNode = pLeafNodeLeft;
					bLeft = true;
					nDonorFoundIndex = nLeftFoundIndex;
				}
				else
				{
					pDonorNode = pLeafNodeRight;
					bLeft = false;
					nDonorFoundIndex = nRightFoundIndex;
				}
			}

			if (pDonorNode.get() == nullptr)
			{
				pCheckNode = pParentNode;
				continue;
			}

			bUnion = pCheckNode->PossibleUnion(pDonorNode);
			if (!bUnion && !m_bMinSplit)
			{
				bAlignment = pCheckNode->PossibleAlignment(pDonorNode);
			}

			if (bUnion)
			{
				if (pDonorNode->GetAddr() == pParentNode->Less() || bLeft)
					UnionInnerNode(pParentNode, pDonorNode, pCheckNode, nFoundIndex);
				else
					UnionInnerNode(pParentNode, pCheckNode, pDonorNode, nDonorFoundIndex);
			}
			else if (bAlignment)
			{
				AlignmentInnerNode(pParentNode, pCheckNode, nFoundIndex, pDonorNode, nDonorFoundIndex, bLeft);
			}

			pCheckNode = pParentNode;
		}

	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to remove from inner node", exc);
		throw;
	}
}



BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::UnionInnerNode(TBPTreeNodePtr pParentNode, TBPTreeNodePtr pNode, TBPTreeNodePtr pDonorNode, int32_t nDonorFoundIndex)
{
	try
	{
		TBPTreeNodePtr pMinNode = GetMinimumNode(GetNode(pDonorNode->Less()));
		pNode->UnionWithInnerNode(pDonorNode, &pMinNode->Key(0), false);
		pNode->SetFlags(CHANGE_NODE, true);		
		pParentNode->RemoveByIndex(nDonorFoundIndex);
		DeleteNode(pDonorNode);

		pParentNode->SetFlags(CHANGE_NODE | CHECK_REM_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to union inners node", exc);
		throw;
	}
}



BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::AlignmentInnerNode(TBPTreeNodePtr pParentNode, TBPTreeNodePtr pNode, int32_t nFoundIndex, TBPTreeNodePtr pDonorNode, int32_t nDonorFoundIndex, bool bLeft)
{

	TBPTreeNodePtr pMinNode = bLeft ? GetMinimumNode(GetNode(pNode->Less())) : GetMinimumNode(GetNode(pDonorNode->Less()));
	if (!pNode->AlignmentInnerNodeOf(pDonorNode, pMinNode->Key(0), bLeft))
		return;

	if (!bLeft) // Node donor is on the right
	{
		pMinNode = GetMinimumNode(GetNode(pDonorNode->Less()));
		pParentNode->UpdateKey(nDonorFoundIndex, pMinNode->Key(0));
		pParentNode->SetFlags(CHANGE_NODE, true);
	}
	else
	{
		pMinNode = GetMinimumNode(GetNode(pNode->Less()));
		pParentNode->UpdateKey(nFoundIndex, pMinNode->Key(0));
		pParentNode->SetFlags(CHANGE_NODE, true);
	}

	pParentNode->SetFlags(CHANGE_NODE, true);
	pNode->SetFlags(CHANGE_NODE, true);
	pDonorNode->SetFlags(CHANGE_NODE, true);
}
