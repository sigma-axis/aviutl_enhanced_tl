/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <bit>
#include <cmath>
#include <string>
#include <memory>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "memory_protect.hpp"
#include "str_encodes.hpp"
#include "inifile_op.hpp"
#include "timeline.hpp"

#include "enhanced_tl.hpp"
#include "context_menu.hpp"
#include "mouse_override.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 右クリックメニュー追加．
////////////////////////////////
using namespace enhanced_tl::context_menu;
namespace tl = enhanced_tl::timeline;
using sigma_lib::string::encode_sys;

static void set_zoom(int level)
{
	namespace mo = enhanced_tl::mouse_override;
	exedit.set_timeline_zoom(level,
		mo::internal::zoom_center_frame(mo::settings.zoom_gauge.zoom_center));
}


////////////////////////////////
// シーン切り替えボタンのメニュー．
////////////////////////////////
namespace scene_commands
{
	enum id : uint32_t {
		settings	= 1096, // 「シーンの設定」
	};
}
constexpr int32_t menu_item_base_idx = 8;

static constinit struct {
	uint32_t menu_id_toggle_alpha = 4096;
	decltype(&::TrackPopupMenu) hook = nullptr, orig = nullptr;

	void setup(decltype(&::TrackPopupMenu) dest)
	{
		// store the function pointer to a variable.
		hook = dest;

		// find a vacant id from the menu (the initial value is vacant in most cases).
		HMENU menu = ::GetSubMenu(*exedit.root_menu_scene, 0);
		int const N = ::GetMenuItemCount(menu);
		for (;; menu_id_toggle_alpha++) {
			for (int i = 0; i < N; i++) {
				if (::GetMenuItemID(menu, i) == menu_id_toggle_alpha)
					goto found_any;
			}
			break;
		found_any:;
		}

		// add a separator if there is not one at the bottom.
		MENUITEMINFOW mi{
			.cbSize = sizeof(mi),
			.fMask = MIIM_FTYPE,
		};
		::GetMenuItemInfoW(menu, N - 1, TRUE, &mi);
		if ((mi.fType & MFT_SEPARATOR) == 0)
			::AppendMenuW(menu, MFT_SEPARATOR, 0, nullptr);

		// add a menu item to toggle the alpha channels of scenes.
		::AppendMenuW(menu, MF_STRING | MF_UNCHECKED,
			menu_id_toggle_alpha, L"アルファチャンネルあり");
	}

	std::pair<BOOL, bool> track_popup_menu(HMENU menu, UINT flags, int x, int y, int nReserved, HWND hwnd, RECT const* prcRect) const
	{
		// update the menu item.
		update_extra_item(menu);

		// call to ::TrackPopupMenu() or ::TrackPopupMenuEx().
		BOOL ret = orig == &::TrackPopupMenu ?
			// no other hooks present. call ::TrackPopupMenuEx() instead.
			track_popup_menu_ex(menu, x, y, hwnd) :
			// the function was already hooked. call that hooking function
			// (no functionality of excluding certain rect).
			orig(menu, flags | TPM_RETURNCMD | TPM_NONOTIFY, x, y, nReserved, hwnd, prcRect);

		// see if the added item was selected.
		bool const extra_selected = ret == menu_id_toggle_alpha;
		if (extra_selected) ret = 0; // make it as if the menu was dismissed.

		// post WM_COMMAND message if TPM_RETURNCMD was not set.
		if ((flags & TPM_RETURNCMD) == 0) {
			if (ret != 0) ::PostMessageW(hwnd, WM_COMMAND, ret & 0xffff, 0);

			// note that if TPM_RETURNCMD is not set,
			// ::TrackPopupMenu() always returns TRUE even if the user dismissed it.
			ret = TRUE;
		}

		return { ret, extra_selected };
	}

private:
	// updates the status of the added menu item.
	void update_extra_item(HMENU menu) const
	{
		int const scene_index = *exedit.current_scene;
		using Flags = ExEdit::SceneSetting::Flag;
		bool const
			has_alpha = has_flag_or(exedit.SceneSettings[scene_index].flag, Flags::Alpha),
			is_enabled = scene_index != 0 && *exedit.curr_scene_len != 0;

		MENUITEMINFOW const mi = {
			.cbSize = sizeof(mi),
			.fMask = MIIM_STATE,
			.fState = static_cast<uint32_t>(
				(has_alpha ? MFS_CHECKED : MFS_UNCHECKED) |
				(is_enabled ? MFS_ENABLED : MFS_DISABLED))
		};
		::SetMenuItemInfoW(menu, menu_id_toggle_alpha, FALSE, &mi);
	}

	// call to ::TrackPopupMenuEx().
	static BOOL track_popup_menu_ex(HMENU menu, int x, int y, HWND hwnd)
	{
		namespace cs = enhanced_tl::timeline::constants;
		constexpr RECT gauge_area{
			.left = 0, .top = 0,
			.right = cs::width_layer_area,
			.bottom = cs::top_zoom_gauge
		};

		// calculate the excluded domain.
		TPMPARAMS tpmp = {
			.cbSize = sizeof(tpmp),
			.rcExclude = gauge_area,
		};
		::MapWindowPoints(exedit.fp->hwnd, nullptr, reinterpret_cast<POINT*>(&tpmp.rcExclude), 2);

		return ::TrackPopupMenuEx(menu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_VERTICAL,
			x, y, hwnd, &tpmp);
	}
} scene_button_hook;

