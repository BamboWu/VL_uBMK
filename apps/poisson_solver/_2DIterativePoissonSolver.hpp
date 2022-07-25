#ifndef _2DITERATIVEPOISSONSOLVER_HPP
#define _2DITERATIVEPOISSONSOLVER_HPP

#include "_2DPoissonSolver.hpp"

class _2DIterativePoissonSolver : public _2DPoissonSolver
{

public:
	double tolerance;

	int maxNumberOfIterations,
		actualNumberOfIterations;

public:
	virtual void operator()(_2DPoissonEquation *eq){};

	_2DIterativePoissonSolver(double tol, int it)
	{
		tolerance = tol;
		maxNumberOfIterations = it;
		actualNumberOfIterations = 0;
	}

	void copyVector(double *src, double *dst, int n)
	{

		for (int i = 0; i < n; i++)
			dst[i] = src[i];
	}

	double getTolerance()
	{
		return tolerance;
	}

	int getMaxNumberOfIterations()
	{
		return maxNumberOfIterations;
	}

	int getActualNumberOfIterations()
	{
		return actualNumberOfIterations;
	}

	void incrActualNumberOfIterations() { actualNumberOfIterations++; }
};

#endif