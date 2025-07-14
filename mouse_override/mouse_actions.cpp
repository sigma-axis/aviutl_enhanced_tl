/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;

#include "../modkeys.hpp"

#include "mouse_actions.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// ドラッグ状態管理の基礎クラス．
////////////////////////////////
using namespace enhanced_tl::mouse_override;
using namespace enhanced_tl::mouse_override::drags;

static inline bool state_changing = false;
struct change_state {
	// avoid recursive calls.
	change_state() { state_changing = true; }
	~change_state() { state_changing = false; }
};
static inline drag_state* curr_drag = nullptr;

NS_END


////////////////////////////////
// exported functions.
////////////////////////////////
namespace expt = enhanced_tl::mouse_override;

bool expt::drag_state::is_active() { return curr_drag != nullptr; }
bool expt::drag_state::active() const { return curr_drag == this; }
bool expt::drag_state::is_changing() { return state_changing; }
void expt::drag_state::invalidate_click()
{
	if (is_active() && status == drag_status::entering)
		status = drag_status::moving;
}
bool expt::drag_state::fallback_on_down(drag_state& other, modkeys mkeys)
{
	curr_drag = &other;
	return other.on_mouse_down_core(mkeys);
}
bool expt::drag_state::fallback_on_down()
{
	return fallback_on_down(drags::no_action, {});
}
bool expt::drag_state::fallback_on_move()
{
	curr_drag = &drags::no_action;
	status = drag_status::moving;
	return curr_drag->on_mouse_move_core({});
}

bool expt::drag_state::on_mouse_down(HWND hwnd, int x, int y, mouse_button btn, modkeys mkeys)
{
	if (is_changing() || !can_continue()) return false;
	change_state cs{};

	bool ret = false;
	if (is_active()) {
		if (curr_drag == this) return false; // keep the current drag.

		// cancel the current drag.
		ret = curr_drag->on_mouse_cancel_core(false);
		if (status != drag_status::entering ||
			target != hwnd) {
			// terminate the drag by replacing with `no_action`.
			ret |= fallback_on_down();

			// suppress click actions on mouse-up,
			// and other drags from taking place.
			button = btn;
			status = drag_status::moving;
			return ret;
		}

		// otherwise, overwrite the current drag.
	}

	// enter the state "waiting for mouse to move a bit".
	curr_drag = this;
	target = hwnd;
	button = btn;
	pt_start = pt_prev = pt_curr = { x, y };
	ret |= on_mouse_down_core(mkeys);
	status = drag_status::entering;

	return ret;
}

bool expt::drag_state::on_mouse_move(int x, int y, modkeys mkeys)
{
	if (is_changing() || !is_active()) return false;
	if (!curr_drag->can_continue()) return cancel(true);
	change_state cs{};

	pt_prev = std::exchange(pt_curr, { x, y });
	if (status == drag_status::entering) {
		if (std::abs(x - pt_start.x) <= curr_drag->click_tolerance_x &&
			std::abs(y - pt_start.y) <= curr_drag->click_tolerance_y)
			// not enough move of the mouse to recognize as a drag.
			return false;

		// dragging practically started.
		status = drag_status::moving;
	}

	return curr_drag->on_mouse_move_core(mkeys);
}

bool expt::drag_state::cancel(bool release)
{
	if (is_changing() || !is_active()) return false;
	change_state cs{};

	// let cancel the drag.
	auto ret = curr_drag->on_mouse_cancel_core(release);

	// set to the canceled status.
	curr_drag = nullptr;
	target = nullptr;
	status = drag_status::idle;
	last_status = last_drag_status::canceled;

	return ret;
}

bool expt::drag_state::on_mouse_up_all(int x, int y, modkeys mkeys)
{
	if (is_changing()) return false;
	if (!curr_drag->can_continue()) return cancel(true);
	change_state cs{};

	if (!is_active()) status = drag_status::idle;
	switch (status) {
	default:
	case drag_status::idle: return false;
	case drag_status::entering:
	{
		pt_prev = std::exchange(pt_curr, { x, y });
		last_status = button == mouse_button::L_and_R ?
			last_drag_status::canceled :
			last_drag_status::clicked;
		break;
	}
	case drag_status::moving:
	{
		pt_prev = std::exchange(pt_curr, { x, y });
		last_status = last_drag_status::dragged;
		break;
	}
	}

	auto ret = curr_drag->on_mouse_up_core(mkeys);
	curr_drag = nullptr;
	target = nullptr;
	status = drag_status::idle;

	return ret;
}
void expt::drag_state::unmark_canceled() { last_status = last_drag_status::clicked; }
bool expt::drag_state::handle_key_messages(bool& ret, UINT message, WPARAM wparam, LPARAM lparam)
{
	if (!is_active()) return false;
	if (!curr_drag->can_continue()) {
		ret |= cancel(true);
		return true;
	}

	return curr_drag->handle_key_messages_core(ret, message, wparam, lparam);
}


expt::drags::no_action::no_action() {
	constexpr int vast_len = 1 << 16;
	click_tolerance_x = click_tolerance_y = vast_len;
}
bool expt::drags::no_action::on_mouse_down_core(modkeys mkeys)
{
	::SetCapture(target);
	::SetCursor(::LoadCursorW(nullptr, reinterpret_cast<wchar_t const*>(IDC_ARROW)));
	return false;
}
bool expt::drags::no_action::on_mouse_move_core(modkeys mkeys) { return false; }
bool expt::drags::no_action::on_mouse_up_core(modkeys mkeys)
{
	::ReleaseCapture();
	return false;
}
bool expt::drags::no_action::on_mouse_cancel_core(bool release)
{
	if (release) ::ReleaseCapture();
	return false;
}

bool expt::clicks::no_action(int x, int y, modkeys mkeys)
{
	return false;
}

bool expt::wheels::no_action(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	return false;
}
