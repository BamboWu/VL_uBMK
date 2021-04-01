#include "grid.hpp"
#include "fdtd.hpp"

#include <iostream>
#include <cstdlib>
#include <cstring>
#include <argp.h>

static argp_option options[] = {
	{"print", 'p', nullptr, 0, "Print result grid values" },
	{"dynamic", 'd', nullptr, 0, "Use dynamic queue resizing" },
#ifdef QT
	{"qthreads", 'q', "#", 0, "Use QThread pool scheduling" },
#endif // QT
#ifdef VL
	{"vlink", 'v', nullptr, 0, "Use VirtualLink queues" },
#endif // VL
	{ 0 },
};

static char args_doc[] = "x y z t";

struct Configuration {
	unsigned long args[4];
	unsigned long flags = 0;
	unsigned long threads = 0;
};

static error_t parse_opt(int key, char *arg, argp_state *state)
{
	Configuration *cfg = (Configuration *)state->input;

	switch (key) {
	case 'p':
		cfg->flags |= FLAG_PRT;
		break;
	case 'd':
		cfg->flags |= FLAG_DYN;
		break;
	case 'v':
		cfg->flags |= FLAG_VTL;
		break;
	case 'q': {
		cfg->flags |= FLAG_QTH;
		if (!arg)
			argp_usage(state);

		unsigned long th = std::strtoul(arg, nullptr, 0);
		const char *thval = std::to_string(th + 1).c_str();
		setenv("QT_NUM_SHEPHERDS", "1", 1);
		setenv("QT_HWPAR", thval, 1);
		setenv("QT_NUM_WORKERS_PER_SHEPHERD", thval, 1);
		break;
	}
	case ARGP_KEY_ARG:
		if (state->arg_num >= 4)
			argp_usage(state);
		cfg->args[state->arg_num] = std::strtoul(arg, nullptr, 0);
		break;
	case ARGP_KEY_END:
		if (state->arg_num < 4)
			argp_usage(state);
		break;
	default:
		return ARGP_ERR_UNKNOWN;
	}
	return 0;
}

static argp argp = { options, parse_opt, args_doc, nullptr};

int main(int argc, char **argv)
{
	Configuration cfg;
	argp_parse(&argp, argc, argv, 0, 0, &cfg);
	return fdtd(cfg.args[0], cfg.args[1], cfg.args[2], cfg.args[3], cfg.flags);
}
