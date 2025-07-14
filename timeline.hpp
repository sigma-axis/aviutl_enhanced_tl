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
#include <tuple>
#include <vector>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>


////////////////////////////////
// タイムライン操作．
////////////////////////////////
namespace enhanced_tl::timeline
{
	/// @brief 指定された位置か，そこより左から始まる最も近いオブジェクトの `exedit.SortedObject` での位置を探す．各種検索関数の低レベル部分で，二分法を使っているので拡張編集の内部関数より高速．
	/// @param pos 指定位置，フレーム単位．
	/// @param layer 検索対象のレイヤー．
	/// @return 検索結果の `exedit.SortedObject` での位置．存在しない場合は `-1`．
	int find_left_sorted_index(int pos, int layer);

	/// @brief 指定された位置より左にある最も近い境界を探す．
	/// @param pos 指定位置，フレーム単位．
	/// @param layer 検索対象のレイヤー．
	/// @param skip_midpoints 中間点を無視するかどうか．
	/// @param skip_inactives 無効オブジェクトを無視するかどうか．
	/// @return 検索結果のフレーム位置．
	int find_adjacent_left(int pos, int layer, bool skip_midpoints, bool skip_inactives);

	/// @brief 指定された位置より右にある最も近い境界を探す．
	/// @param pos 指定位置，フレーム単位．
	/// @param layer 検索対象のレイヤー．
	/// @param skip_midpoints 中間点を無視するかどうか．
	/// @param skip_inactives 無効オブジェクトを無視するかどうか．
	/// @return 検索結果のフレーム位置．`-1` の場合は最右端．
	int find_adjacent_right(int pos, int layer, bool skip_midpoints, bool skip_inactives);

	/// @brief `find_adjacent_right(int pos, int layer, bool skip_midpoints, bool skip_inactives)` の「`-1` で最右端」を再右端のフレーム位置に補正したラッパー．
	/// @param pos 指定位置，フレーム単位．
	/// @param layer 検索対象のレイヤー．
	/// @param skip_midpoints 中間点を無視するかどうか．
	/// @param skip_inactives 無効オブジェクトを無視するかどうか．
	/// @param len シーンの長さ，フレーム単位．
	/// @return 検索結果のフレーム位置．
	inline int find_adjacent_right(int pos, int layer,
		// 「-1 で最右端」規格がめんどくさいのでラップ．
		bool skip_midpoints, bool skip_inactives, int len) {
		pos = find_adjacent_right(pos, layer, skip_midpoints, skip_inactives);
		return 0 <= pos && pos < len ? pos : len - 1;
	}

	/// @brief 指定フレームの左右にある境界点を検索する．指定フレームはオブジェクトの範囲内とは限らない．
	/// @param pos 指定位置，フレーム単位．
	/// @param layer 検索対象のレイヤー．
	/// @param skip_midpoints 中間点を無視するかどうか．
	/// @param skip_inactives 無効オブジェクトを無視するかどうか．
	/// @return 検索結果の左右のフレーム位置 (`{ L, R }`). `R` が `-1` の場合は最右端．
	std::tuple<int, int> find_interval(int pos, int layer, bool skip_midpoints, bool skip_inactives);

	/// @brief 指定フレームにあるオブジェクトを特定する．
	/// @param pos 指定フレーム．
	/// @param layer 対象レイヤー．
	/// @return オブジェクトのインデックス．`-1` の場合はそのフレームにオブジェクトなし．
	int object_at_frame(int pos, int layer);

	/// @brief 指定フレーム区間の間にあるオブジェクトを列挙する．
	/// @param pos_L 区間の左端フレーム位置．
	/// @param pos_R 区間の右端フレーム位置．
	/// @param layer 検索対象のレイヤー．
	/// @param inclusive 区間の端を越えているオブジェクトも含めるかどうか．`true` で含める，`false` で含めない．
	/// @return 検索結果のオブジェクトのインデックスのリスト．順序は左から．
	std::vector<size_t> objects_in_interval(int pos_L, int pos_R, int layer, bool inclusive);

	/// @brief whether the object is active (not inactivated).
	bool is_active(ExEdit::Object const* obj);
	/// @brief whether the layer is visible (not hidden).
	bool is_visible(ExEdit::LayerSetting const& setting);
	/// @brief returns the beginning frame of the chain of objects.
	int chain_begin(ExEdit::Object const* obj);
	/// @brief returns the final frame of the chain of objects. the next object would start one frame after this.
	int chain_end(ExEdit::Object const* obj);

	// 座標変換．
	/// @brief Converts client x-coordinate to frame number.
	/// @param x the client x-coordinate, may exceed the window area.
	/// @return the corresponding frame. may not be negative, but may exceed the length of the scene.
	int point_to_frame(int x);
	/// @brief Converts frame number to client x-coordinate.
	/// @param f the index of frame. may be negative or exceed the length of the scene.
	/// @return the corresponding client x-coordinate, may exceed the window area.
	int point_from_frame(int f);
	/// @brief Converts client y-coordinate to layer number. the layer number is 0-based.
	/// @param y the client y-coordinate, may exceed the window area.
	/// @return the layer index. clamped to 0--99 range, but may exceed the range visible in the window.
	int point_to_layer(int y);
	/// @brief Calculates the client y-coordinate of the top of the specified layer. the layer number is 0-based. the layer height is obtained by `*exedit.curr_timeline_layer_height`.
	/// @param l the index of the layer, may be negative or exceed the maximum number of layers.
	/// @return the top-most client y-coordinate where the layer is drawn. may exceed the window area.
	int point_from_layer(int l);

	// スクロールバー関連．
	/// @brief delta scroll size of horizontal scrollbar at the current timeline scale.
	int horiz_scroll_size();
	/// @brief the size of the right side margin of the timeline, measured in the scrollbar scale, at the current timeline scale.
	int horiz_scroll_margin();

	namespace constants
	{
		constexpr int
			top_zoom_gauge = 22,
			width_layer_area = 64, top_layer_area = 42,
			scrollbar_thick = 13,
			num_layers = 100, num_scenes = 50,
			num_zoom_levels = 27;
	}

	/// @ brief represents the different areas within a timeline user interface.
	enum class timeline_area : uint8_t {
		object,		// over (maybe near) an object.
		blank,			// timeline area where there's no objects.
		ruler,			// the ruler above the timeline.
		layer,			// the layer area on the left side of the timeline.
		zoom_gauge,		// the zoom gauge above the layer area.
		scene_button,	// the scene button on the top-left.
		scrollbar_h,	// the horizontal scrollbar on the top of the timeline.
		scrollbar_v,	// the vertical scrollbar on the right of the timeline.
	};

	/// @brief Specifies the mode for object detection in `area_from_point()`.
	enum class area_obj_detection {
		no,		// skips detecting objects.
		exact,	// detect objects exactly at the point.
		nearby,	// detect objects near to the point.
	};

	/// @brief Determines the timeline area at a given point.
	/// @param x The client x-coordinate of the point.
	/// @param y The client y-coordinate of the point.
	/// @param mode specifies how objects on the timeline are detected.
	/// @return The timeline_area corresponding to the specified point.
	timeline_area area_from_point(int x, int y, area_obj_detection mode);
}
