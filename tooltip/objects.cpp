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
#include <array>
#include <string>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include <../str_encodes.hpp>
#include <../monitors.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"
#include "objects.hpp"

#include "../script_name.hpp"
#include "../mouse_override.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// オブジェクトのツールチップ．
////////////////////////////////
using namespace enhanced_tl::tooltip;
namespace tl = enhanced_tl::timeline;
using sigma_lib::string::encode_sys;

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

static std::wstring remove_control_chars(std::wstring const& text)
{
	// remove control characters from the string.
	std::wstring result;
	result.reserve(text.size());
	for (wchar_t c : text) {
		static_assert(!std::signed_integral<decltype(c)>);
		if (c >= 0x20) // only keep printable characters including space.
			result.push_back(c);
	}
	return result;
}

// measures the text size for the timline object.
static SIZE measure_text_for_obj(std::wstring const& text)
{
	HDC const dc = ::GetDC(exedit.fp->hwnd);
	auto const old_font = ::SelectObject(dc, **exedit.load_tl_object_font);

	RECT rc{};
	::DrawTextW(dc, text.c_str(), text.length(), &rc,
		DT_CALCRECT | DT_SINGLELINE | DT_NOPREFIX | DT_NOCLIP | DT_EXTERNALLEADING);
	::SelectObject(dc, old_font);
	::ReleaseDC(exedit.fp->hwnd, dc);

	return { rc.right, rc.bottom };
}


////////////////////////////////
// ツールチップの表示内容．
////////////////////////////////
struct info_piece {
	int x, y;
	std::wstring text;

	constexpr static uint32_t draw_text_options = DT_NOPREFIX | DT_NOCLIP;
	static SIZE measure(HDC dc, std::wstring const& text)
	{
		if (text.empty()) return { 0, 0 };
		RECT rc{ 0, 0, 0, 0 };
		::DrawTextW(dc, text.c_str(), text.length(), &rc,
			DT_CALCRECT | draw_text_options);
		return { rc.right, rc.bottom };
	}
	SIZE measure(HDC dc) const
	{
		return measure(dc, text);
	}
	static void draw(HDC dc, std::wstring const& text, int x, int y, int w, int h)
	{
		if (text.empty()) return;
		RECT rc{ x, y, x + w, y + h };
		::DrawTextW(dc, text.c_str(), text.length(), &rc,
			draw_text_options);
	}
	void draw(HDC dc, int x0, int y0, int w, int h) const
	{
		draw(dc, text, x0 + x, y0 + y, w, h);
	}
};

#ifdef NDEBUG
constinit
#endif
static struct Layout {
	constexpr static int gap_title = 2, indent_below_title = 8;

	int index_object = -1;
	int w = 0, h = 0;
	std::wstring title{};
	info_piece type{}, chain{}, length{}, filters{};

	constexpr bool is_valid() const { return w > 0 && h > 0; }
	constexpr void invalidate(int idx) { index_object = idx; w = h = 0; }
	ExEdit::Object& target() const { return (*exedit.ObjectArray_ptr)[index_object]; }

	// determintes the layout.
	void measure(HDC dc)
	{
		prepare();

		// determine the placement of `title`.
		int x = 0, y = 0; w = 0;
		bool title_visible = false;
		if (auto const sz = info_piece::measure(dc, title);
			sz.cx > 0 && sz.cy > 0) {
			w = std::max<int>(w, x + sz.cx);
			x += indent_below_title;
			y += sz.cy + gap_title;

			title_visible = true;
		}

		// determine the placements of contents below `title`.
		bool any_pieces_visible = false;
		for (info_piece* p : { &type, &chain, &length, &filters }) {
			p->x = x; p->y = y;
			if (auto const sz = p->measure(dc);
				sz.cx > 0 && sz.cy > 0) {
				w = std::max<int>(w, x + sz.cx);
				y += sz.cy;

				any_pieces_visible = true;
			}
		}

		// remove the pre-added gap if there's no `info_piece` below.
		if (title_visible && !any_pieces_visible) y -= gap_title;

		// determine the width and height.
		w = std::max(w, 1);
		h = std::max(y, 1);
	}

