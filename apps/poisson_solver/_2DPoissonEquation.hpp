#ifndef _2DPOISSONEQUATION_HPP
#define _2DPOISSONEQUATION_HPP

#include <stdlib.h>
#include <functional>
#include <cmath>
#include "_2DBoundaryFunction.hpp"
#include "_2DGrid.hpp"
#include "opt.hpp"

using namespace std;

/*
	F, the vector of constant terms
	
	0= F[0][0]   	F[0][1] ...  F[0][n-2]  	F[0][n-1] = 0
	0= F[1][0]   	F[1][1] ...  F[1][n-2]  	F[1][n-1] = 0
	   .
	   .
	   .
	0=F[n-1][0]   F[n-1][1] ... F[n-1][n-2]	   F[n-1][n-1] = 0

	   in the non specified points (border) is undefined (zero)	
*/

class _2DPoissonEquation
{

public:
	_2DGrid *U;

	double *__attribute__((aligned(ALIGNMENT))) F;

	_2DBoundaryFunction *G;

	std::function<double(double, double)> E;

public:
	_2DPoissonEquation(_2DGrid *grid,
					   _2DBoundaryFunction *g);

	_2DPoissonEquation(_2DGrid *grid,
					   _2DBoundaryFunction *g,
					   double *f);

	_2DPoissonEquation(_2DGrid *grid,
					   _2DBoundaryFunction *g,
					   std::function<double(double, double)> f,
					   std::function<double(double, double)> e);

	double checkExactSolution(std::function<double(double, double)> u);

	~_2DPoissonEquation();

	_2DGrid *getGrid();

	double *getF();

	double getSolutionError();

	void printF();

	void printU();
};

#endif