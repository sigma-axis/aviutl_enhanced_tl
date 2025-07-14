/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#pragma once

#include <cstdint>
#include <vector>
#include <set>
#include <bit>
#include <concepts>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "modkeys.hpp"


////////////////////////////////
// 主要情報源の変数アドレス．
////////////////////////////////
enum class drag_kind : int32_t {
	none				= 0, // ドラッグなし．
	move_object			= 1, // オブジェクト移動ドラッグ．
	move_object_left	= 2, // オブジェクトの左端や中間点右側をつまんで移動．
	move_object_right	= 3, // オブジェクトの右端や中間点左側をつまんで移動．
	move_frame			= 4, // 現在フレームを動かすドラッグ．
	timeline_zoom		= 5, // タイムラインの拡大率ゲージを操作．
	range_select		= 6, // Ctrl での複数選択ドラッグ．
	scroll				= 7, // Alt でのスクロールドラッグ．
};

inline constinit struct ExEdit092 {
	AviUtl::FilterPlugin* fp;
	bool init(AviUtl::FilterPlugin* this_fp);

	int32_t*	SortedObjectLayerBeginIndex;	// 0x149670; int32_t[]
	int32_t*	SortedObjectLayerEndIndex;		// 0x135ac8; int32_t[]
	ExEdit::Object**	SortedObject;			// 0x168fa8; Object*[]
	ExEdit::Object**	ObjectArray_ptr;		// 0x1e0fa4; Object(*)[]
	int32_t*	NextObjectIdxArray;				// 0x1592d8; int32_t[]
	int32_t*	SettingDialogObjectIndex;		// 0x177a10
	int32_t*	SelectedObjectNum_ptr;			// 0x167d88
	int32_t*	SelectedObjectIndex;			// 0x179230; int32_t[]
	ExEdit::SceneSetting*	SceneSettings;		// 0x177a50; SceneSetting[]
	ExEdit::LayerSetting*	LayerSettings;		// 0x188498; LayerSetting[]

	int32_t*	current_scene;					// 0x1a5310
	int32_t*	curr_scene_len;					// 0x1a5318
	int32_t*	curr_edit_frame;				// 0x1a5304
	//int32_t*	curr_play_frame;				// 0x0a3fc0, green line.
	int32_t*	curr_timeline_zoom_level;		// 0x0a3fc4
	int32_t*	curr_timeline_zoom_len;			// 0x0a3fc8
	int32_t*	timeline_zoom_lengths;		// 0x0a3fd0; int32_t[]
	int32_t*	curr_timeline_layer_height;		// 0x0a3e20

	int32_t*	scroll_follows_cursor;			// 0x1790d0, 0: OFF, 1: ON

	HWND*		hwnd_setting_dlg;				// 0x1539c8
	HWND*		timeline_h_scroll_bar;			// 0x179108
	int32_t*	timeline_h_scroll_pos;			// 0x1a52f0
	int32_t*	timeline_width_in_frames;		// 0x1a52f8
	SIZE*		timeline_size_in_pixels;		// 0x1a52fc, excludes right vertical scroll bar.
	HWND*		timeline_v_scroll_bar;			// 0x158d34
	int32_t*	timeline_v_scroll_pos;			// 0x1a5308
	int32_t*	timeline_height_in_layers;		// 0x0a3fbc

	HMENU*		root_menu_scene;				// 0x158f64

	uint32_t*	undo_id_ptr;					// 0x244e08 + 12
	drag_kind*	timeline_drag_kind;				// 0x177a24
	//int32_t*	timeline_drag_edit_flag;		// 0x177a08, 0: clean, 1: dirty.
		// makes sense only for drag kinds 1, 2, 3 above.
	int32_t*	last_clicked_point_x;			// 0x1460b4
	int32_t*	last_clicked_point_y;			// 0x196744
	int32_t*	idx_obj_selection_origin;		// 0x19675c
	int32_t*	is_drag_midpt_with_shift;		// 0x196608

	int32_t*	timeline_BPM_tempo;				// 0x159190, multiplied by 10'000
	int32_t*	timeline_BPM_frame_origin;		// 0x158d28, 1-based
	int32_t*	timeline_BPM_num_beats;			// 0x178e30
	int32_t*	timeline_BPM_show_grid;			// 0x196760, 0: hidden, 1: visible

	int32_t*	layer_height_mode;				// 0x1539d4; 0: large, 1: medium, 2: small.
	int32_t*	layer_height_preset;			// 0x0a3e08; int32_t[3]
	int32_t*	midpt_mk_sz_preset;				// 0x0a3e14; int32_t[3]
	int32_t*	layer_height;					// 0x0a3e20
	int32_t*	midpt_marker_size;				// 0x0a3e24

	char const*	basic_scene_change_names;		// 0x0aef38
	char const*	basic_animation_names;			// 0x0c1f08
	char const*	basic_custom_obj_names;			// 0x0ce090
	char const*	basic_camera_eff_names;			// 0x0d20d0

	ExEdit::Filter**	loaded_filter_table;	// 0x187c98; Filter*[].
	uintptr_t*	exdata_table;					// 0x1e0fa8
	AviUtl::EditHandle**	editp;				// 0x1a532c

	// 0x037240 8b 0d xx xx xx xx  mov ecx,dword ptr ds:[exedit+0x00167d84]
	HFONT**		load_tl_object_font;			// 0x037240 + 2

	void(*nextundo)();							// 0x08d150
	// 0x08d290
	// mode:
	//   0x00 -> ??
	//   0x01 -> an entire chain of objects;
	//           internally calls mode=0x00 for each interval.
	//   0x02 -> new object.
	//   0x04 -> delete object.
	//   0x08 -> single object.
	//   0x10 -> layer.
	void(*setundo)(uint32_t index, uint32_t mode);
	int32_t(*update_object_tables)();			// 0x02b0f0

	// 0x0395e0
	void(*set_timeline_zoom)(int32_t zoom_level, int32_t center_frame);

	// 0x032ee0
	// returns -1 if no objects found.
	// offset at most 8 pixels is allowed.
	int32_t(*obj_from_point)(int32_t client_x, int32_t client_y);

	// 0x033220
	// returns certain points (object boundaries or current frame?) near the given frame.
	int32_t(*find_nearby_frame)(int32_t frame, int32_t layer, int32_t zero_unknown);

	// 0x034560
	// internally calls setundo(idx_object, 1).
	// returns the index of the newly created object.
	int32_t(*add_midpoint)(int32_t idx_object, int32_t frame, int32_t zero_unknown);

	// 0x034a30
	// internally calls setundo(idx_object, 1).
	// returns < 0 if failed. otherwise, returns the index of the removed object.
	int32_t(*delete_midpoint)(int32_t idx_object, int32_t frame);

	// 0x039130
	void(*adjust_h_scroll)(int client_x); // yet unknown functionality.

	// 0x039190
	void(*begin_range_selection)(int client_x, int client_y); // yet unknown functionality.

	// 0x034eb0
	// https://github.com/nazonoSAUNA/patch.aul/blob/r43_73/patch/patch_any_obj.hpp#L47
	void(*deselect_all_objects)();

	// 0x02ba60
	// https://github.com/nazonoSAUNA/patch.aul/blob/dd7bac283dd66f4b8d23b3a396945f2570117467/patch/offset_address.hpp#L343
	void(*change_disp_scene)(int32_t scene_idx, AviUtl::FilterPlugin* fp, AviUtl::EditHandle* editp);

	// 0x0305e0
	// updating the setting dialog.
	int32_t(*update_setting_dlg)(int32_t idx_object);

private:
	void init_pointers(HINSTANCE dll_hinst, HINSTANCE hinst_parent)
	{
		auto pick_addr = [exedit_base = reinterpret_cast<uintptr_t>(dll_hinst)]
			<class T>(T & target, ptrdiff_t offset) { target = std::bit_cast<T>(exedit_base + offset); };
		pick_addr(SortedObjectLayerBeginIndex,	0x149670);
		pick_addr(SortedObjectLayerEndIndex,	0x135ac8);
		pick_addr(SortedObject,					0x168fa8);
		pick_addr(ObjectArray_ptr,				0x1e0fa4);
		pick_addr(NextObjectIdxArray,			0x1592d8);
		pick_addr(SettingDialogObjectIndex,		0x177a10);
		pick_addr(SelectedObjectNum_ptr,		0x167d88);
		pick_addr(SelectedObjectIndex,			0x179230);
		pick_addr(SceneSettings,				0x177a50);
		pick_addr(LayerSettings,				0x188498);

		pick_addr(current_scene,				0x1a5310);
		pick_addr(curr_scene_len,				0x1a5318);
		pick_addr(curr_edit_frame,				0x1a5304);
		//pick_addr(curr_play_frame,				0x0a3fc0);
		pick_addr(curr_timeline_zoom_level,		0x0a3fc4);
		pick_addr(curr_timeline_zoom_len,		0x0a3fc8);
		pick_addr(timeline_zoom_lengths,		0x0a3fd0);
		pick_addr(curr_timeline_layer_height,	0x0a3e20);

		pick_addr(scroll_follows_cursor,		0x1790d0);

		pick_addr(hwnd_setting_dlg,				0x1539c8);
		pick_addr(timeline_h_scroll_bar,		0x179108);
		pick_addr(timeline_h_scroll_pos,		0x1a52f0);
		pick_addr(timeline_width_in_frames,		0x1a52f8);
		pick_addr(timeline_size_in_pixels,		0x1a52fc);
		pick_addr(timeline_v_scroll_bar,		0x158d34);
		pick_addr(timeline_v_scroll_pos,		0x1a5308);
		pick_addr(timeline_height_in_layers,	0x0a3fbc);

		pick_addr(root_menu_scene,				0x158f64);

		pick_addr(undo_id_ptr,					0x244e08 + 12);
		pick_addr(timeline_drag_kind,			0x177a24);
		//pick_addr(timeline_drag_edit_flag,		0x177a08);
		pick_addr(last_clicked_point_x,			0x1460b4);
		pick_addr(last_clicked_point_y,			0x196744);
		pick_addr(idx_obj_selection_origin,		0x19675c);
		pick_addr(is_drag_midpt_with_shift,		0x196608);

		pick_addr(timeline_BPM_tempo,			0x159190);
		pick_addr(timeline_BPM_frame_origin,	0x158d28);
		pick_addr(timeline_BPM_num_beats,		0x178e30);
		pick_addr(timeline_BPM_show_grid,		0x196760);

		pick_addr(layer_height_mode,			0x1539d4);
		pick_addr(layer_height_preset,			0x0a3e08);
		pick_addr(midpt_mk_sz_preset,			0x0a3e14);
		pick_addr(layer_height,					0x0a3e20);
		pick_addr(midpt_marker_size,			0x0a3e24);

		pick_addr(basic_scene_change_names,		0x0aef38);
		pick_addr(basic_animation_names,		0x0c1f08);
		pick_addr(basic_custom_obj_names,		0x0ce090);
		pick_addr(basic_camera_eff_names,		0x0d20d0);

		pick_addr(loaded_filter_table,			0x187c98);
		pick_addr(exdata_table,					0x1e0fa8);
		pick_addr(editp,						0x1a532c);

		pick_addr(load_tl_object_font,			0x037240 + 2);

		pick_addr(nextundo,						0x08d150);
		pick_addr(setundo,						0x08d290);
		pick_addr(update_object_tables,			0x02b0f0);

		pick_addr(set_timeline_zoom,			0x0395e0);
		pick_addr(obj_from_point,				0x032ee0);
		pick_addr(find_nearby_frame,			0x033220);
		pick_addr(add_midpoint,					0x034560);
		pick_addr(delete_midpoint,				0x034a30);
		pick_addr(adjust_h_scroll,				0x039130);
		pick_addr(begin_range_selection,		0x039190);
		pick_addr(deselect_all_objects,			0x034eb0);
		pick_addr(change_disp_scene,			0x02ba60);
		pick_addr(update_setting_dlg,			0x0305e0);
	}
} exedit;

