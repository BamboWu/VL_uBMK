#pragma once

#include "grid.hpp"

#include <cmath>

namespace phys
{
	constexpr elem_t pi(3.141592653);
	constexpr elem_t eps0(8.8541878e-12);
	constexpr elem_t mu0(4e-7 * pi);
	constexpr elem_t c0(299792458);

	constexpr elem_t lx(0.5);
	constexpr elem_t ly(0.4);
	constexpr elem_t lz(0.3);

	struct params
	{
		const elem_t nx;
		const elem_t ny;
		const elem_t nz;

		const elem_t cx;
		const elem_t cy;
		const elem_t cz;

		const elem_t dt;
		const elem_t ch; // dt/mu0
		const elem_t ce; // dt/eps0

		params(dim_t x, dim_t y, dim_t z) :
				nx(x), ny(y), nz(z),
				cx(nx/lx), cy(ny/ly), cz(nz/lz),
				dt(1 / (100 * std::sqrt(cx*cx + cy*cy + cz*cz) * phys::c0)),
				ch(dt/phys::mu0), ce(dt/phys::eps0) {}
	};
}