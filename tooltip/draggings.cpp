/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>
#include <bit>
#include <memory>
#include <vector>
#include <set>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include <../str_encodes.hpp>
//#include <../color_abgr.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"
#include "draggings.hpp"

#include "../script_name.hpp"
#include "../mouse_override.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// ドラッグ中のツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;
using sigma_lib::string::encode_sys;

// returns the number of objects in the selection,
// identifying two objects if they belong to the same chain.
static size_t count_selected_chains()
{
	std::set<int> chains{};

	// collect leading objects, and return the resulting size.
	auto const* const objects = *exedit.ObjectArray_ptr;
	for (int i = *exedit.SelectedObjectNum_ptr; --i >= 0;) {
		int const j = exedit.SelectedObjectIndex[i];
		auto const& obj = objects[j];
		chains.insert(obj.index_midpt_leader >= 0 ? obj.index_midpt_leader : j);
	}
	return chains.size();
}

// return the maximum length of the string
// that can be parsed as a multi-byte string properly.
static size_t valid_multibytes(char const* str)
{
	size_t i;
	for (i = 0; str[i] != '\0'; i++) {
		if (::IsDBCSLeadByte(str[i])) {
			// if the next byte is not a trail byte, return the length.
			if (str[i + 1] == '\0') return i;
			i++; // skip the trail byte.
		}
		else if (str[i] < 0) return i; // invalid character.
	}
	return i; // all characters are valid.
}

// describes whether to update the tooltip position only,
// or both the content and the position.
struct update_mode {
	enum : uint8_t {
		none,
		pos,
		both,
	} state;
	//constexpr operator bool() const { return position(); }
	constexpr bool position() const { return state != none; }
	constexpr bool content() const { return state == both; }
	update_mode(bool position, bool content) {
		state = content ? both : position ? pos : none;
	}
};


////////////////////////////////
// ツールチップの表示内容．
////////////////////////////////
struct content {
	int index_object = -1;
	int prev_x{ -1 << 16 }, prev_y{ -1 << 16 };
	std::string text{};

	// determine the content of this tip.
	update_mode needs_update(int x, int y) {
		if (prev_x == x && prev_y == y) return { false, false };
		prev_x = x; prev_y = y;
		if (was_content_updated(true)) {
			text.clear();
			return { true, true }; // update both content and the position.
		}
		return { true, false }; // update the position only.
	}
	//virtual ~content() {} // no derived class has extra fields that need to be freed.

	std::string const& update_text() {
		if (text.empty()) {
			was_content_updated(false);
			text = prepare_text();
		}
		return text;
	}

	virtual POINT pos(int width_screen, int height_screen) const
	{
		POINT pt_cursor = { prev_x, prev_y };
		::ClientToScreen(exedit.fp->hwnd, &pt_cursor);
		return internal::clamp_into_monitor(internal::tooltip_pos(
			pt_cursor.x, pt_cursor.y,
			(*exedit.ObjectArray_ptr)[index_object].layer_disp), 
			width_screen, height_screen, exedit.fp->hwnd);
	}

protected:
	virtual std::string prepare_text() = 0;
	// checks if the content should update.
	// if `peek` is false, the return value is discarded,
	// and the internal old data shall be updated.
	virtual bool was_content_updated(bool peek) = 0;
};

struct content_move : content {
	content_move(int idx, int frame_begin, int layer_disp) {
		index_object = idx;
		former_frame_begin = frame_begin;
		former_layer_disp = layer_disp;
		count_chains = count_selected_chains();
	}
	int former_frame_begin, former_layer_disp, count_chains;

protected:
	std::string prepare_text() override
	{
		auto const& obj = (*exedit.ObjectArray_ptr)[index_object];
		if (obj.frame_begin == former_frame_begin &&
			obj.layer_disp == former_layer_disp)
			// no movements. simple text.
			return "移動なし";
		else {
			// object(s) has moved.
			constexpr char
				pat1[] = "%d 個を移動中",
				pat2[] = "\n%sに %d F (%d.%02d 秒)",
				pat3[] = "\n%sに %d レイヤー";
			char buf[std::bit_ceil(std::max({
				std::size(pat1) + 5 - 2,
				std::size(pat2) + 20 - 10,
				std::size(pat3) + 4 - 4,
			}))];

			// title.
			std::string ret;
			if (count_chains > 1)
				ret.assign(buf, ::sprintf_s(buf, pat1, count_chains));
			else ret.assign("移動中");

			// framewise.
			if (obj.frame_begin != former_frame_begin) {
				int const
					diff_frames = std::abs(obj.frame_begin - former_frame_begin),
					centisec = internal::centisec_converter()(diff_frames);
				ret.append(buf, ::sprintf_s(buf, pat2,
					obj.frame_begin < former_frame_begin ? "左" : "右",
					diff_frames, centisec / 100, centisec % 100));
			}

			// layerwise.
			if (obj.layer_disp != former_layer_disp)
				ret.append(buf, ::sprintf_s(buf, pat3,
					obj.layer_disp < former_layer_disp ? "上" : "下",
					std::abs(obj.layer_disp - former_layer_disp)));

			return ret;
		}
	}