	// draws the content.
	void draw(HDC dc, RECT const& rc) const
	{
		if (!is_valid()) return;

		// draw `title`.
		if (!title.empty())
			info_piece::draw(dc, title, rc.left, rc.top, w, h);

		// draw the contents below `title`.
		for (info_piece const* p : { &type, &chain, &length, &filters }) {
			if (!p->text.empty())
				p->draw(dc, rc.left, rc.top, w - p->x, h - p->y);
		}
	}

private:
	// determine the content of this tip.
	void prepare();
} layout;

inline void Layout::prepare()
{
	// collect fundamental data.
	auto* const objects = *exedit.ObjectArray_ptr;
	auto const& obj = target();
	int pos_chain = 0, num_chain = 0, idx_head = index_object, idx_tail = index_object;
	if (obj.index_midpt_leader >= 0) {
		idx_head = obj.index_midpt_leader;
		for (int j = obj.index_midpt_leader; j >= 0; num_chain++, j = exedit.NextObjectIdxArray[j]) {
			if (j == index_object) pos_chain = num_chain;
			idx_tail = j;
		}
	}
	auto const& head = objects[idx_head]; auto const& tail = objects[idx_tail];
	int const head_frame = head.frame_begin, tail_frame = tail.frame_end + 1;

	int const num_filters = head.countFilters();
	bool const has_out_filter = has_flag_or(
		exedit.loaded_filter_table[head.filter_param[num_filters - 1].id]->flag,
		ExEdit::Filter::Flag::Output);

	// title.
	title.clear();
	if (char const* title_src = head.dispname; title_src != nullptr) {
		if (head.filter_param[0].id == filter_id::scn_chg) {
			// as to scene changes, the name of the script is not displayed.
			auto const filter_name = enhanced_tl::script_name::names{ head, 0 };

			// L"スクリプト名 (フィルター名)"
			title.assign(encode_sys::to_wide_str(filter_name.script))
			.append(L" (").append(encode_sys::to_wide_str(filter_name.filter)).append(L")");
		}
		else {
			// get the object name as a title, removing invalid characters.
			title = remove_control_chars(encode_sys::to_wide_str(
				title_src, valid_multibytes(title_src)));

			constexpr int pad_left = 4; // the padding of the text on the left.
			int const left_px = tl::point_from_frame(head_frame);
			if (left_px >= tl::constants::width_layer_area) {
				// the text does not overflow to the left.
				auto const sz = measure_text_for_obj(title);
				if (sz.cx + pad_left <= std::min<int>(tl::point_from_frame(tail_frame),
					exedit.timeline_size_in_pixels->cx - tl::constants::width_layer_area) - left_px)
					// the text does not overflow to either side, no need to show.
					title.clear();
			}
		}
	}

	// type.
	type.text = encode_sys::to_wide_str(exedit.loaded_filter_table[obj.filter_param[0].id]->name);
	if (has_out_filter)
		type.text.append(L" [").append(encode_sys::to_wide_str(
			exedit.loaded_filter_table[obj.filter_param[num_filters - 1].id]->name)).append(L"]");
	if (!has_flag_or(obj.filter_status[0], ExEdit::Object::FilterStatus::Active))
		type.text.append(L" (無効)");

	// chain.
	chain.text.clear();
	if (obj.index_midpt_leader >= 0) {
		constexpr wchar_t pat[] = L"中間点区間: %d / %d";
		wchar_t buf[std::bit_ceil(std::size(pat) + (5 * 2 - 2 * 2))];
		chain.text.assign(buf,
			::swprintf_s(buf, pat, pos_chain + 1, num_chain));
	}

	// length.
	length.text.clear();
	{
		auto to_centisec = internal::centisec_converter();
		auto append_text = [&](int len_frames, std::wstring_view const& header /* at most 8-len */) {
			int const centisec = to_centisec(len_frames);
			constexpr wchar_t pat[] = L"%s: %d F (%d.%02d 秒)";
			wchar_t buf[std::bit_ceil(std::size(pat) + (8 + 8 + 10 - 2 - 8))];
			length.text.append(buf,
				::swprintf_s(buf, pat, header.data(), len_frames, centisec / 100, centisec % 100));
		};

		append_text(tail_frame - head_frame, L"長さ");
		if (num_chain > 1)
			append_text(obj.frame_end + 1 - obj.frame_begin, L"\n区間長さ");
	}

	// filters.
	filters.text.clear();
	{
		int const extra = num_filters - (has_out_filter ? 2 : 1),
			display = extra <= settings.object_info.max_filters ?
				extra : settings.object_info.max_filters - 1,
			overflows = extra - display;
		for (int i = 0; i < display; i++) {
			if (!filters.text.empty()) filters.text.append(L"\n");

			auto const filter_name = enhanced_tl::script_name::names{ head, 1 + i };
			filters.text.append(L"+");
			if (filter_name.script == nullptr)
				filters.text.append(encode_sys::to_wide_str(filter_name.filter));
			else // L"スクリプト名 (フィルター名)"
				filters.text.append(encode_sys::to_wide_str(filter_name.script))
					.append(L" (").append(encode_sys::to_wide_str(filter_name.filter)).append(L")");

			// show when it's disabled.
			if (!has_flag_or(head.filter_status[1 + i], ExEdit::Object::FilterStatus::Active))
				filters.text.append(L" (無効)");
		}
		if (overflows > 0) {
			if (!filters.text.empty()) filters.text.append(L"\n");

			constexpr wchar_t pat[] = L"+フィルタ効果 %d 個";
			wchar_t buf[std::bit_ceil(std::size(pat) + (2 - 2))];
			filters.text.append(buf, ::swprintf_s(buf, pat, overflows));

			// show the number of disabled filters.
			int disabled = 0;
			for (int i = 0; i < overflows; i++) {
				if (!has_flag_or(head.filter_status[1 + display + i], ExEdit::Object::FilterStatus::Active))
					disabled++;
			}
			if (disabled == overflows)
				filters.text.append(L" (全て無効)");
			else if (disabled > 0)
				filters.text.append(buf, ::swprintf_s(buf, L" (無効 %d 個)", disabled));
		}
	}
}

