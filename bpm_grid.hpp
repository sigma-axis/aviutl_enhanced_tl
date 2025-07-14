/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <algorithm>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>


////////////////////////////////
// BPM グリッドの丸め計算．
////////////////////////////////
namespace enhanced_tl::BPM_Grid
{
	// BPM グリッドの計算補助クラス．
	struct BPM_Grid {
		BPM_Grid(int origin, int beats_numer, int beats_denom);
		/// @brief `origin` を現在の拡張編集の設定値として初期化．
		BPM_Grid(int beats_numer, int beats_denom);

		/// @brief 正しく初期化されたかどうかをチェック．
		constexpr operator bool() const { return fpb_n > 0 && fpb_d > 0; }

		/// @brief フレーム位置から拍数を計算．
		/// フレーム位置 `pos` 上かそれより左にある拍数線のうち，最も大きい拍数を返す．
		/// (1 フレームに複数の拍数線が重なっている状態でも，そのうち最も大きい拍数が対象．)
		int64_t beat_from_pos(int32_t pos) const;

		/// @brief 拍数からフレーム位置を計算．拍数 `beat` で描画される拍数線のフレーム位置を返す．
		int32_t pos_from_beat(int64_t beat) const;

		// the origin of the BPM grid.
		int32_t origin;

	private:
		// "frames per beat" represented as a rational number.
		int64_t fpb_n, fpb_d;
	};
}
