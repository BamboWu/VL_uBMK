#ifndef _2DPARALLELGAUSSSEIDELREDBLACK_HPP
#define _2DPARALLELGAUSSSEIDELREDBLACK_HPP

#include "_2DIterativePoissonSolver.hpp"

using namespace std;

class _2DParallelGaussSeidelRedBlack : public _2DIterativePoissonSolver
{

public:
	int numberOfWorkers;
	int chunk;
	int ci;
	int cj;

public:
	_2DParallelGaussSeidelRedBlack(double tol, int it, int nw, int chunk, int i, int j);

	void operator()(_2DPoissonEquation *eq);

	int getNumberOfWorkers();

	int getChunk();
};

#endif
