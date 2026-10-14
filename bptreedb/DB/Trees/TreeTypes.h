#pragma once

// Internal header of the tree implementations (Trees/*.cpp): the node and tree types of the row id trees,
// the value trees and the indexes, and the common holder of a tree. The implementations are split into
// several files: the instantiations of all the tree types in one object file exceed the section limit of
// MSVC (C1128 without /bigobj).

#include "../DBTrees.h"
#include "../Values.h"

#include "../../../CommonLib/stream/MemoryStream.h"
#include "../../../CommonLib/stream/FixedMemoryStream.h"
#include "../../../CommonLib/data/blob.h"
#include "../../../CommonLib/SpatialData/GeoShape.h"

#include "../../BPtreeLib/BPMap.h"
#include "../../BPtreeLib/BPMultiMap.h"
#include "../../BPtreeLib/BPStringValueMap.h"
#include "../../BPtreeLib/BPBlobValueMap.h"
#include "../../BPtreeLib/BPTreeInnerNode.h"
#include "../../BPtreeLib/BPTreeMapLeafNode.h"
#include "../../BPtreeLib/BaseNodeCompressor.h"
#include "../../BPtreeLib/CutNodeCompressor.h"
#include "../../BPtreeLib/BaseEncoder.h"
#include "../../BPtreeLib/BaseDIffEncoder.h"
#include "../../BPtreeLib/OrderedKey.h"
#include "../../BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "../../BPtreeLib/Compressor/StringCompressor/ZLibStringEncoder.h"
#include "../../BPtreeLib/Compressor/StringCompressor/ZlibCutStringEncoder.h"
#include "../../BPtreeLib/Compressor/BlobCompressor/BlobValueEncoder.h"
#include "../../BPtreeLib/Compressor/OrderedEncoder/OrderedDiffEncoder.h"
#include "../../BPtreeLib/Compressor/OrderedEncoder/ShuffleZlibEncoder.h"

#include "../../SpatialLib/RTree/RTree.h"
#include "../../SpatialLib/ZOrderTree/BPZOrderGridIndex.h"

#include <algorithm>
#include <cstring>

namespace bptreedb
{
	namespace db
	{
		namespace trees
		{
			// the value trees and the indexes by the groups of types (ValueFields*.cpp, IndexTrees*.cpp)
			// storageType: evsNumberNumLen or evsNumber (zlib, older files)
			IValueFieldPtr CreateSignedValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage);
			IValueFieldPtr CreateUnsignedValueField(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, EValueStorage storageType, uint32_t nLength, int64_t nInfoPage);
			IIndexTreePtr CreateSignedIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage);
			IIndexTreePtr CreateUnsignedIndexTree(SDBContextPtr ptrContext, storage::IFilePagerPtr ptrPager, eDataTypes type, int64_t nInfoPage);

			typedef TValueDiffEncoder<int64_t, int64_t, CUnsignedNumLenEncoder> TRowIdEncoder;   // sorted row ids
			typedef TBaseValueEncoder<int64_t, CUnsignedNumLenEncoder> TLinkEncoder;            // child addresses
			typedef TOrderedComp<int64_t> TRowIdComp;

			// a tree with row id keys
			template <class TValue, class TValueEncoder>
			struct TRowIdNodes
			{
				typedef BPTreeLeafNodeMap<int64_t, TValue, TBaseNodeCompressor<int64_t, TValue, TRowIdEncoder, TValueEncoder> > TLeaf;
				typedef BPTreeInnerNode<int64_t, TBaseNodeCompressor<int64_t, int64_t, TRowIdEncoder, TLinkEncoder> > TInner;
			};

			typedef TRowIdNodes<uint8_t, TNullValueEncoder<uint8_t> > TRowNodes;
			typedef TBPMap<int64_t, uint8_t, TRowIdComp, storage::IFilePager, TRowNodes::TInner, TRowNodes::TLeaf> TRowTreeImpl;

			// the numbers: NumLen of the differences (the size is computed: a full leaf splits off one element);
			// TShuffleZlibEncoder - the value trees of the files of older versions (evsNumber)
			template <class T, class TEncoder = TNumberValueEncoder<T> >
			struct TNumberTree
			{
				typedef TRowIdNodes<T, TEncoder> TNodes;
				typedef TBPMap<int64_t, T, TRowIdComp, storage::IFilePager, typename TNodes::TInner, typename TNodes::TLeaf> TTree;
			};

			// a tree with row id keys and values compressed by zlib: TCutNodeCompressor
			template <class TValue, class TValueEncoder>
			struct TRowIdCutNodes
			{
				typedef BPTreeLeafNodeMap<int64_t, TValue, TCutNodeCompressor<int64_t, TValue, TRowIdEncoder, TValueEncoder> > TLeaf;
				typedef typename TRowIdNodes<TValue, TValueEncoder>::TInner TInner;
			};

			// the strings: a leaf is encoded once and cut where the page is full (evsStringCut);
			// TStringTree - zlib per leaf by the estimate, the value trees of the files of older versions (evsString)
			typedef TRowIdCutNodes<StringValue, CZlibCutStringComp> TStringCutNodes;
			typedef TBPStringValueMap<int64_t, TRowIdComp, storage::IFilePager, TStringCutNodes::TInner, TStringCutNodes::TLeaf> TStringCutTree;

			typedef TRowIdNodes<StringValue, CZlibStringComp> TStringNodes;
			typedef TBPStringValueMap<int64_t, TRowIdComp, storage::IFilePager, TStringNodes::TInner, TStringNodes::TLeaf> TStringTree;

			typedef TRowIdNodes<BlobValue, CBlobValueEncoder> TBlobNodes;
			typedef TBPBlobValueMap<int64_t, TRowIdComp, storage::IFilePager, TBlobNodes::TInner, TBlobNodes::TLeaf> TBlobTree;

