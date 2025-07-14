/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "inifile_op.hpp"
#include "color_abgr.hpp"
namespace gdi = sigma_lib::W32::GDI;

#include "enhanced_tl.hpp"
#include "timeline.hpp"
#include "layer_resize.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// レイヤー幅の動的変更．
////////////////////////////////
using namespace enhanced_tl::layer_resize;

// helper functions for variants of scales.
inline static double zoom_from_val(int32_t value)
{
	auto const& beh = settings.behavior;
	switch (beh.scale) {
		using enum Settings::Scale;
	case linear:
	default:
		return (value - beh.size_min) / double(beh.size_max - beh.size_min);
	case logarithmic:
		return std::log(value / double(beh.size_min)) / std::log(beh.size_max / double(beh.size_min));
	case harmonic:
		return (value - beh.size_min) / double(beh.size_max - beh.size_min) * beh.size_max / value;
	}
}
inline static int32_t zoom_to_val(double pos)
{
	auto const& beh = settings.behavior;
	return std::lround([&] {
		switch (beh.scale) {
			using enum Settings::Scale;
		case linear:
		default:
			return pos * (beh.size_max - beh.size_min) + beh.size_min;
		case logarithmic:
			return std::exp(pos * std::log(beh.size_max / double(beh.size_min))) * beh.size_min;
		case harmonic:
			return beh.size_min * beh.size_max / (pos * (beh.size_min - beh.size_max) + beh.size_max);
		}
	}());
}


////////////////////////////////
// データ授受の中核クラス．
////////////////////////////////
struct layer_sizes {
	int32_t L, M, S;
	int32_t& operator[](int32_t i) {
		return i > 0 ? i > 1 ? S : M : L;
	}
};
static constinit struct GuiData {
	int32_t mode_curr = 0;
	int32_t size_curr = 0, size_pend = 0;
	layer_sizes def_layer_sizes{ 0, 0, 0 };

	void exit() {
		kill_timer();
	}
	// returns whether the state had changed by pulling data.
	bool pull()
	{
		kill_timer();

		if (auto mode_prev = std::exchange(mode_curr, *exedit.layer_height_mode);
			mode_prev != mode_curr) {
			size_curr = exedit.layer_height_preset[mode_curr];
			return true;
		}

		if (auto size_prev = std::exchange(size_curr, exedit.layer_height_preset[mode_curr]);
			size_prev != size_curr) {
			return true;
		}

		return false;
	}
	// returns whether the new value is (or is being) applied.
	bool push(int32_t size_new, int delay)
	{
		if (size_pend == size_new) return false;
		if (size_curr == size_new) {
			kill_timer();
			return false;
		}

		if (delay <= 0) return push_core(size_new);

		size_pend = size_new;
		::SetTimer(enhanced_tl::this_fp->hwnd, timer_id(), delay, on_timer);
		return true;
	}
	bool flush()
	{
		if (!is_timer_active()) return false;

		::KillTimer(enhanced_tl::this_fp->hwnd, timer_id());
		on_timer();
		return true;
	}
	bool set_mode(int32_t mode)
	{
		if (mode_curr == mode) return false;
		flush();

		mode_curr = *exedit.layer_height_mode = mode;
		*exedit.midpt_marker_size = exedit.midpt_mk_sz_preset[mode];
		return push_core(exedit.layer_height_preset[mode]);
	}

	void load(int32_t prev_L, int32_t prev_M, int32_t prev_S) {
		def_layer_sizes = save();

		if (prev_L > 0) exedit.layer_height_preset[0] = prev_L;
		if (prev_M > 0) exedit.layer_height_preset[1] = prev_M;
		if (prev_S > 0) exedit.layer_height_preset[2] = prev_S;

		*exedit.layer_height = exedit.layer_height_preset[*exedit.layer_height_mode];
	}
	static layer_sizes save() {
		return {
			.L = exedit.layer_height_preset[0],
			.M = exedit.layer_height_preset[1],
			.S = exedit.layer_height_preset[2],
		};
	}

	constexpr int32_t gui_value() const { return is_timer_active() ? size_pend : size_curr; }

private:
	uintptr_t timer_id() const { return reinterpret_cast<uintptr_t>(this); }
	constexpr bool is_timer_active() const { return size_pend > 0; }
	void kill_timer() {
		if (!is_timer_active()) return;

		::KillTimer(enhanced_tl::this_fp->hwnd, timer_id());
		size_pend = 0;
	}
	static void CALLBACK on_timer(HWND hwnd, UINT, UINT_PTR id, DWORD)
	{
		::KillTimer(hwnd, id);
		if (auto that = reinterpret_cast<GuiData*>(id);
			that != nullptr && that->is_timer_active() && hwnd == enhanced_tl::this_fp->hwnd)
			that->on_timer();
	}
	void on_timer()
	{
		push_core(std::exchange(size_pend, 0));
	}

	bool push_core(int32_t size_new)
	{
		if (mode_curr != *exedit.layer_height_mode) return pull();
		if (size_curr == size_new) return false;

		*exedit.layer_height = exedit.layer_height_preset[mode_curr] = size_curr = size_new;
		recalc_window_size();

		return true;
	}

	int32_t prev_tl_ht1 = -1, prev_tl_ht2 = -1;
	void recalc_window_size()
	{
		// promote redraw.
		::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);

		// resize the window to fit with integral number of layers.
		RECT rc;
		::GetWindowRect(exedit.fp->hwnd, &rc);

		if (prev_tl_ht2 == rc.bottom - rc.top)
			// if the height didn't change from the last adjustment,
			// restore the previous original height.
			rc.bottom = rc.top + prev_tl_ht1;
		else prev_tl_ht1 = rc.bottom - rc.top; // backup the original height otherwise.

		// let exedit window adjust the window height.
		::SendMessageW(exedit.fp->hwnd, WM_SIZING, WMSZ_BOTTOM, reinterpret_cast<LPARAM>(&rc));
		prev_tl_ht2 = rc.bottom - rc.top; // backup the adjusted height.

		// then change the window size.
		::SetWindowPos(exedit.fp->hwnd, nullptr, 0, 0,
			rc.right - rc.left, rc.bottom - rc.top, SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOZORDER);
	}
} gui_data;


