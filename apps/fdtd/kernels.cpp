#include "kernels.hpp"

#include <cstdlib>
#include <iostream>

template class Worker<H>;
template class Worker<E>;

GridPrinter::GridPrinter(phys::params params, bool silent) : params(params), silent(silent)
{
	input.addPort<elem_t>("grid");
}

raft::kstatus GridPrinter::run()
{;
	if (silent)
		return raft::stop;

	Grid g = Grid(params.nx, params.ny, params.nz);
	for (dim_t i = 0, lim = g.x() * g.y() * g.z(); i < lim; i++)
		input["grid"].pop(g[i]);

	std::stringstream ss;
	ss << "Grid(" << g.x() << ", " << g.y() << ", " << g.z() << ") = [ ";
	for (dim_t i = 0; i < g.x() * g.y() * g.z(); i++)
		ss << g[i] << " ";
	ss << "]" << std::endl;

	std::cout << ss.rdbuf() << std::endl;
	return raft::proceed;
}

template <typename T>
Worker<T>::Worker(phys::params params, unsigned long i, bool silent) :
	params(params), iterations(i), silent(silent), grid(params.nx, params.ny, params.nz) {
	input.addPort<int>("dummy");
	input.addPort<elem_t>("A");
	input.addPort<elem_t>("B");
	output.addPort<elem_t>("out_A");
	output.addPort<elem_t>("out_B");
	output.addPort<elem_t>("final");

	// Random initial grid
	for (dim_t j = 0, lim = params.nx * params.ny * params.nz; j < lim; j++) {
		grid[j] = drand48();
	}
}

template <typename T>
void Worker<T>::popGrids(Grid &a, Grid &b)
{
	a = Grid(params.nx, params.ny, params.nz);
	b = Grid(params.nx, params.ny, params.nz);
	for (dim_t i = 0, lim = params.nx * params.ny * params.nz; i < lim; i++) {
		input["A"].pop(a[i]);
		input["B"].pop(b[i]);
	}
}

template <typename T>
void Worker<T>::pushGrid()
{
	for (dim_t i = 0, lim = params.nx * params.ny * params.nz; i < lim; i++) {
		output["out_A"].push(grid[i]);
		output["out_B"].push(grid[i]);
	}
}

// H-field workers first pop grids, then compute, then push.
template <typename T>
raft::kstatus Worker<T>::run()
{
	if (iterations-- == 0) {
		if (!silent) {
			for (dim_t i = 0, lim = params.nx * params.ny * params.nz; i < lim; i++)
				output["final"].push(grid[i]);
		}
		return raft::stop;
	}

	// E-field workers should push before popping
	if (typeid(T) == typeid(E))
		pushGrid();

	Grid a, b;
	popGrids(a, b);

	// Bounds also differ by one between H and E fields
	dim_t x_min = typeid(T) == typeid(E);
	dim_t x_max = x_min + params.nx - 1;
	dim_t y_min = typeid(T) == typeid(E);
	dim_t y_max = y_min + params.ny - 1;
	dim_t z_min = typeid(T) == typeid(E);
	dim_t z_max = z_min + params.nz - 1;

	// Calculate delta at each location and in-place modify our grid
	for (dim_t x = x_min; x < x_max; x++) {
		for (dim_t y = y_min; y < y_max; y++) {
			for (dim_t z = z_min; z < z_max; z++) {
				grid.at(x, y, z) += diff(a, b, x, y, z);
			}
		}
	}

	// H-field workers should push after the work is done
	if (typeid(T) == typeid(H))
		pushGrid();
	return raft::proceed;
}

// Unfortunately, there's not really a cleaner way to write out all the calculations, but here's the gist:
// A field (Type, Dim) takes as input the two fields of opposite Type and differing Dim.
// It then computes a cross product over those two fields across space.

elem_t Hx::diff(const Grid &ey, const Grid &ez, dim_t x, dim_t y, dim_t z)
{
	return params.ch * ((ey.at(x,y,z+1)-ey.at(x,y,z))*params.cz - (ez.at(x,y+1,z)-ez.at(x,y,z))*params.cy);
}

elem_t Hy::diff(const Grid &ez, const Grid &ex, dim_t x, dim_t y, dim_t z)
{
	return params.ch * ((ez.at(x+1,y,z)-ez.at(x,y,z))*params.cx - (ex.at(x,y,z+1)-ex.at(x,y,z))*params.cz);
}

elem_t Hz::diff(const Grid &ex, const Grid &ey, dim_t x, dim_t y, dim_t z)
{
	return params.ch * ((ex.at(x,y+1,z)-ex.at(x,y,z))*params.cy - (ey.at(x+1,y,z)-ey.at(x,y,z))*params.cx);
}

elem_t Ex::diff(const Grid &hy, const Grid &hz, dim_t x, dim_t y, dim_t z)
{
	return params.ce * ((hy.at(x,y,z)-hy.at(x,y,z-1))*params.cz - (hz.at(x,y,z)-hz.at(x,y-1,z))*params.cy);
}

elem_t Ey::diff(const Grid &hz, const Grid &hx, dim_t x, dim_t y, dim_t z)
{
	return params.ce * ((hz.at(x,y,z)-hz.at(x-1,y,z))*params.cx - (hx.at(x,y,z)-hx.at(x,y,z-1))*params.cz);
}

elem_t Ez::diff(const Grid &hx, const Grid &hy, dim_t x, dim_t y, dim_t z)
{
	return params.ce * ((hx.at(x,y,z)-hx.at(x,y-1,z))*params.cy - (hy.at(x,y,z)-hy.at(x-1,y,z))*params.cx);
}
