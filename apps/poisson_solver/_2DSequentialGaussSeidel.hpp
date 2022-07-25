#ifndef _2DSEQUENTIALGAUSSSEIDEL_HPP
#define _2DSEQUENTIALGAUSSSEIDEL_HPP

#include "_2DIterativePoissonSolver.hpp"

using namespace std;

class _2DSequentialGaussSeidel : public _2DIterativePoissonSolver
{

public:
	_2DSequentialGaussSeidel(double tol, int it);

	void operator()(_2DPoissonEquation *eq);
};

#endif