// 編集状態の確認．編集中で，かつ出力中ない場合にのみ true.
bool is_editing();
bool is_editing(AviUtl::EditHandle* editp);

// 複数選択オブジェクトの取得．
std::set<int32_t> get_multi_selected_objects();

// 複数選択オブジェクトの設定．
void set_multi_selected_objects(std::set<int32_t> const& sel);

// 設定ダイアログのデータ表示を更新．
void update_setting_dialog(int index);

// 編集データの変更でメイン画面を更新．
void update_current_frame();

template<class ExDataT>
inline ExDataT* find_exdata(ptrdiff_t obj_exdata_offset, ptrdiff_t filter_exdata_offset) {
	return reinterpret_cast<ExDataT*>((*exedit.exdata_table)
		+ obj_exdata_offset + filter_exdata_offset + 0x0004);
}

namespace filter_id
{
	enum id : int32_t {
		draw_std = 0x0a, // 標準描画
		draw_ext = 0x0b, // 拡張描画
		play_std = 0x0c, // 標準再生
		particle = 0x0d, // パーティクル出力

		scn_chg = 0x0e, // シーンチェンジ
		anim_eff = 0x4f, // アニメーション効果
		cust_obj = 0x50, // カスタムオブジェクト
		cam_eff = 0x61, // カメラ効果
	};
}


