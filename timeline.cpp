/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <algorithm>
#include <tuple>
#include <vector>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "enhanced_tl.hpp"
#include "timeline.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// タイムライン操作．
////////////////////////////////

// division that rounds toward negative infinity.
// `divisor` is assumed to be positive.
template<std::integral IntT>
constexpr IntT floor_div(IntT dividend, std::integral auto divisor) {
	if constexpr (std::signed_integral<IntT>)
		dividend = dividend < 0 ? dividend - static_cast<IntT>(divisor - 1) : dividend;
	return dividend / static_cast<IntT>(divisor);
}

// returns the index of the right-most object whose beginning point is at `pos` or less,
// `idx_L-1` if couldn't find such.
static int find_nearest_index(int pos, int idx_L, int idx_R)
{
	if (idx_L > idx_R) return idx_L - 1; // 空レイヤー．

	// まず両端のオブジェクトを見る．
	if (exedit.SortedObject[idx_L]->frame_begin > pos) return idx_L - 1;
	if (exedit.SortedObject[idx_R]->frame_begin <= pos) idx_L = idx_R;

	// あとは２分法．
	while (idx_L + 1 < idx_R) {
		auto const idx = (idx_L + idx_R) >> 1;
		if (exedit.SortedObject[idx]->frame_begin <= pos) idx_L = idx; else idx_R = idx;
	}
	return idx_L;
}

#define ToSkip(obj)		(skip_inactives && !enhanced_tl::timeline::is_active(obj))
#define ChainBegin(obj)	(skip_midpoints ? enhanced_tl::timeline::chain_begin(obj) : (obj)->frame_begin)
#define ChainEnd(obj)	(skip_midpoints ? enhanced_tl::timeline::chain_end(obj) : (obj)->frame_end)

// idx 以下のオブジェクトの境界や中間点で，条件に当てはまる pos 以下の点を線形に検索．
static int find_adjacent_left_core(int idx, int idx_L, int pos, bool skip_midpoints, bool skip_inactives)
{
	if (auto const obj = exedit.SortedObject[idx]; !ToSkip(obj))
		return pos < obj->frame_end + 1 ? ChainBegin(obj) : ChainEnd(obj) + 1;

	// 無効でない最も近いオブジェクトを探すのは線形探索しか方法がない．
	while (idx_L <= --idx) {
		// 初期 idx が無効オブジェクトでスキップされたという前提があるため，
		// 最初に見つかった有効オブジェクトの終了点は中間点でなくオブジェクト境界になる．
		// なので skip_midpoints の確認や ChainEnd() 呼び出しは必要ない．
		if (auto const obj = exedit.SortedObject[idx]; enhanced_tl::timeline::is_active(obj))
			return obj->frame_end + 1;
	}
	return 0;
}

// idx 以上のオブジェクトの境界や中間点で，条件に当てはまる pos 以上の点を線形に検索．
static int find_adjacent_right_core(int idx, int idx_R, int pos, bool skip_midpoints, bool skip_inactives)
{
	// 上の関数の左右逆．
	if (auto const obj = exedit.SortedObject[idx]; !ToSkip(obj))
		return pos <= obj->frame_begin ? obj->frame_begin : ChainEnd(obj) + 1;

	while (++idx <= idx_R) {
		if (auto const obj = exedit.SortedObject[idx]; enhanced_tl::timeline::is_active(obj))
			return obj->frame_begin;
	}
	return -1;
}

#undef ToSkip
#undef ChainBegin
#undef ChainEnd

// タイムラインズームサイズの分母．
constexpr int scale_denom = 10'000; // *(double*)(exedit_h + 0x9a548)

// タイムラインスクロールバーの尺度をズームサイズから計算する係数．
constexpr int scroll_step_numerator = 1'000'000,
	scroll_margin_numerator = 960'000;
NS_END


////////////////////////////////
// exported functions.
////////////////////////////////
namespace expt = enhanced_tl::timeline;

