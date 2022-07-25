#ifndef _2DJACOBI_HPP
#define _2DJACOBI_HPP

#include "_2DIterativePoissonSolver.hpp"

using namespace std;

class _2DSequentialJacobi : public _2DIterativePoissonSolver
{

public:
	_2DSequentialJacobi(double tol, int it);

	void operator()(_2DPoissonEquation *eq);
};

#endif