			// indexes: (value, row id) keys; bCut - the string keys by TStringMultiKeyCutEncoder (TCutNodeCompressor
			// leaves), the numbers have one format
			template <class K, bool bCut = false>
			struct TIndexTypes
			{
				typedef TMultiKey<K> TMKey;
				typedef TOrderedComp<K> TComp;
				typedef TOrderedMultiKeyEncoder<K> TKeyEncoder;
				typedef BPTreeLeafNodeMap<TMKey, uint8_t, TBaseNodeCompressor<TMKey, uint8_t, TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
				typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
				typedef TBPMultiMap<K, TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
			};

			template <>
			struct TIndexTypes<StringValue, false>
			{
				typedef TMultiKey<StringValue> TMKey;
				typedef CompStringValue TComp;
				typedef TStringMultiKeyEncoder TKeyEncoder;
				typedef BPTreeLeafNodeMap<TMKey, uint8_t, TBaseNodeCompressor<TMKey, uint8_t, TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
				typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
				typedef TBPMultiMap<StringValue, TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
			};

			template <>
			struct TIndexTypes<StringValue, true>
			{
				typedef TMultiKey<StringValue> TMKey;
				typedef CompStringValue TComp;
				typedef TStringMultiKeyCutEncoder TKeyEncoder;
				typedef BPTreeLeafNodeMap<TMKey, uint8_t, TCutNodeCompressor<TMKey, uint8_t, TKeyEncoder, TNullValueEncoder<uint8_t> > > TLeaf;
				typedef BPTreeInnerNode<TMKey, TBaseNodeCompressor<TMKey, int64_t, TKeyEncoder, TLinkEncoder> > TInner;
				typedef TBPMultiMap<StringValue, TComp, uint8_t, storage::IFilePager, TInner, TLeaf> TTree;
			};

			inline uint32_t BlobInlineSize(uint32_t nPageSize)
			{
				// (inline + 2 * 8 + 8) * 2 <= page size (TBPBlobValueMap)
				return (std::min)((uint32_t)TBlobTree::DefaultMaxInlineSize, nPageSize / 4);
			}

			//--------------------------------------------------------------------------------------------
			// the pages of a B+ tree: the inner nodes are read, the leaves are the links of the lowest inner level
			// (all the leaves are at one depth)

			template <class TTree>
			void BPTreeSpace(TTree& tree, STreeSpace& space)
			{
				space.m_nInfoPages += 1;
				auto pRoot = tree.Root();
				if (!pRoot.get())
					throw CommonLib::CExcBase("the tree has no root");
				if (pRoot->IsLeaf())
				{
					space.m_nLeafPages += 1;
					space.m_nLevels = 1;
					return;
				}

				// the depth: the leftmost path
				uint32_t nLevels = 1;
				{
					auto pNode = pRoot;
					while (!pNode->IsLeaf())
					{
						pNode = tree.GetNode(pNode->Less());
						if (!pNode.get())
							throw CommonLib::CExcBase("a node of the tree isn't found");
						++nLevels;
					}
				}
				space.m_nLevels = nLevels;

				std::vector<std::pair<int64_t, uint32_t> > stack;   // inner nodes: the address, the level (the root 1)
				auto visit = [&](const decltype(pRoot)& pNode, uint32_t nLevel)
				{
					space.m_nInnerPages += 1;
					int32_t nCount = (int32_t)pNode->Count();
					if (nLevel + 1 == nLevels)
					{
						space.m_nLeafPages += (uint64_t)nCount + 1;     // Less() and the links
						return;
					}
					stack.emplace_back(pNode->Less(), nLevel + 1);
					for (int32_t i = 0; i < nCount; ++i)
						stack.emplace_back(pNode->Link(i), nLevel + 1);
				};

				visit(pRoot, 1);
				while (!stack.empty())
				{
					std::pair<int64_t, uint32_t> item = stack.back();
					stack.pop_back();
					auto pNode = tree.GetNode(item.first);
					if (!pNode.get())
						throw CommonLib::CExcBase("node {0} of the tree isn't found", item.first);
					visit(pNode, item.second);
					tree.CheckCache();
				}
			}

			//--------------------------------------------------------------------------------------------
			// common part of the trees: creation, flush, change counter

			template <class TTree>
			class TTreeHolder
			{
			public:
				// bMinSplit: the keys are inserted in ascending order (row ids)
				TTreeHolder(SDBContextPtr ptrContext, std::unique_ptr<TTree>&& ptrTree, int64_t nInfoPage, uint32_t nMaxLen, bool bMinSplit) :
					m_ptrContext(ptrContext), m_ptrTree(std::move(ptrTree))
				{
					if (nInfoPage == -1)
					{
						m_ptrTree->InnitTree(ptrContext->MakeTreeParams(nMaxLen), bMinSplit, 0, storage::eEmptyType, 0, storage::eEmptyType);
						m_bChanged = true;
					}
					m_ptrTree->SetMinSplit(bMinSplit);
				}

				TTree& Tree() { return *m_ptrTree; }
				int64_t InfoPage() const { return m_ptrTree->GetPageBTreeInfo(); }
				uint64_t Changes() const { return m_nChanges; }
				void Changed() { ++m_nChanges; m_bChanged = true; }

				void FlushTree()
				{
					if (!m_bChanged)
						return;
					m_ptrTree->Flush();
					m_bChanged = false;
				}

			protected:
				SDBContextPtr m_ptrContext;
				std::unique_ptr<TTree> m_ptrTree;
				uint64_t m_nChanges{ 0 };
				bool m_bChanged{ false };
			};
		}
	}
}