	int prev_frame_begin{ -1 }, prev_layer_disp{ -1 };
	bool was_content_updated(bool peek) override
	{
		auto const& obj = (*exedit.ObjectArray_ptr)[index_object];
		if (peek) return
			prev_frame_begin != obj.frame_begin ||
			prev_layer_disp != obj.layer_disp;
		else {
			prev_frame_begin = obj.frame_begin;
			prev_layer_disp = obj.layer_disp;
			return true;
		}
	}
};

struct content_move_point : content {
	int former_frame, diff_cursor_x;
	POINT pos(int width_screen, int height_screen) const override
	{
		auto const& obj = (*exedit.ObjectArray_ptr)[index_object];
		POINT pt_cursor = {
			diff_cursor_x + tl::point_from_frame(get_curr_frame(obj)),
			prev_y
		};
		::ClientToScreen(exedit.fp->hwnd, &pt_cursor);
		return internal::clamp_into_monitor(internal::tooltip_pos(
			pt_cursor.x, pt_cursor.y, obj.layer_disp),
			width_screen, height_screen, exedit.fp->hwnd);
	}

protected:
	content_move_point(int idx, int x, int former)
		: former_frame{ former }, diff_cursor_x{ x - tl::point_from_frame(former) }
	{
		index_object = idx;
	}

	virtual int get_diff_frame(ExEdit::Object const& obj) const
	{
		return get_curr_frame(obj) - former_frame;
	}
	virtual int get_curr_frame(ExEdit::Object const& obj) const = 0;

	int prev_frame{ -1 };
	bool was_content_updated(bool peek) override
	{
		auto const& obj = (*exedit.ObjectArray_ptr)[index_object];
		if (peek) return
			prev_frame != get_curr_frame(obj);
		else {
			prev_frame = get_curr_frame(obj);
			return true;
		}
	}
};

struct content_move_end : content_move_point {
	int idx_left, idx_right;

protected:
	content_move_end(int idx, int x, int former)
		: content_move_point{ idx, x, former }
	{
		idx_left = (*exedit.ObjectArray_ptr)[idx].index_midpt_leader;
		if (idx_left < 0) idx_left = idx_right = idx;
		else {
			idx_right = idx;
			for (int i = idx_right; i >= 0; i = exedit.NextObjectIdxArray[i])
				idx_right = i;
		}
	}

	std::string prepare_text() override
	{
		auto const* const objects = *exedit.ObjectArray_ptr;
		auto const& obj = objects[index_object];
		constexpr char pat_1[] =
			"%+d F (%c%d.%02d 秒)"
			"\n長さ: %d F (%d.%02d 秒)",
			pat_2[] =
			"%+d F (%c%d.%02d 秒)"
			"\n区間: %d F (%d.%02d 秒)"
			"\n全体: %d F (%d.%02d 秒)";
		char buf[std::bit_ceil(std::max(
			std::size(pat_1) + ((20 - 11) + (12 - 8)),
			std::size(pat_2) + ((20 - 11) + 2 * (12 - 8))))];
		auto to_centisec = internal::centisec_converter();

		// movement.
		int const diff_frame = get_diff_frame(obj),
			diff_centisec = to_centisec(std::abs(diff_frame));
		// final length.
		int const len_frame = obj.frame_end + 1 - obj.frame_begin,
			len_centisec = to_centisec(len_frame);

		if (obj.index_midpt_leader < 0)
			return { buf, static_cast<size_t>(::sprintf_s(buf, pat_1,
				diff_frame, diff_frame >= 0 ? '+' : '-',
				diff_centisec / 100, diff_centisec % 100,
				len_frame, len_centisec / 100, len_centisec % 100)) };
		else {
			// total length.
			int idx_right = index_object;
			for (int i = idx_right; i >= 0; i = exedit.NextObjectIdxArray[i])
				idx_right = i;
			int const
				len_frame_tot = objects[idx_right].frame_end + 1
					- objects[obj.index_midpt_leader].frame_begin,
				len_centisec_tot = to_centisec(len_frame_tot);

			return { buf, static_cast<size_t>(::sprintf_s(buf, pat_2,
				diff_frame, diff_frame >= 0 ? '+' : '-',
				diff_centisec / 100, diff_centisec % 100,
				len_frame, len_centisec / 100, len_centisec % 100,
				len_frame_tot, len_centisec_tot / 100, len_centisec_tot % 100)) };
		}
	}
};

