

/**
	The discrete region of n*m points, where we find a solution for u, made of
	points {(x,y) | ax<=x<=bx, ay<=y<=by} such that
	(x, y) = (ax + i*hx, ay + j*hy), where
	i = 1...n-1, j = 1...m-1 and
	hx = (bx - ax)/n, hy = (by - ay)/m are the spacing between grid-points

**/

/**
	The discretized 2D Poisson equation (in matrix from)
				AU = -F
	A, the coefficients matrix of size (n*m)*(n*m)
	U, the grid of n*m points
	F, the coefficients vector of size (n*m)
**/

/*

void _2DPoissonEquation::addBoundariesConditions()
{
	int _n = grid->getXlen(),
    	_m = grid->getYlen();

    double _hx = grid->getXspacing(),
           _hy = grid->getYspacing();         	

    for(int i=0; i < _n; i++)
	  for(int j=0; j < _m; j++)
	  	F[i + j*_m] += G(i*_hx, j*_hy);

}*/

#include "_2DPoissonEquation.hpp"
#include "_2DBoundaryFunction.hpp"
#include "_2DGrid.hpp"
#include <stdlib.h>
#include <assert.h>
#include <cstdio>
#include "opt.hpp"

using namespace std;

_2DPoissonEquation::_2DPoissonEquation(_2DGrid *grid,
									   _2DBoundaryFunction *g)
{

	U = grid;
	G = g;

	const auto res(posix_memalign((void **)&F, ALIGNMENT,
                (grid->getXlen() * grid->getYlen()) * sizeof(double)));
	assert(0 == res);

	for (int i = 0; i < grid->getXlen() * grid->getYlen(); i++)
		F[i] = 0.0;

	(*G)(U);
}

_2DPoissonEquation::_2DPoissonEquation(_2DGrid *grid,
									   _2DBoundaryFunction *g,
									   std::function<double(double, double)> f,
									   std::function<double(double, double)> e)
{

	U = grid;
	G = g;
	E = e;
	const auto res(posix_memalign((void **)&F, ALIGNMENT,
                (grid->getXlen() * grid->getYlen()) * sizeof(double)));
	assert(0 == res);

	(*G)(U);

	double _hx = U->getXspacing(),
		   _hy = U->getYspacing();

	int _n = U->getXlen(),
		_m = U->getYlen();

	double _ax = U->getax(),
		   _ay = U->getay(),
		   _by = U->getby(),
		   _bx = U->getbx();

	for (int j = 0; j < _m; j++)
	{
		F[0 + j] = f(_ax + j * _hx, _by);			  //north border
		F[(_n - 1) * _m + j] = f(_ax + j * _hx, _ay); //south border
	}

	for (int i = 1; i < _n - 1; i++)
	{
		F[i * _m + 0] = f(_ax, _ay + i * _hy);		//east border
		F[i * _m + _m - 1] = f(_bx, _ay + i * _hy); //west border
	}

	for (int i = 1; i < _n - 1; i++)
		for (int j = 1; j < _m - 1; j++)
			F[i * _m + j] = f(_ax + j * _hx, _ay + i * _hy);
}

_2DPoissonEquation::_2DPoissonEquation(_2DGrid *grid,
									   _2DBoundaryFunction *g,
									   double *f)
{

	U = grid;
	G = g;
	F = f;

	(*G)(U);
}

_2DPoissonEquation::~_2DPoissonEquation() { free(F); }

double _2DPoissonEquation::checkExactSolution(std::function<double(double, double)> u)
{

	double _hx = U->getXspacing(),
		   _hy = U->getYspacing();

	int _n = U->getXlen(),
		_m = U->getYlen();

	double _ax = U->getax(),
		   _ay = U->getay(),
		   _by = U->getby(),
		   _bx = U->getbx();

	long double e = 0.0;

	//check borders error

	for (int j = 0; j < _m; j++)
	{
		e += (U->get(0 + j) - E(_ax + j * _hx, _by)) * (U->get(0 + j) - E(_ax + j * _hx, _by));				//north border
		e += (U->get(0 + j) - E(_ax + j * _hx, _by)) * (U->get((_n - 1) * _m + j) - E(_ax + j * _hx, _ay)); //south border
	}

	for (int i = 1; i < _n - 1; i++)
	{
		e += (U->get(i * _m + 0) - E(_ax, _ay + i * _hy)) * (U->get(i * _m + 0) - E(_ax, _ay + i * _hy));			//east border
		e += (U->get(i * _m + _m - 1) - E(_bx, _ay + i * _hy)) * (U->get(i * _m + _m - 1) - E(_bx, _ay + i * _hy)); //west border
	}

	//check interior points error
	for (int i = 1; i < _n - 1; i++) //over rows
		for (int j = 1; j < _m - 1; j++)
		{ //over columns
			e += (U->get(i * _m + j) - E(_ax + j * _hx, _ay + i * _hy)) * (U->get(i * _m + j) - E(_ax + j * _hx, _ay + i * _hy));
		}

	return sqrt(e) / sqrt(_n * _m);
}

void _2DPoissonEquation::printU()
{
	U->printGrid();
}

double _2DPoissonEquation::getSolutionError()
{
	return U->getError();
}

_2DGrid *_2DPoissonEquation::getGrid() { return U; }

double *_2DPoissonEquation::getF() { return F; }

void _2DPoissonEquation::printF()
{
	int _n = U->getXlen(),
		_m = U->getYlen();

	for (int i = 0; i < _n; i++)
		for (int j = 0; j < _m; j++)
			printf("%f\n", F[i * _m + j]);
}