int expt::find_left_sorted_index(int pos, int layer)
{
	// 指定された位置か，そこより左にある最も近いオブジェクトの index を探す．
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];
	// pos か，そこより左側に開始点のあるオブジェクトの index を取得．
	int const idx = find_nearest_index(pos, idx_L, idx_R);
	if (idx < idx_L) return -1; // 空レイヤー or 左側に何もない．
	return idx;
}

int expt::find_adjacent_left(int pos, int layer, bool skip_midpoints, bool skip_inactives)
{
	// 指定された位置より左にある最も近い境界を探す．
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];

	// pos より左側に開始点のあるオブジェクトの index を取得．
	int const idx = find_nearest_index(pos - 1, idx_L, idx_R);
	if (idx < idx_L) return 0; // 空レイヤー or 左側に何もない．

	// そこを起点に線形探索してスキップ対象を除外．
	return find_adjacent_left_core(idx, idx_L, pos - 1, skip_midpoints, skip_inactives);
}

// the return value of -1 stands for the end of the scene.
int expt::find_adjacent_right(int pos, int layer, bool skip_midpoints, bool skip_inactives)
{
	// 上の関数の左右逆版．
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];

	// pos の位置かそれより左側に開始点のあるオブジェクトを取得．
	int idx = find_nearest_index(pos, idx_L, idx_R);

	// pos より右側に終了点のあるオブジェクトの index に修正．
	if (idx < idx_L) idx = idx_L;
	else if (exedit.SortedObject[idx]->frame_end + 1 <= pos) idx++;
	if (idx > idx_R) return -1; // 最右端．

	// そこを起点に線形探索してスキップ対象を除外．
	return find_adjacent_right_core(idx, idx_R, pos + 1, skip_midpoints, skip_inactives);
}

std::tuple<int, int> expt::find_interval(int pos, int layer, bool skip_midpoints, bool skip_inactives)
{
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];

	// pos の位置かそれより左側に開始点のあるオブジェクトを取得．
	int idx = find_nearest_index(pos, idx_L, idx_R);

	// まずは左端．
	int const pos_l = idx < idx_L ? 0 :
		find_adjacent_left_core(idx, idx_L, pos, skip_midpoints, skip_inactives);

	// そして右端．
	// pos より右側に終了点のあるオブジェクトの index に修正．
	if (idx < idx_L) idx = idx_L;
	else if (exedit.SortedObject[idx]->frame_end + 1 <= pos) idx++;
	int const pos_r = idx > idx_R ? -1 :
		find_adjacent_right_core(idx, idx_R, pos + 1, skip_midpoints, skip_inactives);

	return { pos_l, pos_r };
}

int expt::object_at_frame(int pos, int layer)
{
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];

	// 候補オブジェクトを特定．
	int const idx = find_nearest_index(pos, idx_L, idx_R);
	if (idx < idx_L) return -1; // no match.

	auto const* const obj = exedit.SortedObject[idx];
	if (obj->frame_end < pos) return -1; // pos を含んでいない．
	return obj - *exedit.ObjectArray_ptr;
}

std::vector<size_t> expt::objects_in_interval(int pos_L, int pos_R, int layer, bool inclusive)
{
	int const
		idx_L = exedit.SortedObjectLayerBeginIndex[layer],
		idx_R = exedit.SortedObjectLayerEndIndex[layer];

	// 左端のオブジェクトを特定．
	int idx_l = find_nearest_index(pos_L, idx_L, idx_R);
	if (idx_l < idx_L) return {}; // 空レイヤー or 左側に何もない．
	if (auto const* const obj = exedit.SortedObject[idx_l];
		(inclusive ? obj->frame_end : obj->frame_begin) < pos_L) {
		// pos_L より左側のオブジェクトは除外．
		idx_l++;
		if (idx_l > idx_R) return {}; // no more objects.
	}

	// 右端のオブジェクトを特定．
	int idx_r = find_nearest_index(pos_R, idx_L, idx_R); // never less than pos_R.
	if (auto const* const obj = exedit.SortedObject[idx_r];
		!inclusive && obj->frame_end >= pos_R)
		// pos_R より右側にはみ出したオブジェクトは除外．
		idx_r--;

	if (idx_l > idx_r) return {}; // no matches.

	// collect indices.
	std::vector<size_t> ret{};
	ExEdit::Object const* const objects = *exedit.ObjectArray_ptr;
	for (int i = idx_l; i <= idx_r; i++)
		ret.push_back(exedit.SortedObject[i] - objects);
	return ret;
}

