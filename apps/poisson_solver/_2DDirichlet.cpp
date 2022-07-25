#include "_2DBoundaryFunction.hpp"
#include "_2DDirichlet.hpp"

using namespace std;

_2DDirichlet::_2DDirichlet(std::function<double(double, double)> gfun)
{
	g = gfun;
}

void _2DDirichlet::operator()(_2DGrid *grid)
{

	int _m = grid->getYlen(),
		_n = grid->getXlen();

	double _hx = grid->getXspacing(),
		   _hy = grid->getYspacing(),
		   *_u = grid->getU();

	double _by = grid->getby(),
		   _ay = grid->getay(),
		   _ax = grid->getax(),
		   _bx = grid->getbx();

	double mean = 0.;

	for (int j = 0; j < _m; j++)
	{
		_u[0 + j] = g(_ax + j * _hx, _by);			   //north border
		_u[(_n - 1) * _m + j] = g(_ax + j * _hx, _ay); //south border
	}

	for (int i = 1; i < _n - 1; i++)
	{
		_u[i * _m + 0] = g(_ax, _ay + i * _hy);		 //east border
		_u[i * _m + _m - 1] = g(_bx, _ay + i * _hy); //west border
	}

	// Average the boundary values, to come up with a reasonable
	// initial value for the interior.

	for (int j = 0; j < _m; j++)
	{
		mean += _u[0 + j];
	}

	for (int j = 0; j < _m; j++)
	{
		mean += _u[(_n - 1) * _m + j];
	}

	for (int i = 1; i < _n - 1; i++)
	{
		mean += _u[i * _m + 0];
	}

	for (int i = 1; i < _n - 1; i++)
	{
		mean += _u[i * _m + _m - 1];
	}

	mean = mean / (double)(2 * _n + 2 * _m - 4);

	for (int i = 1; i < _n - 1; i++)
		for (int j = 1; j < _m - 1; j++)
			_u[i * _m + j] = mean;
}