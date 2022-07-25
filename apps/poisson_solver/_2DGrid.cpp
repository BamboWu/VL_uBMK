#include "_2DGrid.hpp"
#include <stdlib.h>
#include <stdio.h>

using namespace std;

_2DGrid::_2DGrid(double _ax, double _bx,
				 double _ay, double _by,
				 int _m, int _n)
{

	ax = _ax;
	bx = _bx;
	ay = _ay;
	by = _by;

	m = _m;
	n = _n;

	hx = (bx - ax) / n;
	hy = (by - ay) / m;

	Error = 0.;

	U = (double *)calloc(n * m, sizeof(double));
}

_2DGrid::~_2DGrid() { free(U); }

double _2DGrid::get(int index) { return U[index]; }

int _2DGrid::getXlen() { return n; }

int _2DGrid::getYlen() { return m; }

double _2DGrid::getXspacing() { return hx; }

double _2DGrid::getYspacing() { return hy; }

double _2DGrid::getError() { return Error; }

double *_2DGrid::getU() { return U; }

void _2DGrid::setError(double e) { Error += e; }

void _2DGrid::resetError() { Error = 0.0; }

double _2DGrid::getax() { return ax; }

double _2DGrid::getbx() { return bx; }

double _2DGrid::getay() { return ay; }

double _2DGrid::getby() { return by; }

void _2DGrid::printGrid()
{
	int _n = getXlen(),
		_m = getYlen();

	double _ax = getax(),
		   _ay = getay();

	double _hx = getXspacing(),
		   _hy = getYspacing();

	for (int i = 0; i < _n; i++)
		for (int j = 0; j < _m; j++)
			printf("%f %f %f\n", _ax + j * _hx, _ay + i * _hy, get(i * m + j));
}