static bool update_tip(int x, int y)
{
	int const curr_idx = is_editing() ? exedit.obj_from_point(x, y) : -1;
	if (curr_idx != layout.index_object) {
		layout.invalidate(curr_idx);
		return true;
	}
	return false;
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

LRESULT expt::Objects::on_show()
{
	// adjust the tooltip size based on the content.
	RECT rc = {
		.left = 0, .top = 0,
		.right = layout.w + internal::pad_right_extra,
		.bottom = layout.h + internal::pad_bottom_extra,
	};
	::MapWindowPoints(tip, nullptr, reinterpret_cast<POINT*>(&rc), 2);
	::SendMessageW(tip, TTM_ADJUSTRECT, TRUE, reinterpret_cast<LPARAM>(&rc));

	// adjust the position.
	int const width = rc.right - rc.left, height = rc.bottom - rc.top;
	auto pt = internal::clamp_into_monitor(
		internal::tooltip_pos(layout.target().layer_disp),
		width, height, exedit.fp->hwnd);

	// set the position of the tooltip.
	::SetWindowPos(tip, nullptr, pt.x, pt.y, width, height,
		SWP_NOZORDER | SWP_NOACTIVATE);
	return TRUE;
}

LRESULT expt::Objects::on_draw(NMTTCUSTOMDRAW const& info)
{
	switch (info.nmcd.dwDrawStage) {
	case CDDS_PREPAINT:
	{
		// prepare the tooltip content.
		// measure the content.
		if (!layout.is_valid())
			layout.measure(info.nmcd.hdc);

		// actual drawing is done on the CDDS_POSTPAINT notification.
		if ((info.uDrawFlags & DT_CALCRECT) == 0)
			return CDRF_NOTIFYPOSTPAINT;
		break;
	}
	case CDDS_POSTPAINT:
	{
		auto dc = info.nmcd.hdc;

		// draw the content.
		if (layout.is_valid())
			layout.draw(dc, info.nmcd.rc);
		break;
	}
	}
	return CDRF_DODEFAULT;
}

RECT expt::Objects::get_rect()
{
	return {
		.left = tl::constants::width_layer_area,
		.top = tl::constants::top_layer_area,
		.right = exedit.timeline_size_in_pixels->cx,
		.bottom = exedit.timeline_size_in_pixels->cy,
	};
}

void expt::Objects::update(int x, int y)
{
	if (update_tip(x, y)) {
		// update the tooltip text.
		TTTOOLINFOW ti{
			.cbSize = TTTOOLINFOW_V1_SIZE,
			.hwnd = exedit.fp->hwnd,
			.uId = uid(),
			.hinst = exedit.fp->hinst_parent,
			.lpszText = LPSTR_TEXTCALLBACKW,
		};
		::SendMessageW(tip, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&ti));
	}
}

