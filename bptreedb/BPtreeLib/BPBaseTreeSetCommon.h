#pragma once


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::AddToCache(TBPTreeNodePtr pNode)
{
	m_NodeCache.AddElem(pNode->GetAddr(), pNode);
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::GetNodeFromCache(int64_t nAddr, bool bNotMove)
{
	CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eGetNodeFromCache);
	TBPTreeNodePtr pNode = m_NodeCache.GetElem(nAddr, bNotMove);
	if (pNode.get() != nullptr)
		return pNode;

	return TBPTreeNodePtr();
}


BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::GetNode(int64_t nAddr)
{
	 try
	 {

		 CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eGetNode);

		 if (nAddr == -1)
			 return TBPTreeNodePtr();

		 TBPTreeNodePtr pNode = GetNodeFromCache(nAddr);
		 if (pNode.get() == nullptr)
		 {
			 pNode = LoadNodeFromStorage(nAddr);
			 CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eAddToCache);
			 AddToCache(pNode);
		 }

		 return pNode;
	 }
	 catch (std::exception& exc)
	 {
		 CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to get node addr {0}", nAddr, exc);
		 throw;
	 }
}


BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::LoadNodeFromStorage(int64_t nAddr)
{
	try
	{

		m_pStorage->ReadPage(m_cachePage, nAddr);
		CommonLib::CReadMemoryStream stream;
		stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());

		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eLoadNode);
		TBPTreeNodePtr node = LoadNode<TBPTreeNode>(&stream, m_pAllocsSet, GetNodePageSize(), nAddr, m_pCompressParams, &m_Context);

		return node;
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to load node from storage addr {0}", nAddr, exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::GetParentNode(TBPTreeNodePtr pNode, int32_t* nElementPos)
{
	try
	{

		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eGetParentNode);


		if (pNode->GetAddr() == m_nRootAddr)
			return TBPTreeNodePtr();

		const TKey& key = pNode->Key(0);
		int32_t nIndex = -1;
		int64_t nNextAddr = m_pRoot->inner_lower_bound(m_comp, key, nIndex);
		TBPTreeNodePtr pParent = m_pRoot;

		for (;;)
		{
			if (pNode->GetAddr() == nNextAddr)
			{
				if (nElementPos)
					*nElementPos = nIndex;

				return pParent;
			}

			TBPTreeNodePtr pNode = GetNode(nNextAddr);
			if (!pNode.get())
			{
				throw CommonLib::CExcBase("FindParent failed to find parent for node addr {0}", pNode->GetAddr());
			}
			
			nNextAddr = pNode->inner_lower_bound(m_comp, key, nIndex);
			pParent = pNode;
		}

		throw CommonLib::CExcBase("FindParent failed to find parent for node addr {0}", pNode->GetAddr());
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] GetParentNode failed to find parent node", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::GetParentNodeByKey(TBPTreeNodePtr pNode, const TKey& key, int32_t* nElementPos)
{
	try
	{

		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eGetParentNode);


		if (pNode->GetAddr() == m_nRootAddr)
			return TBPTreeNodePtr();
		 
		int32_t nIndex = -1;
		int64_t nNextAddr = m_pRoot->inner_lower_bound(m_comp, key, nIndex);
		TBPTreeNodePtr pParent = m_pRoot;

		for (;;)
		{
			if (pNode->GetAddr() == nNextAddr)
			{
				if (nElementPos)
					*nElementPos = nIndex;

				return pParent;
			}

			TBPTreeNodePtr pNode = GetNode(nNextAddr);
			if (!pNode.get())
			{
				throw CommonLib::CExcBase("FindParent failed to find parent for node addr {0}", pNode->GetAddr());
			}

			nNextAddr = pNode->inner_lower_bound(m_comp, key, nIndex);
			pParent = pNode;
		}

		throw CommonLib::CExcBase("FindParent failed to find parent for node addr {0}", pNode->GetAddr());
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] GetParentNode by key ailed to find parent node", exc);
		throw;
	}
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::DropNode(TBPTreeNodePtr& pNode)
{
	try
	{
		uint32_t nFlags = pNode->GetFlags();
		if (nFlags & REMOVE_NODE)
		{
			m_pStorage->DropFilePage(pNode->GetAddr());
			return;
		}

		if (nFlags & CHANGE_NODE)
		{
			m_bLockRemoveItemFromCache = true; // TO DO use RAII

			SaveNode(pNode);

			m_bLockRemoveItemFromCache = false; // TO DO use RAII
		}

	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to drop node addr", exc);
	}
}

