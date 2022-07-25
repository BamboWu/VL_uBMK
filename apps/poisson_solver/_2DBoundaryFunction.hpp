#ifndef _2DBOUNDARYFUNCTION_HPP
#define _2DBOUNDARYFUNCTION_HPP

#include "_2DGrid.hpp"
#include <functional>

using namespace std;

class _2DBoundaryFunction
{

public:
	std::function<double(double, double)> g;

public:
	virtual void operator()(_2DGrid *grid){};
};

#endif