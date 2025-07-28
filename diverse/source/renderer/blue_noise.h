#pragma once

#include "maths/maths_utils.h"
#include <vector>

namespace diverse
{
	std::vector<i32>& get_rank_tile_data();
	std::vector<i32>& get_scrambling_tile_data();
	std::vector<i32>& get_sobol_data();
}