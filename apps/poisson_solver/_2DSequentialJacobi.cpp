#include "_2DSequentialJacobi.hpp"
#include "opt.hpp"
#include <algorithm>
#include <stdio.h>

using namespace std;

_2DSequentialJacobi::_2DSequentialJacobi(double tol,
										 int it) : _2DIterativePoissonSolver(tol, it) {}

//Given a Poisson equation E, we compute
//		U = J(E)
//where J is the Jacobi iterative method,
//and U the solution to E
void _2DSequentialJacobi::operator()(_2DPoissonEquation *eq)
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

	//long int CI = I,
	//	     CJ = J;

	//copyVector(_unew, _uold, _n*_m);

	while (getActualNumberOfIterations() < getMaxNumberOfIterations() && Error > getTolerance())
	{

		Error = 0.0;

		copyVector(_unew, _uold, _n * _m);

		for (int i = 1; i < _n - 1; i++)
		{
			for (int j = 1; j < _m - 1; j++)
			{
				const double val = 0.5 * (hhxx * hhyy * _f[i * _m + j] + hhxx * (_uold[i * _m + j - 1] + _uold[i * _m + j + 1]) + hhyy * (_uold[(i - 1) * _m + j] + _uold[(i + 1) * _m + j])) / (hhyy + hhxx);

				_unew[i * _m + j] = val;
				Error += (_uold[i * _m + j] - val) * (_uold[i * _m + j] - val);
			}
		}

		// BLOCKED VERSION

		/*for(long ii = 1; ii < _n; ii+=CI){
	      for(long jj = 1; jj < _m; jj+=CJ){
	        for(long i = ii; i < min(ii + CI, _n-1); i++){
	          for (long j = jj; j < min(jj + CJ, _m-1); j++){

	          		const long double val = 0.5 * ( hhxx*hhyy*_f[i*_m + j] - hhyy*(_uold[i*_m + j-1] + _uold[i*_m + j+1]) -
								  					hhxx*(_uold[(i-1)*_m + j] + _uold[(i+1)*_m + j]))/(hhxx + hhxx);

					_unew[i*_m + j] = val;
					Error += (_uold[i*_m + j] - val) * (_uold[i*_m + j] - val);
	            }
	        }
	      }
	    }*/

		Error = sqrt(Error) / sqrt(_n * _m);

		//std::swap(_unew, _uold);
		incrActualNumberOfIterations();

		//printf("%i %e\n", getActualNumberOfIterations(), Error);
	}

	//std::swap(_unew, _uold);
	//copyVector(_unew, _uold, _n*_m);

	grid->setError(Error);

	free(_uold);
}