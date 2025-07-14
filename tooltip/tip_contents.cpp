/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <cmath>
#include <string>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <CommCtrl.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "../enhanced_tl.hpp"
#include "../timeline.hpp"
#include "../tooltip.hpp"
#include "tip_contents.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// ツールチップ表示．
////////////////////////////////
using namespace enhanced_tl::tooltip;

static tip_content* curr_active = nullptr;
static bool tme_sent = false;

static constexpr bool in_rect(int x, int y, RECT const& rc) {
	return rc.left <= x && x < rc.right
		&& rc.top <= y && y < rc.bottom;
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::tooltip;

void expt::tip_content::setup(bool initializing)
{
	if (initializing) {
		// Create the tooltip window.
		tip = ::CreateWindowExW(
			WS_EX_TOPMOST | WS_EX_TOOLWINDOW,
			TOOLTIPS_CLASSW, nullptr,
			WS_POPUP | TTS_NOPREFIX | TTS_ALWAYSTIP |
			(settings.animation ? TTS_NOFADE | TTS_NOANIMATE : 0),
			CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT, CW_USEDEFAULT,
			exedit.fp->hwnd, nullptr, exedit.fp->hinst_parent, nullptr);

		::SetWindowPos(tip, HWND_TOPMOST, 0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

		// settings of delay time for the tooltip.
		::SendMessageW(tip, TTM_SETDELAYTIME, TTDT_INITIAL,	0xffff & settings.delay);
		::SendMessageW(tip, TTM_SETDELAYTIME, TTDT_AUTOPOP,	0xffff & settings.duration);
		::SendMessageW(tip, TTM_SETDELAYTIME, TTDT_RESHOW,	0xffff & (settings.delay / 5));
		// initial values are... TTDT_INITIAL: 340, TTDT_AUTOPOP: 3400, TTDT_RESHOW: 68.

		// allow multiple lines in the tooltip.
		::SendMessageW(tip, TTM_SETMAXTIPWIDTH, 0, (1 << 15) - 1);
	}
	else {
		// terminating the tooltip.
		::DestroyWindow(tip);
		tip = nullptr;
	}
}

void expt::tip_content::setup()
{
	auto const hwnd = exedit.fp->hwnd;
	TTTOOLINFOW ti{
		.cbSize = TTTOOLINFOW_V1_SIZE,
		.uFlags = TTF_TRANSPARENT,
		.hwnd = hwnd,
		.uId = uid(),
		.rect = get_rect(),
		.hinst = exedit.fp->hinst_parent,
		.lpszText = LPSTR_TEXTCALLBACKW,
	};
	setup_core(ti);
	::SendMessageW(tip, TTM_ADDTOOLW, 0, reinterpret_cast<LPARAM>(&ti));
}

void expt::tip_content::relay(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	switch (message) {
	case WM_LBUTTONDOWN:
	case WM_LBUTTONUP:
	case WM_MBUTTONDOWN:
	case WM_MBUTTONUP:
	case WM_MOUSEMOVE:
	case WM_NCMOUSEMOVE:
	case WM_RBUTTONDOWN:
	case WM_RBUTTONUP:
	{
		// relayed messages.
		MSG msg{ .hwnd = hwnd, .message = message, .wParam = wparam, .lParam = lparam };
		::SendMessageW(tip, TTM_RELAYEVENT,
			message == WM_MOUSEMOVE ? ::GetMessageExtraInfo() : 0, reinterpret_cast<LPARAM>(&msg));
		break;
	}
	}
}

bool expt::tip_content::is_active() { return curr_active != nullptr; }
bool expt::tip_content::active() const { return curr_active == this; }

bool expt::tip_content::on_mouse_move(int x, int y, bool leaving)
{
	if (leaving) tme_sent = false;
	if (curr_active == nullptr) return false;

	if (leaving || curr_active->should_deactivate(x, y)) {
		if (auto* const tracking = dynamic_cast<tip_content_tracking*>(curr_active);
			tracking != nullptr) {
			tracking->activate(x, y, false);
		}
		else curr_active = nullptr;
		return false;
	}

	curr_active->update(x, y);
	return true;
}

void expt::tip_content::on_resize()
{
	// change the tooltip tool area.
	TTTOOLINFOW ti{
		.cbSize = TTTOOLINFOW_V1_SIZE,
		.hwnd = exedit.fp->hwnd,
		.uId = uid(),
		.rect = get_rect(),
	};
	::SendMessageW(tip, TTM_NEWTOOLRECTW, 0, reinterpret_cast<LPARAM>(&ti));
}

bool expt::tip_content::should_deactivate(int x, int y)
{
	return !in_rect(x, y, get_rect()) && ::GetCapture() != exedit.fp->hwnd;;
}

void expt::tip_content_semitracking::on_get_dispinfo(NMTTDISPINFOA& info)
{
	if (curr_active == nullptr) {
		curr_active = this;
		on_activate();
	}
	else if (curr_active != this) return;
	on_get_dispinfo_core(info);
}

void expt::tip_content_tracking::activate(int x, int y, bool active)
{
	// notify the derived class.
	if (!on_activate(x, y, active)) return;

	if (active) {
		// enable WM_MOUSELEAVE message.
		if (!tme_sent) {
			TRACKMOUSEEVENT tme{
				.cbSize = sizeof(tme),
				.dwFlags = TME_LEAVE,
				.hwndTrack = exedit.fp->hwnd,
			};
			::TrackMouseEvent(&tme);
			tme_sent = true;
		}

		// update the content.
		curr_active = this;
		update(x, y);
	}
	else curr_active = nullptr;

	// activate or deactivate the tooltip.
	TTTOOLINFOW ti{
		.cbSize = TTTOOLINFOW_V1_SIZE,
		.hwnd = exedit.fp->hwnd,
		.uId = uid(),
	};
	::SendMessageW(tip, TTM_TRACKACTIVATE, active ? TRUE : FALSE, reinterpret_cast<LPARAM>(&ti));
}

void expt::tip_content_tracking::setup_core(TTTOOLINFOW& ti)
{
	ti.uFlags |= TTF_TRACK | TTF_ABSOLUTE;
}

SIZE expt::internal::measure_text(std::wstring const& text, HWND hwnd)
{
	SIZE sz = { 0, 0 };
	if (!text.empty()) {
		HDC const hdc = ::GetDC(hwnd);
		auto const old_font = ::SelectObject(hdc,
			reinterpret_cast<HFONT>(::SendMessageW(hwnd, WM_GETFONT, 0, 0)));
		RECT rc = {};
		::DrawTextW(hdc, text.c_str(), text.size(), &rc,
			DT_CALCRECT | DT_NOPREFIX | DT_NOCLIP | DT_EXTERNALLEADING);
		sz.cx = rc.right - rc.left;
		sz.cy = rc.bottom - rc.top;
		::SelectObject(hdc, old_font);
		::ReleaseDC(hwnd, hdc);
	}
	return sz;
}
