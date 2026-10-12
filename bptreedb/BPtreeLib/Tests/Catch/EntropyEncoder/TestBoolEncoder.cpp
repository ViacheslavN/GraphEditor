#include "TestCommon.h"

using namespace bptreedb;
using namespace test_utils;

namespace
{
	// true at every `period`-th position (starting at `offset`), false elsewhere
	std::vector<bool> Periodic(size_t count, size_t period, bool value, size_t offset = 0)
	{
		std::vector<bool> bits(count, !value);
		for (size_t i = offset; i < count; i += period)
			bits[i] = value;
		return bits;
	}

	std::vector<bool> Random(size_t count, double pTrue, uint32_t seed)
	{
		std::mt19937 rng(seed);
		std::bernoulli_distribution dist(pTrue);
		std::vector<bool> bits(count);
		for (size_t i = 0; i < count; ++i)
			bits[i] = dist(rng);
		return bits;
	}

	void AddAll(CBoolEncoder& enc, const std::vector<bool>& bits)
	{
		for (bool b : bits)
			enc.AddSymbol(b);
	}

	// Encodes `bits`, checks that GetCompressSize() equals the bytes written, returns the buffer.
	std::vector<byte_t> Encode(CBoolEncoder& enc, const std::vector<bool>& bits)
	{
		enc.Clear();
		AddAll(enc, bits);
		const uint32_t nEstimate = enc.GetCompressSize();

		SWriteBuf wb(nEstimate + 64);
		REQUIRE(enc.BeginEncoding(&wb.stream));
		for (size_t i = 0; i < bits.size(); ++i)
			enc.EncodeBit(bits[i], (uint32_t)i);

		// all payload is reserved by BeginEncoding, so Pos() is the final size
		CHECK(wb.stream.Pos() == nEstimate);
		return wb.buf;
	}

	void Decode(CBoolEncoder& dec, std::vector<byte_t>& buf, const std::vector<bool>& bits)
	{
		SReadBuf rb(buf);
		dec.BeginDecoding(&rb.stream);
		for (size_t i = 0; i < bits.size(); ++i)
		{
			INFO("position " << i);
			REQUIRE(dec.DecodeBit((uint32_t)i) == bits[i]);
		}
	}

	void RoundTrip(const std::vector<bool>& bits)
	{
		CBoolEncoder enc;
		std::vector<byte_t> buf = Encode(enc, bits);
		CBoolEncoder dec;
		Decode(dec, buf, bits);
	}
}

// ---------------------------------------------------------------------------
// Round trip in every mode (OneBit, BitEncode, PosBits)
// ---------------------------------------------------------------------------

TEST_CASE("CBoolEncoder OneBit: all bits equal", "[entropy][bool]")
{
	SECTION("all false") { RoundTrip(std::vector<bool>(1000, false)); }
	SECTION("all true")  { RoundTrip(std::vector<bool>(1000, true)); }
	SECTION("single bit") { RoundTrip(std::vector<bool>(1, true)); }
}

TEST_CASE("CBoolEncoder BitEncode: balanced bits", "[entropy][bool]")
{
	SECTION("random 50/50") { RoundTrip(Random(5000, 0.5, 1)); }
	SECTION("alternating")  { RoundTrip(Periodic(4001, 2, true)); }
	SECTION("small count")  { RoundTrip({ true, false, true }); }
}

TEST_CASE("CBoolEncoder PosBits: rare true bits", "[entropy][bool]")
{
	SECTION("byte positions")   { RoundTrip(Periodic(200, 40, true)); }
	SECTION("uint16 positions") { RoundTrip(Periodic(20000, 64, true, 3)); }
	SECTION("uint32 positions") { RoundTrip(Periodic(100000, 100, true, 7)); }
}

// Regression: PosBits used to always store positions of `true`, even when `true` was the
// majority, overflowing the position buffer that is sized for the minority count.
TEST_CASE("CBoolEncoder PosBits: rare false bits (minority is false)", "[entropy][bool][regression]")
{
	SECTION("byte positions")   { RoundTrip(Periodic(200, 40, false)); }
	SECTION("uint16 positions") { RoundTrip(Periodic(20000, 64, false, 5)); }
	SECTION("uint32 positions") { RoundTrip(Periodic(100000, 100, false, 1)); }
}

