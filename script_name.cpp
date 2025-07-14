/*
The MIT License (MIT)

Copyright (c) 2025 sigma-axis

Permission is hereby granted, free of charge, to any person obtaining a copy of this software and associated documentation files (the “Software”), to deal in the Software without restriction, including without limitation the rights to use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software, and to permit persons to whom the Software is furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
*/

#include <cstdint>
#include <vector>

using byte = uint8_t;
#include <exedit.hpp>

#include "enhanced_tl.hpp"
#include "script_name.hpp"


#define NS_BEGIN(...) namespace __VA_ARGS__ {
#define NS_END }
NS_BEGIN()

////////////////////////////////
// フィルタ効果のスクリプト名表示．
////////////////////////////////
using namespace enhanced_tl::script_name;

#ifndef _DEBUG
constinit
#endif // !_DEBUG
static struct {
	// supply this struct with the heading pointer, turning the functionality enabled.
	void init(char const* head) { names.clear(); names.push_back(head); }

	// retrieves the name of the script for the fiter at the given index of the given object.
	// the filter is known to have the specific type of exdata.
	template<class ExDataT>
	char const* get(ExEdit::Object const& leader, ExEdit::Object::FilterParam const& filter_param) {
		auto exdata = find_exdata<ExDataT>(leader.exdata_offset, filter_param.exdata_offset);
		return exdata->name[0] != '\0' ? exdata->name : find(exdata->type);
	}

private:
	std::vector<char const*> names{};
	char const* find(size_t idx) {
		if (names.size() <= 1) collect();
		return idx < names.size() - 1 ? names[idx] : nullptr;
	}
	void collect()
	{
		// collect the heading pointers to each name.
		for (auto* head = names.back(); *head != '\0'; names.push_back(head))
			head += std::strlen(head) + 1;
		names.shrink_to_fit();
	}

} basic_anm_names, basic_obj_names, basic_cam_names, basic_scn_names;

static char const* get_script_name(ExEdit::Object const& leader, int filter_index)
{
	auto& filter_param = leader.filter_param[filter_index];

	if (filter_param.is_valid()) {
		using anm_exdata = ExEdit::Exdata::efAnimationEffect; // shared with camera eff and custom object.
		using scn_exdata = ExEdit::Exdata::efSceneChange;

		switch (filter_param.id) {
		case filter_id::anim_eff:
			return basic_anm_names.get<anm_exdata>(leader, filter_param);

		case filter_id::cust_obj:
			return basic_obj_names.get<anm_exdata>(leader, filter_param);

		case filter_id::cam_eff:
			return basic_cam_names.get<anm_exdata>(leader, filter_param);

		case filter_id::scn_chg:
			return basic_scn_names.get<scn_exdata>(leader, filter_param);
		}
	}
	return nullptr;
}

static constinit bool initialized = false;
static void init()
{
	basic_anm_names.init(exedit.basic_animation_names);
	basic_obj_names.init(exedit.basic_custom_obj_names);
	basic_cam_names.init(exedit.basic_camera_eff_names);
	basic_scn_names.init(exedit.basic_scene_change_names);
}
NS_END


////////////////////////////////
// exported functions.
////////////////////////////////
namespace expt = enhanced_tl::script_name;

expt::names::names(int index_leading, int index_filter)
	: names{ (*exedit.ObjectArray_ptr)[index_leading] , index_filter} {}

expt::names::names(ExEdit::Object const& leading, int index_filter)
{
	if (!initialized) init();

	auto const id = leading.filter_param[index_filter].id;
	filter = exedit.loaded_filter_table[id]->name;
	script = get_script_name(leading, index_filter);
}