static BOOL toggle_scene_alpha(HWND, WPARAM)
{
	if (*exedit.current_scene == 0 || *exedit.curr_scene_len == 0)
		return FALSE; // not valid state.

	// toggle the flag.
	using Flags = ExEdit::SceneSetting::Flag;
	exedit.SceneSettings[*exedit.current_scene].flag ^= Flags::Alpha;

	// update the timeline.
	::InvalidateRect(exedit.fp->hwnd, nullptr, FALSE);
	return TRUE; // update the main window.
}

static BOOL __stdcall track_popup_menu_hook(HMENU hMenu, UINT uFlags, int x, int y, int nReserved, HWND hWnd, RECT const* prcRect)
{
	auto const [ret, extra_selected] = scene_button_hook.track_popup_menu(
		hMenu, uFlags, x, y, nReserved, hWnd, prcRect);

	// if the selected item is from this plugin, handle it.
	if (extra_selected)
		// post a mesage for a callback.
		::PostMessageW(enhanced_tl::this_fp->hwnd, PrivateMsg::RequestCallback,
			0, reinterpret_cast<LPARAM>(&toggle_scene_alpha));

	return ret;
}


////////////////////////////////
// 拡大率ゲージのメニュー．
////////////////////////////////
static BOOL exedit_wndproc(hook_wnd_proc& next, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	switch(message) {
	case WM_RBUTTONDOWN:
	{
		int const x = static_cast<int16_t>(lparam & 0xffff), y = static_cast<int16_t>(lparam >> 16);
		namespace cs = tl::constants;
		constexpr RECT exclude_client = {
			.left = 0,
			.top = cs::top_zoom_gauge,
			.right = cs::width_layer_area,
			.bottom = cs::top_layer_area
		};

		//if (tl::area_from_point(x, y,
		//	tl::area_obj_detection::no) != tl::timeline_area::zoom_gauge) break;
		if (!(exclude_client.left <= x && x < exclude_client.right
			&& exclude_client.top <= y && y < exclude_client.bottom)) break;

		UINT const flag_disabled = is_editing(editp) ? 0 : MF_DISABLED;
		HMENU const menu = ::CreatePopupMenu();

		// first, add each command item.
		for (auto const& p : settings.zoom_menus) {
			if (p == nullptr) continue;
			int level = &p - settings.zoom_menus;
			::AppendMenuW(menu, MF_STRING | flag_disabled, level + menu_item_base_idx, p->c_str());
		}

		// then append the footer and a separator.
		{
			if (::GetMenuItemCount(menu) > 0) ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);

			constexpr wchar_t header_pat[] = L"現在の拡大率: %d";
			wchar_t header[std::bit_ceil(std::size(header_pat))];
			::swprintf_s(header, header_pat, 1 + *exedit.curr_timeline_zoom_level);
			::AppendMenuW(menu, MF_STRING | MF_DISABLED, 0, header);
		}

		// show the popup menu.
		POINT pt_screen = { x, y }; ::ClientToScreen(exedit.fp->hwnd, &pt_screen);
		TPMPARAMS tpmp = {
			.cbSize = sizeof(tpmp),
			.rcExclude = exclude_client,
		};
		::MapWindowPoints(exedit.fp->hwnd, nullptr, reinterpret_cast<POINT*>(&tpmp.rcExclude), 2);
		int const id = ::TrackPopupMenuEx(menu,
			TPM_LEFTALIGN | TPM_TOPALIGN | TPM_RETURNCMD | TPM_NONOTIFY | TPM_RIGHTBUTTON | TPM_VERTICAL,
			pt_screen.x, pt_screen.y, exedit.fp->hwnd, &tpmp);
		::DestroyMenu(menu);

		// if no item is selected, do nothing.
		if (id == 0 || flag_disabled != 0) return FALSE;

		// apply the zoom.
		set_zoom(id - menu_item_base_idx);
		return FALSE;
	}
	}

	return next(hwnd, message, wparam, lparam, editp, fp);
}
NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::context_menu;