// Regression: 16-bit positions were read back with Readint16, so positions >= 32768 came
// back negative and the decoded sign was flipped.
TEST_CASE("CBoolEncoder PosBits: 16-bit positions above 32767", "[entropy][bool][regression]")
{
	std::vector<bool> bits = Periodic(60000, 20, true, 11);  // positions up to 59991, uint16 type
	RoundTrip(bits);

	std::vector<bool> inverted = Periodic(60000, 20, false, 11);
	RoundTrip(inverted);
}

// ---------------------------------------------------------------------------
// Size estimate
// ---------------------------------------------------------------------------

TEST_CASE("CBoolEncoder GetCompressSize matches the written size in every mode", "[entropy][bool][regression]")
{
	// Encode() CHECKs estimate == written; run it over a spread of distributions.
	CBoolEncoder enc;
	for (size_t count : { 1, 7, 255, 256, 1000, 40000, 70000 })
	{
		for (double p : { 0.0, 0.01, 0.1, 0.3, 0.5, 0.9, 0.99, 1.0 })
		{
			INFO("count " << count << ", p(true) " << p);
			std::vector<bool> bits = Random(count, p, (uint32_t)(count * 100 + p * 100));
			std::vector<byte_t> buf = Encode(enc, bits);
			CBoolEncoder dec;
			Decode(dec, buf, bits);
		}
	}
}

TEST_CASE("CBoolEncoder RemoveSymbol undoes AddSymbol", "[entropy][bool]")
{
	std::vector<bool> base = Random(3000, 0.05, 7);
	std::vector<bool> extra = Random(2000, 0.7, 8);

	CBoolEncoder a, b;
	AddAll(a, base);
	AddAll(a, extra);
	for (bool v : extra)
		a.RemoveSymbol(v);
	AddAll(b, base);

	REQUIRE(a.GetCompressSize() == b.GetCompressSize());
}

// ---------------------------------------------------------------------------
// Stream handling
// ---------------------------------------------------------------------------

// Regression: BeginEncoding had no return in the OneBit branch (C4715) and used
// throwing writes for the flag byte; a full page must return false, not throw.
TEST_CASE("CBoolEncoder BeginEncoding returns false when the stream is too small", "[entropy][bool][regression]")
{
	auto check = [](const std::vector<bool>& bits)
	{
		CBoolEncoder enc;
		AddAll(enc, bits);
		const uint32_t nNeed = enc.GetCompressSize();
		for (uint32_t size : { 1u, 5u, nNeed - 1 })
		{
			INFO("stream size " << size << ", needed " << nNeed);
			SWriteBuf wb(size);
			bool ok = true;
			REQUIRE_NOTHROW(ok = enc.BeginEncoding(&wb.stream));
			REQUIRE_FALSE(ok);
		}
		SWriteBuf wb(nNeed);
		REQUIRE(enc.BeginEncoding(&wb.stream));
	};

	SECTION("OneBit")    { check(std::vector<bool>(100, false)); }
	SECTION("BitEncode") { check(Random(1000, 0.5, 3)); }
	SECTION("PosBits")   { check(Periodic(20000, 64, true)); }
}

// Regression: TMemoryStreamBase::AttachBuffer kept the old position, so the encoder's
// internal position stream overflowed the second time the same encoder was used.
TEST_CASE("CBoolEncoder can be reused for several encodings", "[entropy][bool][regression]")
{
	CBoolEncoder enc;
	CBoolEncoder dec;
	for (int rep = 0; rep < 5; ++rep)
	{
		INFO("repetition " << rep);
		std::vector<bool> bits = Periodic(40000 + rep, 32, rep % 2 == 0, rep);
		std::vector<byte_t> buf = Encode(enc, bits);
		Decode(dec, buf, bits);  // same decoder object every time
	}
}
