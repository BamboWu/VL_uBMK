#pragma once

#include <cstdint>
#include <cstring>
#include <iostream>
#include <memory>

using dim_t = std::uint64_t;
using elem_t = double;

// A 3-dimensional grid of elements
class Grid {
	// Provided dimensions
	dim_t dimX, dimY, dimZ;
	// Pre-computed medium stride
	dim_t mstride;
	// Internal array
	std::shared_ptr<elem_t[]> grid;

public:
	// Required by RaftLib for some reason
	Grid() : Grid(0, 0, 0) {}

	Grid(dim_t dimX, dim_t dimY, dim_t dimZ) : dimX(dimX), dimY(dimY), dimZ(dimZ),
			mstride(dimX * dimY), grid(new elem_t[dimX * dimY * dimZ]()) {
	}

	Grid(const Grid &o) = default;

	Grid &operator=(const Grid &o) = default;

	elem_t &at(dim_t x, dim_t y, dim_t z) const {
		return grid[x + (y * dimX) + (z * mstride)];
	}

	elem_t &operator[](dim_t index) {
		return grid[index];
	}

	dim_t x() const {
		return dimX;
	}

	dim_t y() const {
		return dimY;
	}

	dim_t z() const {
		return dimZ;
	}
};