////////////////////////////////
// Windows API 利用の補助関数．
////////////////////////////////
template<int N>
inline bool check_window_class(HWND hwnd, wchar_t const(&classname)[N])
{
	wchar_t buf[N + 1];
	return ::GetClassNameW(hwnd, buf, N + 1) == N - 1
		&& std::wmemcmp(buf, classname, N - 1) == 0;
}

inline sigma_lib::modifier_keys::modkeys curr_modkeys() {
	return { ::GetKeyState(VK_CONTROL) < 0, ::GetKeyState(VK_SHIFT) < 0, ::GetKeyState(VK_MENU) < 0 };
}
inline bool key_pressed_any(auto... vkeys) {
	return ((::GetKeyState(vkeys) < 0) || ...);
}

inline void discard_message(HWND hwnd, UINT message) {
	MSG msg;
	while (::PeekMessageW(&msg, hwnd, message, message, PM_REMOVE));
}


////////////////////////////////
// タイムラインウィンドウのフック．
////////////////////////////////
struct hook_wnd_proc {
	friend struct hook_manager;
	using wnd_proc = BOOL __cdecl(hook_wnd_proc& next, HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp);
	using func_wnd_proc = std::remove_pointer_t<decltype(AviUtl::FilterPlugin::func_WndProc)>;