////////////////////////////////
// ツールチップ．
////////////////////////////////
static struct {
	HWND hwnd = nullptr;
	constexpr bool is_active() const { return hwnd != nullptr; }

	// create and initialize a tooltip.
	void init()
	{
		if (!settings.tooltip.show || is_active()) return;

		hwnd = ::CreateWindowExW(WS_EX_TOPMOST | WS_EX_TOOLWINDOW, TOOLTIPS_CLASSW, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP,
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			enhanced_tl::this_fp->hwnd, nullptr, enhanced_tl::this_fp->hinst_parent, nullptr);

		info.hwnd = enhanced_tl::this_fp->hwnd;
		info.hinst = enhanced_tl::this_fp->hinst_parent;
		info.lpszText = const_cast<wchar_t*>(L"");
		info.uId = reinterpret_cast<uintptr_t>(info.hwnd);

		::SendMessageW(hwnd, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&info));

		// required for multi-line tooltips.
		::SendMessageW(hwnd, TTM_SETMAXTIPWIDTH, 0, (1 << 15) - 1);
	}
	// destroy the tooltip if exists.
	void exit()
	{
		if (is_active()) {
			::DestroyWindow(hwnd);
			hwnd = nullptr;
		}
	}

	// turn the tooltip visible.
	void show(int x, int y, int32_t value) {
		if (!is_active()) return;

		change(value); // change the content first, or the tooltip is kept hidden otherwise.
		move(x, y);
		::SendMessageW(hwnd, TTM_TRACKACTIVATE, TRUE, reinterpret_cast<LPARAM>(&info));
	}
	// turn the tooltip hidden.
	void hide() const {
		if (!is_active()) return;

		::SendMessageW(hwnd, TTM_TRACKACTIVATE, FALSE, reinterpret_cast<LPARAM>(&info));
	}
	// change the position.
	void move(int x, int y) const {
		if (!is_active()) return;

		POINT pt{ x, y };
		::ClientToScreen(info.hwnd, &pt);
		::SendMessageW(hwnd, TTM_TRACKPOSITION, 0,
			((pt.x + settings.tooltip.offset_x) & 0xffff) | ((pt.y + settings.tooltip.offset_y) << 16));
	}
	// change the content string.
	void change(int32_t value) {
		if (!is_active()) return;

		wchar_t buf[16]; ::swprintf_s(buf, L"%d", value);
		info.lpszText = buf;
		::SendMessageW(hwnd, TTM_UPDATETIPTEXTW, 0, reinterpret_cast<LPARAM>(&info));
	}

