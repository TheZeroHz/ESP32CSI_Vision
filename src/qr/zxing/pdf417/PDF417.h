/*
 * Copyright 2026 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#pragma once

#include "../BitMatrixCursor.h"
#include "../Pattern.h"
#include "../Point.h"
#include "../ZXAlgorithms.h"

namespace ZXing::PDF417 {

constexpr int MS_THR = 2; // tolerance of pattern size in modules

using Pattern417 = std::array<uint16_t, 8>;

/** Shared MicroPDF417 / PDF417 scanning codeword (not Pdf417::Codeword). C++17-safe (no bitfield defaults). */
struct Codeword
{
	int codeword = -1;
	int cluster = -1;
	int count = 0;
	PointT<float> left{}, right{};

	constexpr operator bool() const noexcept { return codeword != -1 && cluster % 3 == 0; }
	constexpr bool operator==(const Codeword& other) const noexcept { return codeword == other.codeword; }
	PointF leftPos() const noexcept { return PointF(left / count); }
	PointF rightPos() const noexcept { return PointF(right / count); }
};

struct CodewordPattern
{
	int codeword = 0;    // enough for [-1..929)
	unsigned bits = 0;   // packed normalized e2e pattern

	constexpr bool operator<(const CodewordPattern& other) const noexcept { return bits < other.bits; }
	constexpr int pattern(int i) const noexcept { return (bits >> ((6 - i - 1) * 3)) & 0b111; }
	constexpr int cluster() const noexcept { return (pattern(0) - pattern(1) + pattern(4) - pattern(5) + 9) % 9; }
};

constexpr int CodewordCluster(const std::array<int, 8>& np)
{
	return (np[0] - np[2] + np[4] - np[6] + 9) % 9;
}

constexpr int CodewordCluster(const std::array<int, 6>& np)
{
	return (np[0] - np[1] + np[4] - np[5] + 9) % 9;
}

template <typename POINT>
class BitMatrixModuleCursor : public BitMatrixCursor<POINT>
{
public:
	float ms;

	BitMatrixModuleCursor(const BitMatrix& image, POINT p, POINT d, float ms) : BitMatrixCursor<POINT>(image, p, d), ms(ms) {}

	BitMatrixModuleCursor movedBy(POINT o) const noexcept { return {*this->img, this->p + o, this->d, ms}; }
};

using BitMatrixModuleCursorF = BitMatrixModuleCursor<PointF>;

Codeword ReadCodeword(BitMatrixModuleCursorF& cur);
Codeword ReadCodeword(BitMatrixModuleCursorF& cur, int expectedCluster);

bool SkipCodeword(BitMatrixModuleCursorF& cur);

} // namespace ZXing::PDF417
