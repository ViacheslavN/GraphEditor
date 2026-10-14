#pragma once

#include <type_traits>

namespace bptreedb
{
	// The leaf compressor writes as many elements as fit and says how many didn't (TCutNodeCompressor):
	//   static constexpr bool CutWrite = true;
	// The tree encodes such a full leaf once and splits it where the page is full (TBPlusTreeSetBase::CheckLeafNode).
	template <class T, class = void>
	struct TIsCutWrite : std::false_type {};

	template <class T>
	struct TIsCutWrite<T, std::void_t<decltype(T::CutWrite)> > : std::integral_constant<bool, T::CutWrite> {};

	// An encoder that writes as many first elements as fit (EncodeCut: the size is known only by encoding, zlib):
	//   static constexpr bool CanCut = true;
	//   void EncodeCut(const TValueMemSet& vec, CommonLib::IMemoryWriteStream* pStream, uint32_t maxSize, uint32_t& nEncoded);
	// TCutNodeCompressor cuts by it; the other encoder of the node computes its size.
	template <class T, class = void>
	struct TIsCutEncoder : std::false_type {};

	template <class T>
	struct TIsCutEncoder<T, std::void_t<decltype(T::CanCut)> > : std::integral_constant<bool, T::CanCut> {};

	// a leaf node type: by its compressor
	template <class TLeaf, class = void>
	struct TLeafCutWrite : std::false_type {};

	template <class TLeaf>
	struct TLeafCutWrite<TLeaf, std::void_t<typename TLeaf::TCompressor> > : TIsCutWrite<typename TLeaf::TCompressor> {};
}
