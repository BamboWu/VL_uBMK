#ifndef _2DSEQUENTIALGAUSSSEIDELREDBLACK_HPP
#define _2DSEQUENTIALGAUSSSEIDELREDBLACK_HPP

#include "_2DIterativePoissonSolver.hpp"

using namespace std;

class _2DSequentialGaussSeidelRedBlack : public _2DIterativePoissonSolver
{

public:
	_2DSequentialGaussSeidelRedBlack(double tol, int it);

	void operator()(_2DPoissonEquation *eq);
};

#endif