struct content_move_left : content_move_end {
	content_move_left(int idx, int x, int frame_begin, bool left_positive = true)
		: content_move_end{ idx, x, frame_begin }, flip_sign{ left_positive } {}
	bool flip_sign;

protected:
	int get_diff_frame(ExEdit::Object const& obj) const override {
		return (flip_sign ? -1 : +1) * content_move_end::get_diff_frame(obj);
	}
	int get_curr_frame(ExEdit::Object const& obj) const override {
		return obj.frame_begin;
	}
};

struct content_move_right : content_move_end {
	content_move_right(int idx, int x, int frame_end)
		: content_move_end{ idx, x, frame_end } {}

protected:
	int get_curr_frame(ExEdit::Object const& obj) const override {
		return obj.frame_end;
	}
};

struct content_move_mid : content_move_point {
	content_move_mid(int idx_left, int idx_right, int x, int frame_mid)
		: content_move_point{ idx_left, x, frame_mid }, index_next{ idx_right } {}
	int index_next;

protected:
	std::string prepare_text() override
	{
		auto const* const objects = *exedit.ObjectArray_ptr;
		auto const& obj = objects[index_object];
		auto const& next = objects[index_next];
		constexpr char pat[] =
			"%+d F (%c%d.%02d 秒)"
			"\n左: %d F (%d.%02d 秒)"
			"\n右: %d F (%d.%02d 秒)";
		char buf[std::bit_ceil(std::size(pat) + ((20 - 11) + 2 * (18 - 8)))];
		auto to_centisec = internal::centisec_converter();

		// movement.
		int const diff_frame = get_diff_frame(obj),
			diff_centisec = to_centisec(std::abs(diff_frame));
		// final left length.
		int const len_frame_L = next.frame_begin - obj.frame_begin,
			len_centisec_L = to_centisec(len_frame_L);
		// final right length.
		int const len_frame_R = next.frame_end + 1 - next.frame_begin,
			len_centisec_R = to_centisec(len_frame_R);

		// format and return.
		return { buf, static_cast<size_t>(::sprintf_s(buf, pat,
			diff_frame, diff_frame >= 0 ? '+' : '-',
			diff_centisec / 100, diff_centisec % 100,
			len_frame_L, len_centisec_L / 100, len_centisec_L % 100,
			len_frame_R, len_centisec_R / 100, len_centisec_R % 100))
		};
	}
	int get_curr_frame(ExEdit::Object const& obj) const override {
		return obj.frame_end + 1;
	}
};


