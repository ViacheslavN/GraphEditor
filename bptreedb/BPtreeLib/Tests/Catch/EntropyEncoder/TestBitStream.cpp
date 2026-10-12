#include "TestCommon.h"

using namespace bptreedb;
using namespace test_utils;

// ---------------------------------------------------------------------------
// CBitBase::GetByteForBits
// ---------------------------------------------------------------------------

TEST_CASE("GetByteForBits rounds up and adds 64-bit padding", "[entropy][bitstream]")
{
	// The writer stores whole uint64 words, so 8 bytes of slack are always reserved.
	REQUIRE(CBitBase::GetByteForBits(0) == 8);
	REQUIRE(CBitBase::GetByteForBits(1) == 9);
	REQUIRE(CBitBase::GetByteForBits(8) == 9);
	REQUIRE(CBitBase::GetByteForBits(9) == 10);
	REQUIRE(CBitBase::GetByteForBits(64) == 16);
}

// ---------------------------------------------------------------------------
// CBitWriter / CBitReader
// ---------------------------------------------------------------------------

TEST_CASE("CBitWriter packs bits LSB first", "[entropy][bitstream]")
{
	std::vector<byte_t> buf = ZeroBuffer(CBitBase::GetByteForBits(16));
	CBitWriter w;
	w.AttachBuffer(buf.data());

	w.WriteBits(0x1, 1);
	w.WriteBits(0x0, 6);
	w.WriteBits(0x1, 1);   // byte 0 = 1000 0001
	w.WriteBits(0xAB, 8);  // byte 1

	REQUIRE(buf[0] == 0x81);
	REQUIRE(buf[1] == 0xAB);
}

TEST_CASE("CBitWriter masks values wider than the requested bit count", "[entropy][bitstream]")
{
	std::vector<byte_t> buf = ZeroBuffer(CBitBase::GetByteForBits(8));
	CBitWriter w;
	w.AttachBuffer(buf.data());

	w.WriteBits(0xFF, 3);
	w.WriteBits(0x0, 5);

	REQUIRE(buf[0] == 0x07);
}

// WriteBits/ReadBits handle at most 63 bits (31 + 32 through a 33-entry mask table).
// That is all the NumLen encoder needs: a 64-bit value is stored as its length plus
// the 63 bits below the implicit leading 1.
static const uint32_t MAX_BITS = 63;

TEST_CASE("CBitWriter / CBitReader round trip every width 0..63", "[entropy][bitstream]")
{
	std::mt19937_64 rng(12345);

	std::vector<std::pair<uint64_t, uint32_t>> values;
	uint64_t totalBits = 0;
	for (int rep = 0; rep < 20; ++rep)
	{
		for (uint32_t width = 0; width <= MAX_BITS; ++width)
		{
			uint64_t mask = (uint64_t(1) << width) - 1;
			values.emplace_back(rng() & mask, width);
			totalBits += width;
		}
	}

	std::vector<byte_t> buf = ZeroBuffer(CBitBase::GetByteForBits((uint32_t)totalBits));
	CBitWriter w;
	w.AttachBuffer(buf.data());
	for (auto& v : values)
		w.WriteBits(v.first, v.second);

	CBitReader r;
	r.AttachBuffer(buf.data());
	for (size_t i = 0; i < values.size(); ++i)
	{
		INFO("index " << i << ", width " << values[i].second);
		REQUIRE(r.ReadBits(values[i].second) == values[i].first);
	}
}

TEST_CASE("CBitWriter / CBitReader handle the extreme values of each width", "[entropy][bitstream]")
{
	std::vector<std::pair<uint64_t, uint32_t>> values;
	uint64_t totalBits = 0;
	for (uint32_t width = 1; width <= MAX_BITS; ++width)
	{
		uint64_t maxVal = (uint64_t(1) << width) - 1;
		values.emplace_back(maxVal, width);
		values.emplace_back(0, width);
		values.emplace_back(uint64_t(1) << (width - 1), width);
		totalBits += 3 * width;
	}

	std::vector<byte_t> buf = ZeroBuffer(CBitBase::GetByteForBits((uint32_t)totalBits));
	CBitWriter w;
	w.AttachBuffer(buf.data());
	for (auto& v : values)
		w.WriteBits(v.first, v.second);

	CBitReader r;
	r.AttachBuffer(buf.data());
	for (auto& v : values)
	{
		INFO("value " << v.first << ", width " << v.second);
		REQUIRE(r.ReadBits(v.second) == v.first);
	}
}

TEST_CASE("CBitWriter stays inside GetByteForBits(total bits)", "[entropy][bitstream]")
{
	// Guard bytes after the reserved area must stay untouched.
	const uint32_t nBits = 1000 * 63;
	const size_t nReserved = CBitBase::GetByteForBits(nBits);
	std::vector<byte_t> buf(nReserved + 16, 0);
	for (size_t i = nReserved; i < buf.size(); ++i)
		buf[i] = 0xCD;

	CBitWriter w;
	w.AttachBuffer(buf.data());
	for (int i = 0; i < 1000; ++i)
		w.WriteBits(~uint64_t(0), 63);

	for (size_t i = nReserved; i < buf.size(); ++i)
		REQUIRE(buf[i] == 0xCD);
}

TEST_CASE("CBitBase::AttachBuffer restarts at bit 0", "[entropy][bitstream]")
{
	std::vector<byte_t> buf1 = ZeroBuffer(16), buf2 = ZeroBuffer(16);
	CBitWriter w;
	w.AttachBuffer(buf1.data());
	w.WriteBits(0x3, 5);

	w.AttachBuffer(buf2.data());
	w.WriteBits(0x5, 3);
	REQUIRE(buf2[0] == 0x05);
}
