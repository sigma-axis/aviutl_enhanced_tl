/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <tuple>
#include <vector>
#include <set>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#pragma comment(lib, "imm32")

using byte = uint8_t;
#include <exedit.hpp>

#include "str_encodes.hpp"

#include "enhanced_tl.hpp"
#include "walkaround.hpp"
#include "layer_resize.hpp"
#include "mouse_override.hpp"
#include "context_menu.hpp"
#include "tooltip.hpp"


////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
bool ExEdit092::init(AviUtl::FilterPlugin* this_fp)
{
	constexpr char const* info_exedit092 = "拡張編集(exedit) version 0.92 by ＫＥＮくん";

	if (fp != nullptr) return true;
	AviUtl::SysInfo si; this_fp->exfunc->get_sys_info(nullptr, &si);
	for (int i = 0; i < si.filter_n; i++) {
		auto that_fp = this_fp->exfunc->get_filterp(i);
		if (that_fp->information != nullptr &&
			0 == std::strcmp(that_fp->information, info_exedit092)) {
			fp = that_fp;
			init_pointers(fp->dll_hinst, fp->hinst_parent);
			return true;
		}
	}
	return false;
}


////////////////////////////////
// 各機能に共有の関数実装．
////////////////////////////////

bool is_editing()
{
	return is_editing(*exedit.editp);
}

bool is_editing(AviUtl::EditHandle* editp)
{
	return editp != nullptr
		&& exedit.fp->exfunc->is_editing(editp)
		&& !exedit.fp->exfunc->is_saving(editp);
}

// 複数選択オブジェクトの取得．
std::set<int32_t> get_multi_selected_objects()
{
	// store the selected index to std::set<int32_t>.
	return { exedit.SelectedObjectIndex, exedit.SelectedObjectIndex + *exedit.SelectedObjectNum_ptr };
}

// 複数選択オブジェクトの設定．
void set_multi_selected_objects(std::set<int32_t> const& sel)
{
	// copy back the selected index to exedit.
	std::copy(sel.begin(), sel.end(), exedit.SelectedObjectIndex);
	*exedit.SelectedObjectNum_ptr = sel.size();
}

// 設定ダイアログのデータ表示を更新．
void update_setting_dialog(int index)
{
	auto hdlg = *exedit.hwnd_setting_dlg;
	::SendMessageW(hdlg, WM_SETREDRAW, FALSE, {});
	exedit.update_setting_dlg(index);
	::SendMessageW(hdlg, WM_SETREDRAW, TRUE, {});
	::UpdateWindow(hdlg);
}

// タイムラインウィンドウのフック．
BOOL hook_wnd_proc::operator()(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp) const
{
	if (next != nullptr)
		return proc(*next, hwnd, message, wparam, lparam, editp, fp);
	else return reinterpret_cast<func_wnd_proc*>(proc)(
		hwnd, message, wparam, lparam, editp, fp);
}

void hook_manager::init(hook_wnd_proc::func_wnd_proc* original)
{
	hooks.clear();
	hooks.emplace_back(original);
}

void hook_manager::add(hook_wnd_proc::wnd_proc* proc)
{
	hooks.emplace_back(proc, &hooks.back());
	for (size_t i = 1; i < hooks.size(); i++)
		hooks[i].next = &hooks[i - 1];
}

BOOL hook_manager::operator()(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp) const
{
	return hooks.back()(hwnd, message, wparam, lparam, editp, fp);
}

// 編集データの変更でメイン画面を更新．
void update_current_frame()
{
	constexpr UINT msg_update_main = WM_USER + 1;
	::PostMessageW(exedit.fp->hwnd, msg_update_main, 0, 0);
}

// 競合通知メッセージ．
bool warn_conflict(wchar_t const* module_name, wchar_t const* ini_piece)
{
	if (::GetModuleHandleW(module_name) == nullptr) return false;
#pragma warning(suppress : 6387) // suppressing the warning that insists `module_name` might be null.
	auto mes = std::wstring{ module_name }; UINT icon;
	if (ini_piece != nullptr) {
		mes += L" と競合しているため一部機能を無効化しました．\n"
			"次回以降このメッセージを表示させないためには，設定ファイルで以下の設定をしてください:\n\n";
		mes += ini_piece;
		icon = MB_ICONEXCLAMATION;
	}
	else {
		mes += L" と競合しているため，このプラグインの動作を続行できません．\n"
			"このプラグインか当該プラグインのどちらかを削除してください．";
		icon = MB_ICONERROR;
	}

	::MessageBoxW(exedit.fp->hwnd_parent, mes.c_str(),
		sigma_lib::string::encode_sys::to_wide_str(enhanced_tl::this_fp->name).c_str(), MB_OK | icon);
	return true;
}


