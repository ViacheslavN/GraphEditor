#pragma once

// Shared includes and helpers for the BPtreeLib/Compressor/EntropyEncoder Catch2 tests.

#ifdef _WIN32
#define NOMINMAX
#endif

#include <catch2/catch_test_macros.hpp>

#include "CommonLib/CommonLib.h"
#include "CommonLib/exception/exc_base.h"
#include "CommonLib/stream/MemoryStream.h"
#include "CommonLib/stream/FixedMemoryStream.h"

#include "BPtreeLib/Compressor/CompressorParams.h"
#include "BPtreeLib/Compressor/EntropyEncoder/BitStream.h"
#include "BPtreeLib/Compressor/EntropyEncoder/BoolEncoder.h"
#include "BPtreeLib/Compressor/EntropyEncoder/UnsignedNumLenEncoder.h"
#include "BPtreeLib/Compressor/EntropyEncoder/SignedNumLenEncoder.h"

#include <vector>
#include <cstdint>
#include <random>
#include <limits>

namespace test_utils
{
	// Parameters for every ECompressParams id; the NumLen encoders dereference them in the constructor.
	inline bptreedb::TCompressorParamsBasePtr MakeParams()
	{
		bptreedb::TCompressorParamsBasePtr params(new bptreedb::CompressorParamsBase());
		for (bptreedb::ECompressParams id : { bptreedb::eInnerKey, bptreedb::eInnerValue, bptreedb::eLeafKey, bptreedb::eLeafValue })
			params->AddCompressParams(bptreedb::TCompressorParamsPtr(new bptreedb::CompressorParams()), id);
		return params;
	}

	// Zeroed buffer: the bit writers OR bits in and rely on it.
	inline std::vector<byte_t> ZeroBuffer(size_t size)
	{
		return std::vector<byte_t>(size, 0);
	}

	// Fixed-size write stream over a buffer (same kind of stream the tree uses for pages).
	struct SWriteBuf
	{
		std::vector<byte_t> buf;
		CommonLib::CFxMemoryWriteStream stream;

		explicit SWriteBuf(size_t size) : buf(size, 0)
		{
			stream.AttachBuffer(buf.data(), buf.size());
		}
	};

	struct SReadBuf
	{
		CommonLib::CReadMemoryStream stream;

		explicit SReadBuf(std::vector<byte_t>& buf)
		{
			stream.AttachBuffer(buf.data(), buf.size());
		}
	};
}
