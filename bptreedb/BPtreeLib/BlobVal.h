#pragma once
#include "../../CommonLib/alloc/alloc.h"

namespace bptreedb
{
	// The value of a blob map as it is kept in a leaf node.
	//
	// Small blob (inline, m_nAddr == 0): the bytes are in the node (in the blob allocator).
	// Big blob (m_nAddr != 0): a chain of stream pages (TWriteStreamPage / TReadStreamPage) starting at
	// m_nAddr, the node keeps only the address. The stream: int64 size, then the bytes.
	//
	// Leaf encoding (CBlobValueEncoder):
	//   int64 addr
	//   addr == 0: uint16 size, size bytes
	//
	// Page address 0 is never used for a blob stream: 0 marks an inline blob.
	struct BlobValue
	{
		static constexpr uint32_t MaxInlineLimit = 0xFFFF;   // the inline size is written as uint16

		BlobValue();
		BlobValue(const byte_t* pData, uint32_t nSize, CommonLib::IAllocPtr pAlloc);   // inline, copies the data
		BlobValue(uint32_t nSize, CommonLib::IAllocPtr pAlloc);                         // inline, uninitialized data
		explicit BlobValue(int64_t nAddr);                                              // in the stream pages
		~BlobValue();

		BlobValue(const BlobValue&);
		BlobValue(BlobValue&&);
		BlobValue& operator= (const BlobValue&);
		BlobValue& operator= (BlobValue&&);

		bool IsInline() const { return m_nAddr == 0; }
		int64_t Addr() const { return m_nAddr; }

		// inline blob only (0 for a stream blob: its size is in the stream, see IBlob::Size)
		uint32_t InlineSize() const { return m_nSize; }

		// inline data (nullptr for a stream blob or an empty one)
		const byte_t* Data() const { return m_pData; }
		byte_t* Data() { return m_pData; }

		// bytes in the leaf node
		uint32_t EncodedSize() const
		{
			return IsInline() ? (uint32_t)(sizeof(int64_t) + sizeof(uint16_t) + m_nSize) : (uint32_t)sizeof(int64_t);
		}

	private:
		void Clear();
		void CopyFrom(const BlobValue& val);
		void MoveFrom(BlobValue& val);

		int64_t m_nAddr{ 0 };
		uint32_t m_nSize{ 0 };
		byte_t* m_pData{ nullptr };
		CommonLib::IAllocPtr m_pAlloc;
	};
}
