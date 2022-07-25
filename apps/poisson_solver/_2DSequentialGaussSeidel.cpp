#include "_2DSequentialGaussSeidel.hpp"
#include <stdio.h>

using namespace std;

_2DSequentialGaussSeidel::_2DSequentialGaussSeidel(double tol,
												   int it) : _2DIterativePoissonSolver(tol, it) {}

void _2DSequentialGaussSeidel::operator()(_2DPoissonEquation *eq)
{

	_2DGrid *grid = eq->getGrid();

	int _n = grid->getXlen(),
		_m = grid->getYlen();

	double *_unew = grid->getU(),
		   *_uold = (double *)malloc(sizeof(double) * _n * _m);

	double *_f = eq->getF();

	double hhxx = grid->getXspacing() * grid->getXspacing(),
		   hhyy = grid->getYspacing() * grid->getYspacing();

	double Error = 10 * getTolerance();

	//copyVector(_unew, _uold, _n*_m);

	while (getActualNumberOfIterations() < getMaxNumberOfIterations() && Error > getTolerance())
	{

		Error = 0.0;

		copyVector(_unew, _uold, _n * _m);

		for (int i = 1; i < _n - 1; i++)
			for (int j = 1; j < _m - 1; j++)
			{
				const long double val = 0.5 * (hhxx * hhyy * _f[i * _m + j] + hhyy * (_unew[i * _m + j - 1] + _uold[i * _m + j + 1]) + hhxx * (_unew[(i - 1) * _m + j] + _uold[(i + 1) * _m + j])) / (hhyy + hhxx);
				_unew[i * _m + j] = val;
				Error += (_uold[i * _m + j] - val) * (_uold[i * _m + j] - val);
			}

		//std::swap(_unew, _uold);

		incrActualNumberOfIterations();

		Error = sqrt(Error) / sqrt(_n * _m);

		//printf("%i %e\n", getActualNumberOfIterations(), Error);
	}

	grid->setError(Error);

	//std::swap(_unew, _uold);
	free(_uold);
}