	BOOL operator()(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp) const;

	hook_wnd_proc(func_wnd_proc* def_proc)
		: proc{ reinterpret_cast<wnd_proc*>(def_proc) }, next{ nullptr } {}
	hook_wnd_proc(wnd_proc* proc, hook_wnd_proc* next)
		: proc{ proc }, next{ next } {}

private:
	wnd_proc* const proc;
	hook_wnd_proc* next;
};
struct hook_manager {
	void init(hook_wnd_proc::func_wnd_proc* original);
	void add(hook_wnd_proc::wnd_proc* proc);
	BOOL operator()(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, AviUtl::EditHandle* editp, AviUtl::FilterPlugin* fp) const;

private:
	std::vector<hook_wnd_proc> hooks{};
};
namespace exedit_hook
{
	inline hook_manager manager{};
}


////////////////////////////////
// 内部メッセージ．
////////////////////////////////
namespace PrivateMsg
{
	enum : uint32_t {
		// wparam: versatile parameter.
		// lparam: function pointer of the type `BOOL(*)(HWND, WPARAM)`.
		RequestCallback = WM_USER + 1,
	};
	using callback_fnptr = BOOL(__cdecl*)(HWND, WPARAM);
}


////////////////////////////////
// 競合通知メッセージ．
////////////////////////////////
/// @return `true` if the module was found and the confliction message was displayed, `false` otherwise.
bool warn_conflict(wchar_t const* module_name, wchar_t const* ini_piece);


////////////////////////////////
// このプラグインの情報源．
////////////////////////////////
namespace enhanced_tl
{
	inline AviUtl::FilterPlugin* this_fp = nullptr;
}
