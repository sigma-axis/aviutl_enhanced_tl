/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <numeric>
#include <bit>
#include <cmath>
#include <tuple>

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

using byte = uint8_t;
#include <exedit.hpp>

#include "../modkeys.hpp"
#include "../timeline.hpp"
#include "mouse_actions.hpp"

#include "../enhanced_tl.hpp"
#include "../mouse_override.hpp"
#include "zoom_gauge.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// 拡大率ゲージ周りのマウス操作．
////////////////////////////////
using namespace enhanced_tl::mouse_override::zoom_gauge;
namespace internal = enhanced_tl::mouse_override::internal;
namespace tl = enhanced_tl::timeline;

NS_END


////////////////////////////////
// Exported functions.
////////////////////////////////
namespace expt = enhanced_tl::mouse_override::zoom_gauge;

////////////////////////////////
// ホイール．
////////////////////////////////
// wheels::zoom_h
bool expt::wheels::zoom_h(int screen_x, int screen_y, int delta, modkeys mkeys)
{
	if (!is_editing()) return false;

	// apply the zoom.
	exedit.set_timeline_zoom(
		*exedit.curr_timeline_zoom_level + (delta > 0 ? +1 : -1),
		internal::zoom_center_frame(settings.zoom_gauge.zoom_center));
	return false;
}
