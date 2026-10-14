#pragma once


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::insert(TKey& key)
{
	try
	{
		
		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eInsertValue);
		TBPTreeNodePtr pNode = findLeafNodeForInsert(key);

		pNode->InsertInLeaf(m_comp, key);
		pNode->SetFlags(CHANGE_NODE, true);

		CheckLeafNode(pNode);
		CheckCache();
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed insert", exc);
	}
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::findLeafNodeForInsert(const TKey& key)
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
		int64_t nNextAddr = m_pRoot->FindNodeInsert(m_comp, key, nIndex);
		TBPTreeNodePtr pParent = m_pRoot;
		while (nNextAddr != -1)
		{

			TBPTreeNodePtr pNode = GetNode(nNextAddr);
			if (pNode->IsLeaf())
			{
				return pNode;
			}

			pParent = pNode;		
			nNextAddr = pNode->FindNodeInsert(m_comp, key, nIndex);
		}

		throw CommonLib::CExcBase("can't find leaf node");
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed findLeafNodeForInsert", exc);
		throw;
	}
}
 

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::CheckLeafNode(TBPTreeNodePtr &pNode)
{
	try
	{
		if (!pNode->IsNeedSplit())
			return;

		if (pNode->GetAddr() == m_nRootAddr)
		{
			TransformRootToInner();
			return;
		}

		
		// min split: only for a key appended to the end of the leaf (ascending inserts) - the leaf stays full and
		// the new right leaf gets the new key. A key in the middle of a leaf splits it in halves: a min split there
		// would leave the leaf full and split it again at the next insert (a chain of almost empty leaves).
		bool bMinSplit = m_bMinSplit && pNode->LeafLastInsertAppend();
		uint32_t nSplitCount = bMinSplit ? 1 : pNode->LeafSplitCount();

		// A leaf of a cut compressor (zlib values: the size is a guess, the leaf grows over the page by it):
		// encoded once, the elements that fit are the left leaf - its page is written as it is - the rest
		// goes to the new right leaf. The other leaves compute their size: a full leaf splits off one element.
		bool bCut = false;
		if constexpr (TLeafCutWrite<_TLeafNode>::value)
		{
			if (bMinSplit && pNode->Count() <= pNode->LeafMaxCount())
			{
				nSplitCount = EncodeLeafCut(pNode);
				if (nSplitCount == 0)
					return;          // all fit: the guess was too small, the encoder has the ratio of the leaf now
				bCut = true;
			}
		}

		TBPTreeNodePtr pCheckNode = GetParentNode(pNode);
		TBPTreeNodePtr pNewRightLeafNode = NewNode(true, true);
		SplitLeafNode(pNode, pNewRightLeafNode, pCheckNode, nSplitCount);
		if (bCut)
			WriteLeafCut(pNode);

		while (pCheckNode->IsNeedSplit())
		{

			TBPTreeNodePtr pParentNode = GetParentNode(pCheckNode);
			if (!pParentNode.get())
			{
				SplitRootInnerNode();
				break;
			}

			TBPTreeNodePtr pNodeNewRight = NewNode(false, true);
			SplitInnerNode(pCheckNode, pNodeNewRight, pParentNode, bMinSplit ? 3 : (pCheckNode->Count() / 2) - 3);
			pCheckNode = pParentNode;

		}

	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed CheckLeafNode", exc);
	}
}


BPSETBASE_TEMPLATE_PARAMS
uint32_t BPSETBASE_DECLARATION::EncodeLeafCut(TBPTreeNodePtr &pNode)
{
	try
	{
		uint32_t nPageSize = m_cachePage->GetSize();
		m_cutBuf.assign(nPageSize, 0);
		CommonLib::CFxMemoryWriteStream stream;
		stream.AttachBuffer(m_cutBuf.data(), nPageSize);
		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eSaveLeafNode);
		return pNode->Save(&stream, &m_Context);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed EncodeLeafCut", exc);
		throw;
	}
}