////////////////////////////////
// ファイルパス操作．
////////////////////////////////
template<class T, size_t len_max, size_t len_old, size_t len_new>
static void replace_tail(T(&dst)[len_max], size_t len, T const(&tail_old)[len_old], T const(&tail_new)[len_new])
{
	// replaces a file extension when it's known.
	if (len < len_old || len - len_old + len_new > len_max) return;
	std::memcpy(dst + len - len_old, tail_new, len_new * sizeof(T));
}


////////////////////////////////
// 各種コマンド．
////////////////////////////////
namespace Menu
{
	enum category : int32_t {
		walkaround = 1,
		layer_resize = 2,
		context_menu = 3,
	};
	constexpr int category_bits = 8;

	static constexpr int32_t combine_cat_id(category cat, auto id) {
		return (cat << category_bits) | static_cast<int32_t>(id);
	}
	static constexpr std::pair<category, int32_t> decompose_cat_id(int32_t raw_id) {
		return { static_cast<category>(raw_id >> category_bits), raw_id & ((1 << category_bits) - 1) };
	}
	static constexpr void Register(auto& list, category cat, AviUtl::FilterPlugin* fp)
	{
		for (auto const& [id, name] : list)
			fp->exfunc->add_menu_item(fp, name, fp->hwnd, combine_cat_id(cat, id),
				0, AviUtl::ExFunc::AddMenuItemFlag::None);
	}
}


////////////////////////////////
// AviUtlに渡す関数の定義．
////////////////////////////////
static BOOL exedit_wnd_proc_detour(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	return exedit_hook::manager(hwnd, message, wparam, lparam, editp, fp);
}
std::remove_pointer_t<decltype(AviUtl::FilterPlugin::func_WndProc)> func_WndProc;
static BOOL func_init(AviUtl::FilterPlugin* fp)
{
	// グローバル変数への情報源登録．
	enhanced_tl::this_fp = fp;

	// 情報源確保．
	if (!exedit.init(fp)) {
		::MessageBoxW(fp->hwnd_parent, L"拡張編集0.92が見つかりませんでした．",
			sigma_lib::string::encode_sys::to_wide_str(fp->name).c_str(),
			MB_OK | MB_ICONEXCLAMATION);
		return FALSE;
	}

	// 設定ロード．
	char ini_file[MAX_PATH];
	replace_tail(ini_file, ::GetModuleFileNameA(fp->dll_hinst, ini_file, std::size(ini_file)) + 1, "auf", "ini");

	enhanced_tl::walkaround::		settings.load(ini_file);
	enhanced_tl::layer_resize::		settings.load(ini_file);
	enhanced_tl::context_menu::		settings.load(ini_file);
	enhanced_tl::mouse_override::	settings.load(ini_file);
	enhanced_tl::tooltip::			settings.load(ini_file);

	// 競合確認．
	if (!enhanced_tl::layer_resize::	check_conflict() ||
		!enhanced_tl::mouse_override::	check_conflict() ||
		false) return FALSE;

	// メニュー登録．
	Menu::Register(enhanced_tl::walkaround		::menu_items, Menu::walkaround,		fp);
	Menu::Register(enhanced_tl::layer_resize	::menu_items, Menu::layer_resize,	fp);
	Menu::Register(enhanced_tl::context_menu	::menu_items, Menu::context_menu,	fp);

	// IME を無効化．
	::ImmReleaseContext(fp->hwnd, ::ImmAssociateContext(fp->hwnd, nullptr));

	// 拡張編集のメッセージハンドラ差し替え．
	exedit_hook::manager.init(std::exchange(exedit.fp->func_WndProc, &exedit_wnd_proc_detour));

	// 初期化前に呼ばれないようにしていたコールバック関数をこのタイミングで設定 (WM_PAINT が初期化前に降ってくる).
	fp->func_WndProc = func_WndProc;
	::InvalidateRect(fp->hwnd, nullptr, FALSE);

	return TRUE;
}