////////////////////////////////
// 表示形式の選択．
////////////////////////////////
constinit std::unique_ptr<content> curr_tip_content = nullptr;
static update_mode update_tip(int x, int y)
{
	namespace mo = enhanced_tl::mouse_override;
	if (is_editing() &&
		// valid only when the module `mouse_override` is enabled for the timeline.
		mo::settings.timeline.enabled && mo::interop::idx_obj_on_mouse >= 0 &&
		// and must be in the dragging state.
		::GetCapture() == exedit.fp->hwnd) {
		switch (auto const kind = *exedit.timeline_drag_kind) {
		case drag_kind::move_object:
		case drag_kind::move_object_left:
		case drag_kind::move_object_right:
		{
			if (curr_tip_content != nullptr)
				return curr_tip_content->needs_update(x, y);

			// determine the layout for the tooltip.
			if (kind == drag_kind::move_object) {
				curr_tip_content = std::make_unique<content_move>(
					mo::interop::idx_obj_on_mouse,
					mo::interop::former_frame_begin, mo::interop::former_layer_disp);
			}
			else {
				// determine whether grabbing a mid-point.
				auto const* const objects = *exedit.ObjectArray_ptr;
				auto const& obj = objects[mo::interop::idx_obj_on_mouse];
				if (obj.index_midpt_leader >= 0 &&
					(kind == drag_kind::move_object_left ?
						obj.index_midpt_leader != mo::interop::idx_obj_on_mouse :
						exedit.NextObjectIdxArray[mo::interop::idx_obj_on_mouse] >= 0)) {
					// dragging a mid-point.
					int idx_l, idx_r, mid_frame;
					if (kind == drag_kind::move_object_left) {
						idx_r = mo::interop::idx_obj_on_mouse;
						idx_l = obj.index_midpt_leader;
						for (int i = idx_l; i != idx_r; i = exedit.NextObjectIdxArray[i])
							idx_l = i;
						mid_frame = mo::interop::former_frame_begin;
					}
					else {
						idx_l = mo::interop::idx_obj_on_mouse;
						idx_r = exedit.NextObjectIdxArray[idx_l];
						mid_frame = mo::interop::former_frame_end + 1;
					}

					// handle the drag with shift key differently.
					if (*exedit.is_drag_midpt_with_shift != 0) {
						if (kind == drag_kind::move_object_left)
							curr_tip_content = std::make_unique<content_move_right>(
								idx_l, mo::interop::pt_mouse_down.x, mid_frame - 1);
						else
							curr_tip_content = std::make_unique<content_move_left>(
								idx_r, mo::interop::pt_mouse_down.x, mid_frame, false);
					}
					else
						// set the dragging view for mid-point.
						curr_tip_content = std::make_unique<content_move_mid>(
							idx_l, idx_r, mo::interop::pt_mouse_down.x, mid_frame);
				}
				else if (kind == drag_kind::move_object_left)
					curr_tip_content = std::make_unique<content_move_left>(
						mo::interop::idx_obj_on_mouse,
						mo::interop::pt_mouse_down.x, mo::interop::former_frame_begin);
				else // kind == drag_kind::move_object_right
					curr_tip_content = std::make_unique<content_move_right>(
						mo::interop::idx_obj_on_mouse,
						mo::interop::pt_mouse_down.x, mo::interop::former_frame_end);
			}
			return { true, true }; // update both content and the position.
		}
		}
	}

	// deactivate the tooltip if not dragging.
	return { false, nullptr != std::exchange(curr_tip_content, nullptr) };
}

void update_tip_pos(int width_screen, int height_screen) {
	auto const [x, y] = curr_tip_content->pos(width_screen, height_screen);
	::SendMessageW(tip_content::tip, TTM_TRACKPOSITION, 0, (x & 0xffff) | (y << 16));
}
void update_tip_pos(std::string const& text) {
	auto const [w, h] = internal::measure_text(encode_sys::to_wide_str(text), tip_content::tip);
	RECT rc = {
		.left = 0, .top = 0,
		.right = w + internal::pad_right_extra,
		.bottom = h + internal::pad_bottom_extra,
	};
	::MapWindowPoints(tip_content::tip, nullptr, reinterpret_cast<POINT*>(&rc), 2);
	::SendMessageW(tip_content::tip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));
	update_tip_pos(rc.right - rc.left, rc.bottom - rc.top);
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

void expt::Draggings::update(int x, int y)
{
	if (auto const mode = update_tip(x, y);
		mode.content()) {
		// update the tooltip text.
		TTTOOLINFOW ti{
			.cbSize = TTTOOLINFOW_V1_SIZE,
			.hwnd = exedit.fp->hwnd,
			.uId = uid(),
			.hinst = exedit.fp->hinst_parent,
			.lpszText = LPSTR_TEXTCALLBACKW,
		};
		::SendMessageW(tip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));

		// update the position too.
		if (is_editing() && curr_tip_content != nullptr)
			update_tip_pos(curr_tip_content->update_text());
	}
	else if (mode.position()) {
		RECT rc; ::GetWindowRect(tip, &rc);
		update_tip_pos(rc.right - rc.left, rc.bottom - rc.top);
	}
}

void expt::Draggings::on_get_dispinfo(NMTTDISPINFOA& info)
{
	if (!is_editing() || curr_tip_content == nullptr) return;

	// update the text.
	info.lpszText = const_cast<char*>(curr_tip_content->update_text().c_str());
}

bool expt::Draggings::on_activate(int x, int y, bool active)
{
	curr_tip_content = nullptr;
	return !active || (update_tip(x, y).content() && curr_tip_content != nullptr);
}

bool expt::Draggings::should_deactivate(int x, int y)
{
	return curr_tip_content == nullptr;
}