private:
	// using TTTOOLINFOW_V1_SIZE to allow TTF_TRACK work for common controls version < 6.
	TTTOOLINFOW info{
		.cbSize = TTTOOLINFOW_V1_SIZE /* sizeof(info) */,
		.uFlags = TTF_IDISHWND | TTF_TRACK | TTF_ABSOLUTE | TTF_TRANSPARENT,
	};
} tooltip;


////////////////////////////////
// ドラッグ操作の補助クラス．
////////////////////////////////
static constinit struct {
	bool active = false;
	int32_t drag_pos = 0; // client coordinate, clamped.
	int32_t prev_size = 0;

	int prev_x = 0, prev_y = 0;

	void mouse_down(int x, int y, bool& redraw)
	{
		if (active) return;
		prev_x = x; prev_y = y;

		// backup the starting state.
		prev_size = gui_data.gui_value();

		// transform the position to the value.
		auto [p, s] = get_pos_zoom(x, y);
		drag_pos = p;

		// start dragging.
		active = true;
		::SetCapture(enhanced_tl::this_fp->hwnd);

		// push the mouse position.
		auto value = zoom_to_val(s);
		gui_data.push(value, 0);
		redraw = true;

		// show the tooltip.
		tooltip.show(x, y, value);

		// hide the cursor if specified so.
		if (settings.behavior.hide_cursor)
			::SetCursor(nullptr);
	}

	void mouse_up(int x, int y, bool& redraw)
	{
		if (!active) return;

		// finish dragging.
		active = false;
		::ReleaseCapture();

		// make the dragging take effect.
		gui_data.flush();
		redraw = true;

		// hide the tooltip.
		tooltip.hide();
	}

	void mouse_move(int x, int y, bool& redraw)
	{
		if (!active) return;
		if (x == prev_x && y == prev_y) return;
		prev_x = x; prev_y = y;
		tooltip.move(x, y);

		// transform the position to the value.
		auto [p, s] = get_pos_zoom(x, y);
		if (drag_pos == p) return;
		drag_pos = p;

		// push the mouse position.
		auto value = zoom_to_val(s);
		gui_data.push(value, settings.behavior.delay_ms);
		redraw = true;

		// update the tooltip.
		tooltip.change(value);
	}

	void cancel(bool& redraw)
	{
		// either user right-clicked or mouse capture is unexpectedly lost.
		if (!active) return;
		active = false;
		if (::GetCapture() == enhanced_tl::this_fp->hwnd) ::ReleaseCapture();

		// rewind to the state at the drag start.
		gui_data.push(prev_size, 0);
		redraw = true;

		// hide the tooltip.
		tooltip.hide();
	}

private:
	static std::pair<int, double> pt_to_pos_zoom(int x, int y, int w, int h)
	{
		int v, l;
		switch (settings.layout.orientation) {
			using enum Settings::Orientation;
		default:
		case L2R: v = x; l = +w; break;
		case R2L: v = x; l = -w; break;
		case T2B: v = y; l = +h; break;
		case B2T: v = y; l = -h; break;
		}

		v = std::clamp(v, 0, l < 0 ? -l : l);
		double pos = v / static_cast<double>(l);
		if (l < 0) pos += 1.0;

		return { v, pos };
	}
	static std::tuple<int, int, int, int> get_xywh(int x, int y)
	{
		RECT rc; ::GetClientRect(enhanced_tl::this_fp->hwnd, &rc);
		return {
			x - settings.layout.margin_x, y - settings.layout.margin_y,
			std::max<int>(rc.right - 2 * settings.layout.margin_x, 1),
			std::max<int>(rc.bottom - 2 * settings.layout.margin_y, 1)
		};
	}
	static std::pair<int, double> get_pos_zoom(int x, int y) {
		auto [X, Y, w, h] = get_xywh(x, y);
		return pt_to_pos_zoom(X, Y, w, h);
	}
} drag_state;


