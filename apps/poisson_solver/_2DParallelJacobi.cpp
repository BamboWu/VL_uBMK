#include "_2DParallelJacobi.hpp"
#include <functional>
#include "opt.hpp"

#include <raft>

using namespace std;

_2DParallelJacobi::_2DParallelJacobi(double tol,
									 int it,
									 int nw,
									 int chk,
									 int i,
									 int j) : _2DIterativePoissonSolver(tol, it)
{

	numberOfWorkers = nw;
	chunk = chk;
	ci = i;
	cj = j;
}

inline int _2DParallelJacobi::getNumberOfWorkers() { return numberOfWorkers; }

inline int _2DParallelJacobi::getChunk() { return chunk; }

//====================RaftLib Structures========================//
class reduce_kernel : public raft::parallel_k
{
public:
    reduce_kernel(int ninputs, double* err) : raft::parallel_k(), perr(err)
    {
        for (int i = 0; ninputs > i; ++i) {
            addPortTo<double>(input);
        }
    }
    virtual raft::kstatus run()
    {
        for (auto& port : input) {
            if (0 < port.size()) {
                double tmp;
                port.pop<double>(tmp);
                *perr += tmp;
            }
        }
        return raft::proceed;
    }
private:
    double *perr;
};

class jacobi_kernel : public raft::kernel
{
public:
    jacobi_kernel(long int m, double *__restrict__ uold,
            double *__restrict__ unew, double *f, double x2x, double y2y) :
        raft::kernel(), _m(m), _uold(uold), _unew(unew), _f(f), hhxx(x2x),
        hhyy(y2y)
    {
        input.addPort<int>("input");
        output.addPort<double>("output");
    }
    jacobi_kernel(const jacobi_kernel& other) :
        raft::kernel(), _m(other._m), _uold(other._uold), _unew(other._unew),
        _f(other._f), hhxx(other.hhxx), hhyy(other.hhyy)
    {
        input.addPort<int>("input");
        output.addPort<double>("output");
    }
    ~jacobi_kernel() = default;
    virtual raft::kstatus run()
    {
        int i;
        input["input"].pop<int>(i);
        double error_tmp = 0;

		double *__restrict__ __uold = (double *)__builtin_assume_aligned(_uold, ALIGNMENT);
		double *__restrict__ __unew = (double *)__builtin_assume_aligned(_unew, ALIGNMENT);

		for (int j = 1; j < _m - 1; j++)
		{
			const long double val = 0.5 * (hhxx * hhyy * _f[i * _m + j] + hhyy * (__uold[i * _m + j - 1] + __uold[i * _m + j + 1]) + hhxx * (__uold[(i - 1) * _m + j] + __uold[(i + 1) * _m + j])) / (hhyy + hhxx);
			__unew[i * _m + j] = val;
			error_tmp += (__uold[i * _m + j] - __unew[i * _m + j]) * (__uold[i * _m + j] - __unew[i * _m + j]);
		}
        output["output"].push<double>(error_tmp);
        return raft::proceed;
    }
    CLONE(); // enable cloning
private:
    long int _m;
    double *__restrict__ _uold;
    double *__restrict__ _unew;
    double * _f;
    double hhxx;
    double hhyy;
};

class workset_kernel : public raft::parallel_k
{
public:
    workset_kernel(int noutputs, int nchunks) :
        raft::parallel_k(), cnt(nchunks - 1)
    {
        for (int i = 0; noutputs > i; ++i) {
            addPortTo<int>(output);
        }
    }
    virtual raft::kstatus run()
    {
        for (auto& port : output) {
            if (port.space_avail()) {
                if (0 >= cnt) {
                    return raft::stop;
                }
                port.push(cnt--);
            }
        }
        return raft::proceed;
    }
private:
    int cnt;
};

inline void _2DParallelJacobi::operator()(_2DPoissonEquation *eq)
{

	_2DGrid *grid = eq->getGrid();

	long int _n = grid->getXlen(),
			 _m = grid->getYlen();

	double *__restrict__ _unew = grid->getU(),
						 *__restrict__ _uold = (double *)malloc(sizeof(double) * _n * _m);

	double *_f = eq->getF();

	double hhxx = grid->getXspacing() * grid->getXspacing(),
		   hhyy = grid->getYspacing() * grid->getYspacing();

	double Error = 10 * getTolerance();

	while (getActualNumberOfIterations() < getMaxNumberOfIterations() && Error > getTolerance())
	{

		Error = 0.0;

        std::swap(_uold, _unew);

        reduce_kernel reduce_k(getNumberOfWorkers(), &Error);
        workset_kernel workset_k(getNumberOfWorkers(), _n - 2);
        jacobi_kernel jacobi_k(_m, _uold, _unew, _f, hhxx, hhyy);

        raft::map m;

        m += workset_k <= jacobi_k >= reduce_k;

        m.exe< partition_dummy,
#if USE_UT or USE_QTHREAD
            pool_schedule,
#else
            simple_schedule,
#endif
#ifdef VL
            vlalloc,
#elif STDALLOC
            stdalloc,
#else
            dynalloc,
#endif
            no_parallel >();

		Error = sqrt(Error) / sqrt(_n * _m);

		incrActualNumberOfIterations();
	}

	grid->setError(Error);
	grid->U = _unew;
	free(_uold);
}
