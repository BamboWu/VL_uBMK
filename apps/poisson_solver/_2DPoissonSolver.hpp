#ifndef _2DPOISSONSOLVER_HPP
#define _2DPOISSONSOLVER_HPP

#include "_2DPoissonEquation.hpp"

using namespace std;

class _2DPoissonSolver
{

public:
	virtual void operator()(_2DPoissonEquation *eq){};
};

#endif