/*
 * Copyright 2026 Axel Waggershauser
 */
// SPDX-License-Identifier: Apache-2.0

#include "PDF417.h"

#include "../BitArray.h"
#include "PDFCodewordDecoder.h"

namespace ZXing::PDF417 {

Codeword ReadCodeword(BitMatrixModuleCursorF& cur)
{
	auto start = cur.p;
	auto pattern = cur.template readPatternFromBlack<Pattern417>(cur.ms / 2, cur.ms * (17 + MS_THR), cur.ms * (17 - MS_THR));
	auto np = NormalizedPattern<8, 17>(pattern);
	int cluster = CodewordCluster(np);
	int codeword = Pdf417::CodewordDecoder::GetCodeword(ToInt(NormalizedE2EPattern<8, 17>(pattern)));
	return {codeword, cluster, 1, PointT<float>(start), PointT<float>(cur.p)};
}

Codeword ReadCodeword(BitMatrixModuleCursorF& cur, int expectedCluster)
{
	auto start = cur;
	auto cw = ReadCodeword(cur);
	if (!cw || cw.cluster != expectedCluster) {
		for (auto offset : {start.left(), start.right()}) {
			auto curAlt = start.movedBy(cur.ms / 2 * offset);
			if (auto cwAlt = ReadCodeword(curAlt)) {
				if (!cw || cwAlt.cluster == expectedCluster) {
					cw = cwAlt;
					if (cwAlt.cluster == expectedCluster)
						break;
				}
			}
		}
	}
	if (cw)
		cur.ms = dot(cur.p - start.p, mainDirection(cur.d)) / 17.f;

	return cw;
}

bool SkipCodeword(BitMatrixModuleCursorF& cur)
{
	int min = cur.ms * (17 - MS_THR), max = cur.ms * (17 + MS_THR);
	int steps = cur.stepToEdge(8, max);
	int totalSteps = steps;
	while (totalSteps < min && steps) {
		steps = cur.stepToEdge(2, max - totalSteps);
		totalSteps += steps;
	}
	cur.ms = totalSteps / 17.f;
	return totalSteps >= min && max > 0;
}

} // namespace ZXing::PDF417