void expt::Objects::on_get_dispinfo_core(NMTTDISPINFOA& info)
{
	if (!is_editing() || layout.index_object < 0) return;

	// fill with a dummy string.
	info.szText[0] = ' '; info.szText[1] = '\0';
}

void expt::Objects::on_activate()
{
	layout.index_object = -1;;
	if (::GetCapture() == nullptr) {
		POINT pt; ::GetCursorPos(&pt);
		::ScreenToClient(exedit.fp->hwnd, &pt);
		update_tip(pt.x, pt.y);
	}
}

bool expt::Objects::should_deactivate(int x, int y)
{
	return ::GetCapture() != nullptr
		|| tip_content_semitracking::should_deactivate(x, y);
}

expt::internal::pos_cand expt::internal::tooltip_pos(int layer)
{
	// the position of the tooltip based on the cursor and the layer of the targeted object.
	POINT pt_cursor; ::GetCursorPos(&pt_cursor);
	return tooltip_pos(pt_cursor.x, pt_cursor.y, layer);
}

expt::internal::pos_cand expt::internal::tooltip_pos(int x_screen, int y_screen, int layer)
{
	// the position of the tooltip based on the given point and the layer of the targeted object.
	int const layer_y = tl::point_from_layer(layer);
	POINT pts_layer[] = {
		{ tl::constants::width_layer_area, layer_y },
		{ tl::constants::width_layer_area, layer_y + *exedit.layer_height },
	};
	::MapWindowPoints(exedit.fp->hwnd, nullptr, pts_layer, std::size(pts_layer));

	constexpr int offset_y_layer = 4;
	return {
		.point = {
			x_screen + settings.offset_x,
			std::max<int>(y_screen + settings.offset_y, pts_layer[1].y + offset_y_layer)
		},
		.exclude_top = pts_layer[0].y - offset_y_layer,
		.exclude_bottom = pts_layer[1].y + offset_y_layer,
	};
}

POINT expt::internal::clamp_into_monitor(pos_cand const& pos, int width, int height, HWND hwnd_monitor)
{
	// get the monitor area.
	using monitor = sigma_lib::W32::monitor<true>;
	monitor const mon = monitor{ hwnd_monitor }.expand(-8); // shrink by 8 pixels.

	// clamp into the monitor area.
	auto [l, r] = mon.clamp_x(pos.point.x, pos.point.x + width);
	auto [t, b] = mon.clamp_y(pos.point.y, pos.point.y + height);

	if (std::max(t, pos.exclude_top) < std::min(b, pos.exclude_bottom)) {
		// the tooltip overlaps the excluded area.
		if (t + b <= pos.exclude_top + pos.exclude_bottom ?
			pos.exclude_top - mon.bound.top >= b - t : // closer upward.
			mon.bound.bottom - pos.exclude_bottom < b - t) // closer downward.
			// move it upward.
			t += pos.exclude_top - std::exchange(b, pos.exclude_top);
		else // or downward.
			b += pos.exclude_bottom - std::exchange(t, pos.exclude_bottom);

		if (std::max(t, pos.exclude_top) < std::min(b, pos.exclude_bottom)) {
			// still overlaps the excluded area.
			if (pos.exclude_top - mon.bound.top >= mon.bound.bottom - pos.exclude_bottom)
				// upward has more space.
				b += mon.bound.top - std::exchange(t, mon.bound.top);
			else // downward has more.
				t += mon.bound.bottom - std::exchange(b, mon.bound.bottom);
		}
	}

	return { l, t };
}

