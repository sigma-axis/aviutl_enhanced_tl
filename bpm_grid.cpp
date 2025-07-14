/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "enhanced_tl.hpp"
#include "bpm_grid.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// BPM グリッドの丸め計算．
////////////////////////////////

// division that rounds away from zero.
// `divisor` is assumed to be positive.
template<std::integral IntT>
constexpr IntT away_from_zero_div(IntT dividend, std::integral auto divisor) {
	if constexpr (std::signed_integral<IntT>)
		dividend = dividend > 0 ?
			dividend + static_cast<IntT>(divisor - 1) :
			dividend - static_cast<IntT>(divisor - 1);
	else dividend += static_cast<IntT>(divisor - 1);
	return dividend / static_cast<IntT>(divisor);
}

constexpr int one_bpm = 600'000;
NS_END

////////////////////////////////
// exported functions.
////////////////////////////////
namespace expt = enhanced_tl::BPM_Grid;

expt::BPM_Grid::BPM_Grid(int beats_numer, int beats_denom)
	: BPM_Grid(*exedit.timeline_BPM_frame_origin - 1, beats_numer, beats_denom) {}

expt::BPM_Grid::BPM_Grid(int origin, int beats_numer, int beats_denom)
	: origin{ origin }
{
	// find the frame rate.
	AviUtl::FileInfo fi; // the frame rate is calculated as: fi.video_rate / fi.video_scale.
	if (*exedit.editp == nullptr) fi = {};
	else exedit.fp->exfunc->get_file_info(*exedit.editp, &fi);

	// find the tempo in BPM.
	auto tempo = *exedit.timeline_BPM_tempo;

	// calculate "frames per beat".
	if (fi.video_rate > 0 && fi.video_scale > 0 && tempo > 0) {
		fpb_n = static_cast<int64_t>(fi.video_rate) * (one_bpm * beats_numer);
		fpb_d = static_cast<int64_t>(fi.video_scale) * (tempo * beats_denom);
	}
	else fpb_n = fpb_d = 0;
}

// フレーム位置から拍数を計算．
// フレーム位置 pos 上かそれより左にある拍数線のうち，最も大きい拍数を返す．
// (1 フレームに複数の拍数線が重なっている状態でも，そのうち最も大きい拍数が対象．)
int64_t expt::BPM_Grid::beat_from_pos(int32_t pos) const
{
	// frame -> beat is by rounding toward zero.
	pos -= origin;
	// handle negative positions differently.
	auto beats = (pos < 0 ? -1 - pos : pos) * fpb_d / fpb_n;
	return pos < 0 ? -1 - beats : beats;
}

// 拍数からフレーム位置を計算．
// 拍数 beat で描画される拍数線のフレーム位置を返す．
int32_t expt::BPM_Grid::pos_from_beat(int64_t beat) const
{
	// beat -> frame is by rounding away from zero.
	return static_cast<int32_t>(away_from_zero_div(beat * fpb_n, fpb_d)) + origin;
	auto d = std::div(beat * fpb_n, fpb_d); // std::div() isn't constexpr at this time.
	if (d.rem > 0) d.quot++; else if (d.rem < 0) d.quot--;
	return static_cast<int32_t>(d.quot) + origin;
}