bool expt::is_active(ExEdit::Object const* obj)
{
	if (auto i = obj->index_midpt_leader; i >= 0) obj = &(*exedit.ObjectArray_ptr)[i];
	return has_flag_or(obj->filter_status[0], ExEdit::Object::FilterStatus::Active);
}
bool expt::is_visible(ExEdit::LayerSetting const& setting)
{
	return !has_flag_or(setting.flag, ExEdit::LayerSetting::Flag::UnDisp);
}
int expt::chain_begin(ExEdit::Object const* obj)
{
	if (auto i = obj->index_midpt_leader; i >= 0) obj = &(*exedit.ObjectArray_ptr)[i];
	return obj->frame_begin;
}
int expt::chain_end(ExEdit::Object const* obj)
{
	if (auto i = obj->index_midpt_leader; i >= 0) {
		int j;
		while (j = exedit.NextObjectIdxArray[i], j >= 0) i = j;
		obj = &(*exedit.ObjectArray_ptr)[i];
	}
	return obj->frame_end;
}

int expt::point_to_frame(int x)
{
	x -= constants::width_layer_area;
	x *= scale_denom;
	x = floor_div(x, std::max(*exedit.curr_timeline_zoom_len, 1));
	x += *exedit.timeline_h_scroll_pos;
	if (x < 0) x = 0;

	return x;
}

int expt::point_from_frame(int f)
{
	f -= *exedit.timeline_h_scroll_pos;
	{
		// possibly overflow; curr_timeline_zoom_len is at most 100'000 (> 2^16).
		int64_t F = f;
		F *= *exedit.curr_timeline_zoom_len;
		F = floor_div(F, scale_denom);
		f = static_cast<int32_t>(F);
	}
	f += constants::width_layer_area;

	return f;
}

int expt::point_to_layer(int y)
{
	y -= constants::top_layer_area;
	y = floor_div(y, std::max(*exedit.curr_timeline_layer_height, 1));
	y += *exedit.timeline_v_scroll_pos;
	y = std::clamp(y, 0, constants::num_layers - 1);

	return y;
}

int expt::point_from_layer(int l)
{
	l -= *exedit.timeline_v_scroll_pos;
	l *= *exedit.curr_timeline_layer_height;
	l += constants::top_layer_area;

	return l;
}

int expt::horiz_scroll_size()
{
	return scroll_step_numerator / std::max(*exedit.curr_timeline_zoom_len, 1);
}

int expt::horiz_scroll_margin()
{
	return scroll_margin_numerator / std::max(*exedit.curr_timeline_zoom_len, 1);
}

expt::timeline_area expt::area_from_point(int x, int y, area_obj_detection mode)
{
	using enum timeline_area;
	namespace cs = enhanced_tl::timeline::constants;
	if (x < cs::width_layer_area) {
		if (y < cs::top_zoom_gauge)
			return scene_button;
		if (y < cs::top_layer_area)
			return zoom_gauge;
		return layer;
	}
	if (y < cs::scrollbar_thick)
		return scrollbar_h;
	if (x < exedit.timeline_size_in_pixels->cx - cs::scrollbar_thick) {
		if (y < cs::top_layer_area)
			return ruler;
		switch (mode) {
		default:
		case area_obj_detection::no:
			return blank;
		case area_obj_detection::exact:
			return object_at_frame(point_to_frame(x), point_to_layer(y)) < 0 ?
				blank : object;
		case area_obj_detection::nearby:
			return exedit.obj_from_point(x, y) < 0 ?
				blank : object;
		}
	}
	return scrollbar_v;
}