static BOOL func_WndProc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp)
{
	using Message = AviUtl::FilterPlugin::WindowMessage;
	switch (message) {
		// 仕込みの設定/解除．
	case Message::Init:
	{
		enhanced_tl::layer_resize::		setup(hwnd, true);
		enhanced_tl::context_menu::		setup(hwnd, true);
		enhanced_tl::mouse_override::	setup(hwnd, true);
		enhanced_tl::tooltip::			setup(hwnd, true);
		break;
	}

	case Message::Exit:
	{
		enhanced_tl::layer_resize::		setup(hwnd, false);
		enhanced_tl::context_menu::		setup(hwnd, false);
		enhanced_tl::mouse_override::	setup(hwnd, false);
		enhanced_tl::tooltip::			setup(hwnd, false);

		// 設定セーブ．
		char ini_file[MAX_PATH];
		replace_tail(ini_file, ::GetModuleFileNameA(fp->dll_hinst, ini_file, std::size(ini_file)) + 1, "auf", "ini");
		enhanced_tl::layer_resize::settings.save(ini_file);
		break;
	}

	case PrivateMsg::RequestCallback:
	{
		if (auto const callback = reinterpret_cast<PrivateMsg::callback_fnptr>(lparam);
			callback != nullptr) return callback(hwnd, wparam);
		break;
	}

		// command handlers.
	case Message::Command:
	{
		switch (auto const [cat, id] = Menu::decompose_cat_id(static_cast<int32_t>(wparam)); cat) {
		case Menu::walkaround:		return enhanced_tl::walkaround::
			on_menu_command(hwnd, id, editp) ? TRUE : FALSE;
		case Menu::layer_resize:	return enhanced_tl::layer_resize::
			on_menu_command(hwnd, id, editp) ? TRUE : FALSE;
		case Menu::context_menu:	return enhanced_tl::context_menu::
			on_menu_command(hwnd, id, editp) ? TRUE : FALSE;
		default:
			break;
		}
		break;
	}

		// let the parent window handle shortcut keys.
	case WM_KEYDOWN:
	case WM_CHAR:
	case WM_KEYUP:
	case WM_SYSKEYDOWN:
	case WM_SYSCHAR:
	case WM_SYSKEYUP:
	{
		::SendMessageW(fp->hwnd_parent, message, wparam, lparam);
		break;
	}

	default:
		// the rest is handled by `layer_resize` component.
		return enhanced_tl::layer_resize::on_wnd_proc(hwnd, message, wparam, lparam);
	}
	return FALSE;
}


////////////////////////////////
// Entry point.
////////////////////////////////
BOOL WINAPI DllMain(HINSTANCE hinst, DWORD fdwReason, LPVOID lpvReserved)
{
	switch (fdwReason) {
	case DLL_PROCESS_ATTACH:
		::DisableThreadLibraryCalls(hinst);
		break;
	}
	return TRUE;
}


////////////////////////////////
// 看板．
////////////////////////////////
#define PLUGIN_NAME		"Enhanced Timeline"
#define PLUGIN_VERSION	"v1.00-pre1"
#define PLUGIN_AUTHOR	"sigma-axis"
#define PLUGIN_INFO_FMT(name, ver, author)	(name " " ver " by " author)
#define PLUGIN_INFO		PLUGIN_INFO_FMT(PLUGIN_NAME, PLUGIN_VERSION, PLUGIN_AUTHOR)

extern "C" __declspec(dllexport) AviUtl::FilterPluginDLL* __stdcall GetFilterTable(void)
{
	constexpr int32_t def_size = 256;

	// （フィルタとは名ばかりの）看板．
	using Flag = AviUtl::FilterPlugin::Flag;
	static constinit AviUtl::FilterPluginDLL filter{
		.flag = Flag::AlwaysActive | Flag::ExInformation | Flag::DispFilter
			| Flag::WindowSize | Flag::WindowThickFrame,
		.x = def_size, .y = def_size,
		.name = PLUGIN_NAME,

		.func_init = func_init,
		.func_WndProc = nullptr, // 初期化前に WM_PAINT が降ってくるので，初期化時に設定する．
		.information = PLUGIN_INFO,
	};
	return &filter;
}