BPSETBASE_TEMPLATE_PARAMS
bool BPSETBASE_DECLARATION::IsTreeInit()
{
	try
	{
		if (m_nPageBTreeInfo == -1)
			return false;

		storage::IFilePagePtr pPage = m_pStorage->ReadPage(m_nPageBTreeInfo);

		return true;

	}
	catch (std::exception& exc)
	{
		exc;
		 //TO DO log
	}

	return false;
}


BPSETBASE_TEMPLATE_PARAMS
void  BPSETBASE_DECLARATION::InnitTree(TCompressorParamsBasePtr pParams, bool bMinSplit, uint32_t objectID, storage::ObjectPageType objecttype, uint32_t parentID, storage::ObjectPageType parenttype)
{
	try
	{
		if (IsTreeInit())
			throw CommonLib::CExcBase("BTree has been created already");

		m_bMinSplit = bMinSplit;
		if (m_nPageBTreeInfo == -1)
			m_nPageBTreeInfo = m_pStorage->GetNewFilePageAddr();

		storage::TWriteStreamPage stream(m_pStorage);
		stream.Open(m_nPageBTreeInfo, true);

		m_nRootAddr = m_pStorage->GetNewFilePageAddr();

		stream.Write(m_nRootAddr);

		bool bParams = pParams.get() != nullptr ? true : false;
 		stream.Write(bParams);
		m_pCompressParams = pParams;

		if (bParams)
			pParams->Save(&stream);

		m_objectType = objecttype;
		m_objectID = objectID;
		m_parentID = parentID;
		m_parentType = parenttype;

		stream.Write(objectID);
		stream.Write(objecttype);
		stream.Write(parentID);
		stream.Write(parenttype);

	
		stream.Close();

		m_cachePage = m_pStorage->GetNewPage(-1);

		m_pRoot = CreateNode(m_nRootAddr, true, false); 
		m_pRoot->SetFlags(ROOT_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to load root", exc);
	}
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::LoadTree()
{
	try
	{

		if (m_nPageBTreeInfo == -1)
			throw CommonLib::CExcBase("PageBTreeInfo == -1");
		 
		storage::TReadStreamPage stream(m_pStorage);
		stream.Open(m_nPageBTreeInfo);

		m_nRootAddr = stream.ReadInt64();
		bool bParams = stream.ReadBool();
 
		
		if (bParams)
		{
			m_pCompressParams.reset(new CompressorParamsBase());
			m_pCompressParams->Load(&stream);
		}
 		
		m_objectID = stream.ReadIntu32();
		m_objectType = stream.ReadIntu32();
		m_parentID = stream.ReadIntu32();
		m_parentType = stream.ReadIntu32();


		m_cachePage = m_pStorage->GetNewPage(-1);
		m_pRoot = LoadNodeFromStorage(m_nRootAddr);
		m_pRoot->SetFlags(ROOT_NODE, true);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to load root", exc);
	}
}

 

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::SaveNode(TBPTreeNodePtr& pNode)
{
	int64_t nAddr = pNode.get() ? pNode->GetAddr() : -1;
	uint32_t nInCount = pNode.get() ? pNode->Count() : 0;
	bool isLeaf = pNode.get() ? pNode->IsLeaf() : false;


	try
	{
		m_cachePage->Erase(0);
		CommonLib::CFxMemoryWriteStream stream;
		stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
		m_cachePage->SetAddr(nAddr);
		uint32_t nCount = 0;
		
		{
			CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, isLeaf ? eSaveLeafNode : eSaveInnerNode);
			nCount = pNode->Save(&stream, &m_Context);
		}

 

		while (nCount != 0)
		{

		 	if (m_pBPTreePerfCounter.get() != nullptr)
			{
				m_pBPTreePerfCounter->StartOperation(eMissedNode);
				m_pBPTreePerfCounter->StopOperation(eMissedNode);
			}

			TBPTreeNodePtr pParentNode = GetParentNode(pNode);
			if (pParentNode.get() == nullptr)
			{
				if (pNode->IsLeaf())
					TransformRootToInner();
				else
					SplitRootInnerNode();
			}
			else			
			{
				if (pNode->IsLeaf())
				{				
					TBPTreeNodePtr pNewNode = NewNode(true, true);
					SplitLeafNode(pNode, pNewNode, pParentNode, nCount);
				}
				else
				{
					TBPTreeNodePtr pNewNode = NewNode(false, true);
					SplitInnerNode(pNode, pNewNode, pParentNode, nCount);
				}
			}

			// the split may have loaded nodes (neighbour, parent) through the same m_cachePage:
			// restore the address and the stream before saving again
			m_cachePage->Erase(0);
			m_cachePage->SetAddr(nAddr);
			stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
			stream.Seek(0, CommonLib::soFromBegin);
			{
				CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, isLeaf ? eSaveLeafNode : eSaveInnerNode);
				nCount = pNode->Save(&stream, &m_Context);
			}
		}

		m_pStorage->SavePage(m_cachePage);

		pNode->SetFlags(CHANGE_NODE, false);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to save node addr: {0},  count: {1}, leaf: {2}", nAddr, nInCount, isLeaf, exc);
	}

}

