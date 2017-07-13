#include <cstring>
#include <cstdio>
#include <ctime>
#include <cstdlib>

#include "backtrace.h"
#include "backtrace-supported.h"

#include <execinfo.h>
#include <unistd.h>
#include <signal.h>

#define DEBUG 1
#define STACKTRACE 256
#define REPORT 2


struct info {
    char *function;
};

struct bdata {
    struct info *all;
    size_t index;
    size_t max;
};

extern "C" void initRandom() {
	std::srand(std::time(0));
}

extern "C" bool cmpstr(char *first, char *second) {
	return std::strcmp(first, second);
}

static void backtraceErrorCallback(void *vdata, const char *msg, int errnum) {
    fprintf (stderr, "%s", msg);
    if (errnum > 0)
        fprintf (stderr, ": %s", strerror (errnum));
    fprintf (stderr, "\n");
}

static int collectBacktrace (void *vdata, uintptr_t pc,
	      const char *filename, int lineno, const char *function) {
    struct bdata *data = (struct bdata *) vdata;
    struct info *p;

    if (data->index >= data->max) {
        fprintf (stderr, "Callback called too many times\n");
        return 1;
    }

    p = &data->all[data->index];
    if (function != NULL) {
        p->function = strdup(function);
        ++data->index;
    }
    
    return 0;
}

static void backtraceCallbackCreate(void *data, const char *msg, int errnum) {
    fprintf (stderr, "%s", msg);
    if (errnum > 0)
        fprintf (stderr, ": %s", strerror (errnum));
    fprintf (stderr, "\n");

    exit (EXIT_FAILURE);
}

backtrace_state *state = backtrace_create_state("", BACKTRACE_SUPPORTED, backtraceCallbackCreate, NULL);
// Calculate backtrace
extern "C" bool checkTrace(char *functionName) {
	struct info all[STACKTRACE];
    struct bdata data = { &all[0], 0, STACKTRACE };
    
	backtrace_full(state, 0, collectBacktrace, backtraceErrorCallback, &data);
    
    bool found = false;
    
    for (unsigned int i = 0; i < data.index; i++) {
        if (strcmp(data.all[i].function, functionName) == 0) {
            found = true;
        }
    }

    for (unsigned int i = 0; i < data.index; i++) {
        free(data.all[i].function);    
    }
    
	return found;
}

int generateRandom10() {
	return std::rand() % 10;
}

extern "C" void report() {
	#if DEBUG
		puts("Hash corrupted!");
	#endif

	int randNum = generateRandom10();

	#if DEBUG
		printf("Should report: %s (%d)\n", randNum >= REPORT ? "TRUE" : "FALSE", randNum);
	#endif

	// In 20% of the times don't do anything
	if (randNum < REPORT) {
		return;
	}

	// In the other 80% spawn a thread, sleep
	// and kill the process
	int parent = getpid();
	pid_t pid = fork();

	if (pid == 0) {
		randNum = generateRandom10();

		#if DEBUG
			printf("Kill in %d seconds...\n", randNum);
		#endif

		sleep(randNum);

		kill(parent, SIGKILL);
		exit(0);
	}
}

#undef DEBUG
#undef STACKTRACE
#undef REPORT