// iterating menu items.
std::pair<int32_t, expt::menu::title_wrapper> expt::menu::iterator::operator*() const
{
	return { idx + menu_item_base_idx, { encode_sys::from_wide_str(*settings.zoom_menus[idx]) } };
}

// loading settings.
void expt::Settings::load(char const* ini_file)
{
	constexpr auto section = "context_menu";

	using namespace sigma_lib::inifile;
#define read(type, fld, ...) fld = read_##type(fld, ini_file, section, #fld __VA_OPT__(,) __VA_ARGS__)

	read(bool,	scene_button);
	read(bool,	zoom_gauge);
	if (zoom_gauge) {
		// load each menu title.
		constexpr char pat[] = "zoom_menu%d";
		constexpr size_t max_len = 0x100;
		char key[std::bit_ceil(std::size(pat))];
		for (size_t i = 0; i < num_levels; i++) {
			::sprintf_s(key, pat, 1 + i);
			auto s = read_string(u8"", ini_file, section, key, max_len);
			if (!s.empty())
				zoom_menus[i] = std::make_unique<std::wstring>(std::move(s));
		}
	}

#undef read
#undef section
}

// initializing.
bool expt::setup(HWND hwnd, bool initializing)
{
	if (!settings.is_enabled()) return false;
	if (initializing) {
		if (settings.zoom_gauge) exedit_hook::manager.add(&exedit_wndproc);

		// manipulate the binary codes.
		namespace memory = sigma_lib::memory;
		auto const exedit_base = reinterpret_cast<uintptr_t>(exedit.fp->dll_hinst);

		// hook a certain call to ::TrackPopupMenu() function where it shows the scene setting menu.
		constexpr ptrdiff_t call_addr_scene_setting =
			0x03d1af; // ff 15 ** ** ** **    call dword ptr ds:[****]
		if (settings.scene_button) {
			// prepare the menu.
			scene_button_hook.setup(&track_popup_menu_hook);

			// replace the call destination.
			scene_button_hook.orig = *memory::replace_api_call(
				reinterpret_cast<void*>(exedit_base + call_addr_scene_setting),
				&scene_button_hook.hook);
		}
	}
	return true;
}

// menu handler.
bool expt::on_menu_command(HWND hwnd, int32_t menu_id, AviUtl::EditHandle* editp)
{
	if (!is_editing(editp)) return false;

	int const level = menu_id - menu_item_base_idx;
	if (level < 0 || level >= Settings::num_levels) return false;

	set_zoom(level);
	return false;
}
