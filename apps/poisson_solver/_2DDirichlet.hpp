#ifndef _2DDIRICHLET_HPP
#define _2DDIRICHLET_HPP

#include "_2DBoundaryFunction.hpp"
#include <functional>

using namespace std;

class _2DDirichlet : public _2DBoundaryFunction
{

public:
	_2DDirichlet(std::function<double(double, double)> gfun);

	void operator()(_2DGrid *grid);
};

#endif