// after the split: the page encoded by EncodeLeafCut has the elements the leaf has now, only its link to
// the next leaf changed
BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::WriteLeafCut(TBPTreeNodePtr &pNode)
{
	try
	{
		m_cachePage->Erase(0);
		m_cachePage->SetAddr(pNode->GetAddr());
		memcpy(m_cachePage->GetPayLoadData(), m_cutBuf.data(), (std::min)((size_t)m_cachePage->GetSize(), m_cutBuf.size()));

		CommonLib::CFxMemoryWriteStream stream;
		stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
		pNode->WriteLeafLinks(&stream);

		m_pStorage->SavePage(m_cachePage);
		pNode->SetFlags(CHANGE_NODE, false);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed WriteLeafCut", exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::TransformRootToInner()
{
	try
	{
		TBPTreeNodePtr pLeftNode = NewNode(true, true);
		TBPTreeNodePtr pRightNode = NewNode(true, true);

		TKey splitKey;
		int nSplitIndex = m_pRoot->SplitIn(pLeftNode, pRightNode, &splitKey);

		m_pRoot->Clear();
		m_pRoot->TransformToInner(m_pAllocsSet, m_nNodePageSize, m_pCompressParams);

		m_pRoot->SetLess(pLeftNode->GetAddr());
		int nInsertIndex = m_pRoot->InsertInInnerNode(m_comp, splitKey, pRightNode->GetAddr());

		/*pRightNode->SetParent(m_pRoot, nInsertIndex);
		pLeftNode->SetParent(m_pRoot, -1);*/

		pLeftNode->SetNext(pRightNode->GetAddr());
		pRightNode->SetPrev(pLeftNode->GetAddr());

		pLeftNode->SetFlags(CHANGE_NODE, true);
		pRightNode->SetFlags(CHANGE_NODE, true);
		m_pRoot->SetFlags(CHANGE_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed TransformRootToInner", exc);
	}
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::SplitLeafNode(TBPTreeNodePtr &pNode, TBPTreeNodePtr &pNewNode, TBPTreeNodePtr &pParentNode, int32_t nCount)
{
	try
	{
		if (!pNode->IsLeaf() || !pNewNode->IsLeaf())
			throw CommonLib::CExcBase("Node isn't a leaf");

		TKey splitKey;
		int nSplitIndex = pNode->SplitIn(pNewNode, nCount, &splitKey);
		int nInsertIndex = pParentNode->InsertInInnerNode(m_comp, splitKey, pNewNode->GetAddr());


		if (pNode->GetNext() != -1)
		{
			pNewNode->SetNext(pNode->GetNext());				
			TBPTreeNodePtr pNextNode = GetNode(pNode->GetNext());
			pNextNode->SetPrev(pNewNode->GetAddr());
			pNextNode->SetFlags(CHANGE_NODE, true);
		}

		pNode->SetNext(pNewNode->GetAddr());
		pNewNode->SetPrev(pNode->GetAddr());
				
		pNode->SetFlags(CHANGE_NODE, true);
		pNewNode->SetFlags(CHANGE_NODE, true);
		pParentNode->SetFlags(CHANGE_NODE, true);
		
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed SplitLeafNode", exc);
	}
}



BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::SplitRootInnerNode()
{
	try
	{
		TBPTreeNodePtr pLeftNode = NewNode(false, true);
		TBPTreeNodePtr pRightNode = NewNode(false, true);
		TKey nMedianKey;

		m_pRoot->SplitIn(pLeftNode, pRightNode, &nMedianKey);
		int nIndex = m_pRoot->InsertInInnerNode(m_comp, nMedianKey, pRightNode->GetAddr());
		m_pRoot->SetLess(pLeftNode->GetAddr());


		pLeftNode->SetFlags(CHANGE_NODE, true);
		pRightNode->SetFlags(CHANGE_NODE, true);
		m_pRoot->SetFlags(CHANGE_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed TransformRootToInner", exc);
	}

}



BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::SplitInnerNode(TBPTreeNodePtr&pNode, TBPTreeNodePtr& pNodeNewRight, TBPTreeNodePtr& pParentNode, int32_t nCount)
{
	try
	{
		if (!pParentNode.get())
		{
			throw CommonLib::CExcBase("Parent node is null");
		}
					   
		TKey nMedianKey;
		pNode->SplitIn(pNodeNewRight, nCount, &nMedianKey);
		int nIndex = pParentNode->InsertInInnerNode(m_comp, nMedianKey, pNodeNewRight->GetAddr());

		pParentNode->SetFlags(CHANGE_NODE, true);
		pNodeNewRight->SetFlags(CHANGE_NODE, true);
		pNode->SetFlags(CHANGE_NODE, true);

	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed SplitInnerNode", exc);
	}
}