////////////////////////////////
// コンテキストメニュー表示．
////////////////////////////////
static inline bool contextmenu(HWND hwnd, int x, int y)
{
	// prepare a context menu.
	HMENU menu = ::CreatePopupMenu();

	enum id : uintptr_t {
		to_L = 1,
		to_M,
		to_S,
		reset,
		reset_all,
	};
	constexpr uint32_t flag_unsel = MF_STRING | MF_UNCHECKED,
		flag_sel = MF_STRING | MF_CHECKED | MFT_RADIOCHECK;
	wchar_t buf[16];
	::swprintf_s(buf, L"大 (%d)", exedit.layer_height_preset[0]);
	::AppendMenuW(menu, gui_data.mode_curr == 0 ? flag_sel : flag_unsel, id::to_L, buf);
	::swprintf_s(buf, L"中 (%d)", exedit.layer_height_preset[1]);
	::AppendMenuW(menu, gui_data.mode_curr == 1 ? flag_sel : flag_unsel, id::to_M, buf);
	::swprintf_s(buf, L"小 (%d)", exedit.layer_height_preset[2]);
	::AppendMenuW(menu, gui_data.mode_curr == 2 ? flag_sel : flag_unsel, id::to_S, buf);
	::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
	::AppendMenuW(menu, MF_STRING, id::reset, L"リセット");
	::AppendMenuW(menu, MF_STRING, id::reset_all, L"全てリセット");

	// show the context menu.
	uintptr_t id = ::TrackPopupMenu(menu, TPM_NONOTIFY | TPM_RETURNCMD | TPM_RIGHTBUTTON,
		x, y, 0, hwnd, nullptr);
	::DestroyMenu(menu);

	// process the returned command.
	switch (id) {
	case id::to_L: return gui_data.set_mode(0);
	case id::to_M: return gui_data.set_mode(1);
	case id::to_S: return gui_data.set_mode(2);

	case id::reset:
	on_reset_current:
		// rewind the size of the currently selected mode.
		return gui_data.push(gui_data.def_layer_sizes[gui_data.mode_curr], 0);

	case id::reset_all:
	{
		// rewind the sizes of all modes.
		for (int i : {0, 1, 2}) {
			if (i != gui_data.mode_curr)
				exedit.layer_height_preset[i] = gui_data.def_layer_sizes[i];
		}
		goto on_reset_current;
	}
	}

	return false;
}