BPSETBASE_TEMPLATE_PARAMS
uint32_t BPSETBASE_DECLARATION::TrySaveNode(TBPTreeNodePtr& pNode)
{
	try
	{
		m_cachePage->Erase(0);
		CommonLib::CFxMemoryWriteStream stream;
		stream.AttachBuffer(m_cachePage->GetPayLoadData(), m_cachePage->GetSize());
		m_cachePage->SetAddr(pNode->GetAddr());

		uint32_t nCount = 0;
		{
			CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, pNode->IsLeaf() ? eSaveLeafNode : eSaveInnerNode);
			nCount = pNode->Save(&stream, &m_Context);
		}
		if (nCount != 0)
			return nCount;

		m_pStorage->SavePage(m_cachePage);
		pNode->SetFlags(CHANGE_NODE, false);
		return 0;
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] failed to save node addr: {0}", pNode->GetAddr(), exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::CreateNode(int64_t nAddr,  bool isLeaf, bool addToChache)
{
	try
	{
		if (nAddr == -1)
			nAddr = m_pStorage->GetNewFilePageAddr();

		TBPTreeNodePtr pNode = AllocateNewNode(nAddr, isLeaf);

		pNode->SetFlags(CHANGE_NODE, true);
		//pNode->InitCopmressor(m_InnerCompressParams, m_LeafCompressParams);
		if (addToChache)
		{
			AddToCache(pNode);
		}

		return pNode;
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] to CreateNode addr {0}, isLeaf {1}, addToCache {2}", nAddr, isLeaf, addToChache, exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::NewNode(bool isLeaf, bool addToChache)
{
	return CreateNode(-1, isLeaf, addToChache);
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::Flush()
{
	try
	{
		CommonLib::CPrefCounterHolder holder(m_pBPTreePerfCounter, eFlush);
		if (m_pRoot.get() == nullptr)
			return;    // nothing was loaded or changed
		m_bLockRemoveItemFromCache = true; // TO DO use RAII

		while (m_pRoot->GetFlags() & CHANGE_NODE || m_NodeCache.Size() > 0)
		{
		
			while (m_NodeCache.Size())
			{

				TBPTreeNodePtr pBNode = m_NodeCache.RemoveBack();
				if (!(pBNode->GetFlags() & CHANGE_NODE))
					continue;

				SaveNode(pBNode);
			}

			if(m_pRoot->GetFlags() & CHANGE_NODE)
				SaveNode(m_pRoot);
		}
		
		m_bLockRemoveItemFromCache = false;
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("TBPlusTreeSetBase failed to flash", exc);
	}
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::CheckCache()
{
	m_bLockRemoveItemFromCache = true; // TO DO use RAII

	while (m_NodeCache.Size() > m_nCacheSize)
	{

		TBPTreeNodePtr pBNode = m_NodeCache.RemoveBack();
		if (!(pBNode->GetFlags() & CHANGE_NODE))
			continue;

		SaveNode(pBNode);
	}

	m_bLockRemoveItemFromCache = false;
}


BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::SetBPTreePerfCounter(CommonLib::TPrefCounterPtr pBPTreePerfCounter)
{
	m_pBPTreePerfCounter = pBPTreePerfCounter;
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::GetMinimumNode(TBPTreeNodePtr pNode)
 {
	// the leftmost leaf of the subtree; null if the subtree has no leaves (an inner node without children)
	TBPTreeNodePtr pMinNode = pNode;
	while (pMinNode.get() && !pMinNode->IsLeaf())
		pMinNode = GetNode(pMinNode->Less());

	return pMinNode;
 }

BPSETBASE_TEMPLATE_PARAMS
uint32_t BPSETBASE_DECLARATION::GetNodePageSize() const
{
	return m_cachePage->GetSize();
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::CheckTreeStructure()
{
	try
	{
		if (m_pRoot.get() == nullptr)
			LoadTree();
		int32_t nLeafDepth = -1;
		CheckSubtree(m_pRoot, nullptr, nullptr, 0, nLeafDepth);
	}
	catch (std::exception& exc)
	{
		CommonLib::CExcBase::RegenExcT("[TBPlusTreeSetBase] CheckTreeStructure failed", exc);
		throw;
	}
}

BPSETBASE_TEMPLATE_PARAMS
BPSETBASE_TYPENAME_DECLARATION::TBPTreeNodePtr BPSETBASE_DECLARATION::PeekNodeForCheck(int64_t nAddr)
{
	if (nAddr == m_nRootAddr)
		return m_pRoot;

	TBPTreeNodePtr pNode = GetNodeFromCache(nAddr, true);
	if (pNode.get())
		return pNode;

	storage::IFilePagePtr pPage = m_pStorage->ReadPage(nAddr);
	CommonLib::CReadMemoryStream stream;
	stream.AttachBuffer(pPage->GetPayLoadData(), pPage->GetSize());
	return LoadNode<TBPTreeNode>(&stream, m_pAllocsSet, GetNodePageSize(), nAddr, m_pCompressParams, &m_Context);
}

BPSETBASE_TEMPLATE_PARAMS
void BPSETBASE_DECLARATION::CheckSubtree(TBPTreeNodePtr pNode, const TKey* pLo, const TKey* pHi, uint32_t nDepth, int32_t& nLeafDepth)
{
	if (!pNode.get())
		throw CommonLib::CExcBase("missing node at depth {0}", nDepth);

	const int64_t nAddr = pNode->GetAddr();
	const uint32_t nCount = pNode->Count();

	auto checkKeys = [&]()
	{
		for (uint32_t i = 0; i < nCount; ++i)
		{
			const TKey& k = pNode->Key(i);
			if (pLo && m_comp.LE(k, *pLo))
				throw CommonLib::CExcBase("node {0}: key {1} below the separator {2}", nAddr, i, KeyToStr(*pLo));
			if (pHi && !m_comp.LE(k, *pHi))
				throw CommonLib::CExcBase("node {0}: key {1} not below the next separator {2}", nAddr, i, KeyToStr(*pHi));
			if (i > 0 && !m_comp.LE(pNode->Key(i - 1), k))
				throw CommonLib::CExcBase("node {0}: keys not sorted at {1}", nAddr, i);
		}
	};

	if (pNode->IsLeaf())
	{
		if (nCount == 0 && nAddr != m_nRootAddr)
			throw CommonLib::CExcBase("empty leaf {0}", nAddr);
		if (nLeafDepth == -1)
			nLeafDepth = (int32_t)nDepth;
		else if (nLeafDepth != (int32_t)nDepth)
			throw CommonLib::CExcBase("leaf {0} at depth {1}, other leaves at {2}", nAddr, nDepth, nLeafDepth);
		checkKeys();
		return;
	}

	if (pNode->Less() == EMPTY_PAGE_ADDR)
		throw CommonLib::CExcBase("inner node {0} without Less", nAddr);
	checkKeys();

	// copies: children are loaded while this node is walked
	std::vector<TKey> keys;
	std::vector<TLink> links;
	keys.reserve(nCount);
	links.reserve(nCount);
	for (uint32_t i = 0; i < nCount; ++i)
	{
		keys.push_back(pNode->Key(i));
		links.push_back(pNode->Link(i));
	}
	const TLink nLess = pNode->Less();

	CheckSubtree(PeekNodeForCheck(nLess), pLo, nCount ? &keys[0] : pHi, nDepth + 1, nLeafDepth);
	for (uint32_t i = 0; i < nCount; ++i)
		CheckSubtree(PeekNodeForCheck(links[i]), &keys[i], i + 1 < nCount ? &keys[i + 1] : pHi, nDepth + 1, nLeafDepth);
}
