#ifndef _2DGRID_HPP
#define _2DGRID_HPP

using namespace std;

class _2DGrid
{

private:
	double ax,
		bx,
		ay,
		by;

	int m,
		n;

	double hx,
		hy;

	double Error;

public:
	double *U;

public:
	_2DGrid(double ax, double bx,
			double ay, double by,
			int m, int n);

	~_2DGrid();

	int getXlen();

	int getYlen();

	double get(int index);

	double getXspacing();

	double getYspacing();

	double getError();

	void setError(double e);

	void resetError();

	double getax();

	double getbx();

	double getay();

	double getby();

	double *getU();

	void printGrid();
};
#endif