////////////////////////////////
// GUI 描画．
////////////////////////////////
static constexpr bool is_rect_empty(RECT const& rc) {
	return rc.left == rc.right || rc.top == rc.bottom;
}
static inline void grad_fill2(RECT const& rc1, gdi::Color c11, gdi::Color c12,
	RECT const& rc2, gdi::Color c21, gdi::Color c22, bool vert, HDC dc)
{
	constexpr GRADIENT_RECT gr[] = { {.UpperLeft = 0, .LowerRight = 1 }, {.UpperLeft = 2, .LowerRight = 3 }, };
	constexpr auto tv = [](int x, int y, gdi::Color c) -> TRIVERTEX {
		return { .x = x, .y = y, .Red = static_cast<uint16_t>(c.R << 8), .Green = static_cast<uint16_t>(c.G << 8), .Blue = static_cast<uint16_t>(c.B << 8) };
	};
	TRIVERTEX v[] = {
		tv(rc1.left, rc1.top, c11), tv(rc1.right, rc1.bottom, c12),
		tv(rc2.left, rc2.top, c21), tv(rc2.right, rc2.bottom, c22),
	};

	// somehow, ::GdiGradientFill() is erroneous when some of the rectangles are empty.
	size_t ofs = 0, cnt = std::size(gr);
	if (is_rect_empty(rc1)) { ofs = cnt = 1; }
	else if (is_rect_empty(rc2)) { cnt = 1; }
	::GdiGradientFill(dc, v, std::size(v), const_cast<GRADIENT_RECT*>(gr) + ofs, cnt,
		vert ? GRADIENT_FILL_RECT_V : GRADIENT_FILL_RECT_H);
}
static inline void draw_gauge(int pos, RECT const& bound, HDC dc)
{
	if (is_rect_empty(bound)) return;

	// determine the filling region and the orientation.
	RECT fill{ bound }, back{ bound };
	bool vert;

	switch (settings.layout.orientation) {
		using enum Settings::Orientation;
	default:
	case L2R: fill.right = back.left = pos + bound.left; vert = true; break;
	case R2L: fill.left = back.right = pos + bound.left; vert = true; break;
	case T2B: fill.bottom = back.top = pos + bound.top; vert = false; break;
	case B2T: fill.top = back.bottom = pos + bound.top; vert = false; break;
	}

	// fill with ::GdiGradientFill().
	grad_fill2(fill, settings.color.fill_lt, settings.color.fill_rb,
		back, settings.color.back_lt, settings.color.back_rb, vert, dc);
}
static inline void draw()
{
	// get the drawing area.
	RECT rc; ::GetClientRect(enhanced_tl::this_fp->hwnd, &rc);
	rc.left += settings.layout.margin_x; rc.right -= settings.layout.margin_x; rc.right = std::max(rc.left + 1, rc.right);
	rc.top += settings.layout.margin_y; rc.bottom -= settings.layout.margin_y; rc.bottom = std::max(rc.top + 1, rc.bottom);
	auto const w = rc.right - rc.left, h = rc.bottom - rc.top;

	// determine the position of the border.
	int gauge_pos;
	if (drag_state.active) gauge_pos = drag_state.drag_pos;
	else {
		auto pos = std::clamp(zoom_from_val(gui_data.gui_value()), 0.0, 1.0);
		constexpr auto rnd = [](double x) { return static_cast<int>(std::round(x)); };
		switch (settings.layout.orientation) {
			using enum Settings::Orientation;
		default:
		case L2R: gauge_pos = rnd(w * pos); break;
		case R2L: gauge_pos = rnd(w * (1 - pos)); break;
		case T2B: gauge_pos = rnd(h * pos); break;
		case B2T: gauge_pos = rnd(h * (1 - pos)); break;
		}
	}

	// as the drawing is such simple we don't need double buffering.
	HDC dc = ::GetDC(enhanced_tl::this_fp->hwnd);
	draw_gauge(gauge_pos, rc, dc);
	::ReleaseDC(enhanced_tl::this_fp->hwnd, dc);
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::layer_resize;

void expt::Settings::load(char const* ini_file) {
	using namespace sigma_lib::inifile;
	constexpr int
		margin_min = 0, margin_max = 64,
		offset_min = -128, offset_max = 127,
		size_min = 4, size_max = 255,
		delay_min = USER_TIMER_MINIMUM, delay_max = 1000,
		wheel_min = -1, wheel_max = 1;

#define section_head "layer_resize."
#define read(func, hd, fld, ...)	hd.fld = read_##func(hd.fld, ini_file, section_head #hd, #fld __VA_OPT__(,) __VA_ARGS__)

	read(color,	color, fill_lt);
	read(color,	color, fill_rb);
	read(color,	color, back_lt);
	read(color,	color, back_rb);

	read(int,	layout, orientation);
	read(int,	layout, margin_x, margin_min, margin_max);
	read(int,	layout, margin_y, margin_min, margin_max);

	read(bool,	tooltip, show);
	read(int,	tooltip, offset_x, offset_min, offset_max);
	read(int,	tooltip, offset_y, offset_min, offset_max);
	read(color,	tooltip, text_color);

	read(int,	behavior, scale);
	read(int,	behavior, size_min, size_min, size_max);
	read(int,	behavior, size_max, size_min, size_max);
	read(bool,	behavior, hide_cursor);
	read(int,	behavior, delay_ms, USER_TIMER_MINIMUM, 1'000);
	read(int,	behavior, wheel, wheel_min, wheel_max);

#undef read

	// make sure that size_min < size_max.
	if (behavior.size_min >= behavior.size_max) {
		if (behavior.size_min > size_min) behavior.size_min--;
		behavior.size_max = behavior.size_min + 1;
	}

	// load the previous states.
	int L = read_int(0, ini_file, section_head "states", "L"),
		M = read_int(0, ini_file, section_head "states", "M"),
		S = read_int(0, ini_file, section_head "states", "S");
	if (L > 0) L = std::clamp(L, size_min, size_max);
	if (M > 0) M = std::clamp(M, size_min, size_max);
	if (S > 0) S = std::clamp(S, size_min, size_max);
	gui_data.load(L, M, S);
}

void expt::Settings::save(char const* ini_file) const {
	using namespace sigma_lib::inifile;

	// save the current states.
	auto [L, M, S] = gui_data.save();
	save_int(L, ini_file, section_head "states", "L");
	save_int(M, ini_file, section_head "states", "M");
	save_int(S, ini_file, section_head "states", "S");

#undef section_head
}

bool expt::setup(HWND hwnd, bool initializing)
{
	if (initializing) {
		// first synchronization.
		gui_data.pull();
		tooltip.init();
	}
	else {
		tooltip.exit();
		gui_data.exit();
	}
	return true;
}

bool expt::on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle*)
{
	int delta = 0;
	switch (menu_id) {
	case menu::decr_size: delta = -1; break;
	case menu::incr_size: delta = +1; break;
	case menu::reset_size:
	{
		delta = gui_data.def_layer_sizes[gui_data.mode_curr] - gui_data.gui_value();
		break;
	}
	}
	if (delta == 0) return false;

	// apply the change, with no delay.
	if (gui_data.push(std::clamp<int32_t>(gui_data.gui_value() + delta,
		settings.behavior.size_min, settings.behavior.size_max), 0))
		draw();

	return false;
}

BOOL expt::on_wnd_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	using Message = AviUtl::FilterPlugin::WindowMessage;
	BOOL ret = FALSE; bool redraw = false;
	switch (message) {
	case Message::ChangeWindow:
	case Message::Update:
	{
		// synchronize with the effective value.
		if (enhanced_tl::this_fp->exfunc->is_filter_window_disp(enhanced_tl::this_fp) != FALSE &&
			gui_data.pull()) redraw = true;
		break;
	}

	// drag operations.
	case WM_LBUTTONDOWN:
	{
		drag_state.mouse_down(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_LBUTTONUP:
	{
		drag_state.mouse_up(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_MOUSEMOVE:
	{
		drag_state.mouse_move(static_cast<int16_t>(lparam & 0xffff),
			static_cast<int16_t>(lparam >> 16), redraw);
		break;
	}
	case WM_RBUTTONDOWN:
	case WM_CAPTURECHANGED:
	{
		drag_state.cancel(redraw);
		break;
	}

	// wheel operation.
	case WM_MOUSEWHEEL:
	{
		// invalid when dragging or intentionally turned off.
		if (drag_state.active || settings.behavior.wheel == 0) break;

		auto const amount = static_cast<int16_t>(wparam >> 16);
		if (amount == 0) break;

		auto const delta = amount > 0 ? +settings.behavior.wheel : -settings.behavior.wheel;

		// apply the change, with no delay.
		redraw = gui_data.push(std::clamp<int32_t>(gui_data.gui_value() + delta,
			settings.behavior.size_min, settings.behavior.size_max), 0);
		break;
	}

	// context menu.
	case WM_CONTEXTMENU:
	{
		// determine the position of the context menu.
		POINT pt{ .x = static_cast<int16_t>(lparam & 0xffff), .y = static_cast<int16_t>(lparam >> 16) };
		if (pt.x == -1 && pt.y == -1) {
			// for the case of Shift+F10, show the menu at the gauge position.
			RECT rc; ::GetClientRect(hwnd, &rc);

			rc.left = settings.layout.margin_x; rc.right -= settings.layout.margin_x; rc.right = std::max(rc.left + 1, rc.right);
			rc.top = settings.layout.margin_y; rc.bottom -= settings.layout.margin_y; rc.bottom = std::max(rc.top + 1, rc.bottom);

			pt.x = (rc.left + rc.right) >> 1; pt.y = (rc.top + rc.bottom) >> 1;

			auto pos = std::clamp(zoom_from_val(gui_data.gui_value()), 0.0, 1.0);
			constexpr auto rnd = [](auto x) {return static_cast<int>(std::round(x)); };
			switch (settings.layout.orientation) {
				using enum Settings::Orientation;
			default:
			case L2R: pt.x = rnd((1 - pos) * rc.left + pos * rc.right); break;
			case R2L: pt.x = rnd((1 - pos) * rc.right + pos * rc.left); break;
			case T2B: pt.y = rnd((1 - pos) * rc.top + pos * rc.bottom); break;
			case B2T: pt.y = rnd((1 - pos) * rc.bottom + pos * rc.top); break;
			}

			::ClientToScreen(hwnd, &pt);
		}

		// apply the pending changes.
		gui_data.flush();

		// show the context menu.
		if (contextmenu(hwnd, pt.x, pt.y)) {
			redraw = true;
			ret = TRUE; // notify other plugins of possible changes.
		}

		// discard mouse messages posted to hwnd.
		for (MSG msg; ::PeekMessageW(&msg, hwnd, WM_MOUSEFIRST, WM_MOUSELAST, PM_REMOVE) != FALSE;);
		break;
	}

	case WM_PAINT:
	{
		redraw = true;
		break;
	}

	case WM_NOTIFY:
	{
		// change the text color of the tooltip if specified.
		if (auto const hdr = reinterpret_cast<NMHDR*>(lparam);
			settings.tooltip.text_color.A == 0 &&
			tooltip.hwnd != nullptr && hdr->hwndFrom == tooltip.hwnd) {
			switch (hdr->code) {
			case NM_CUSTOMDRAW:
			{
				if (auto const dhdr = reinterpret_cast<NMTTCUSTOMDRAW*>(hdr);
					(dhdr->nmcd.dwDrawStage & CDDS_PREPAINT) != 0)
					::SetTextColor(dhdr->nmcd.hdc, settings.tooltip.text_color.raw);
				break;
			}
			}
		}
		break;
	}
	}

	if (redraw) draw();
	return ret;
}

bool expt::check_conflict()
{
	if (settings.is_enabled() && warn_conflict(L"layer_resize.auf", nullptr))
		return false;

	return true;
}

int expt::internal::get_layer_size_delayed()
{
	// return the pending-to-change layer size.
	return gui_data.gui_value();
}

void expt::internal::set_layer_size(int size, bool delay)
{
	// set the layer size, with or without delay.
	if (gui_data.push(std::clamp<int32_t>(size,
		settings.behavior.size_min, settings.behavior.size_max),
		delay ? settings.behavior.delay_ms : 0))
		draw(); // redraw if necessary.
}
