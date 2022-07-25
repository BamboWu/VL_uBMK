#include "_2DSequentialGaussSeidelRedBlack.hpp"
#include <stdio.h>

using namespace std;

_2DSequentialGaussSeidelRedBlack::_2DSequentialGaussSeidelRedBlack(double tol,
																   int it) : _2DIterativePoissonSolver(tol, it) {}

void _2DSequentialGaussSeidelRedBlack::operator()(_2DPoissonEquation *eq)
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

	while (getActualNumberOfIterations() < getMaxNumberOfIterations() && Error > getTolerance())
	{

		Error = 0.0;

		//printf("ti rompi prima della copia?\n");

		copyVector(_unew, _uold, _n * _m);

		//printf("ti rompi dopo la copia?\n");

		//printf("vado con i rossi!\n");
		//updating red points
		for (int i = 1; i < _n - 1; i++)
			for (int j = 1; j < _m - 1; j++)
				if ((i + j) % 2 == 1)
				{
					//const long double val = 0.5 * ( -hhxx*hhyy*_f[i*_m + j] + hhyy*(_uold[i*_m + j-1] + _uold[i*_m + j+1]) +
					//					 	       hhxx*(_uold[(i-1)*_m + j] + _uold[(i+1)*_m + j]) )/(hhxx + hhxx);
					const double val = 0.5 * (hhxx * hhyy * _f[i * _m + j] + hhxx * (_uold[i * _m + j - 1] + _uold[i * _m + j + 1]) + hhyy * (_uold[(i - 1) * _m + j] + _uold[(i + 1) * _m + j])) / (hhyy + hhxx);

					_unew[i * _m + j] = val;
					Error += (_uold[i * _m + j] - val) * (_uold[i * _m + j] - val);
				}
		//printf("vado con i neri!\n");
		//updating black points
		for (int i = 1; i < _n - 1; i++)
			for (int j = 1; j < _m - 1; j++)
				if ((i + j) % 2 == 0)
				{
					const long double val = 0.5 * (hhxx * hhyy * _f[i * _m + j] + hhyy * (_unew[i * _m + j - 1] + _unew[i * _m + j + 1]) + hhxx * (_unew[(i - 1) * _m + j] + _unew[(i + 1) * _m + j])) / (hhxx + hhxx);

					Error += (_unew[i * _m + j] - val) * (_unew[i * _m + j] - val);
					_unew[i * _m + j] = val;
				}

		//printf("ma i neri li hai finiti??\n");

		//std::swap(_unew, _uold);
		//copyVector(_unew, _uold, _n*_m);

		incrActualNumberOfIterations();

		//printf("ho incrementato!!!\n");

		Error = sqrt(Error) / sqrt(_n * _m); //norm 2 of absolute error ||uold - unew||
											 //printf("%i %e\n", getActualNumberOfIterations(), Error);

		//printf("qui ci arrivi?\n");
	}

	grid->setError(Error);
	//std::swap(_unew, _uold);
	free(_uold);
}