#include <algorithm>
#include <cstring>
#include <string>
#include <ctime>
#include <cstdlib>
#include <vector>
#include "backtrace.h"
#include "backtrace-supported.h"

#include <execinfo.h>
#include <unistd.h>
#include <signal.h>

#define DEBUG 1
#define REPORT 2

extern "C" void initRandom() {
	std::srand(std::time(0));
}

extern "C" bool cmpstr(char *first, char *second) {
	return std::strcmp(first, second);
}

static int collectBacktrace (void *vdata, uintptr_t pc,
	      const char *filename, int lineno, const char *function) {

    std::vector<std::string> *data = (std::vector<std::string> *) vdata;
    if (function != NULL) {
        data->push_back(function);
    } 
    
    return 0;
}

// Calculate backtrace
extern "C" bool checkTrace(char *functionName) {
	std::vector<std::string> trace;
	backtrace_state *state = backtrace_create_state ("", BACKTRACE_SUPPORTS_THREADS, NULL, NULL);
	backtrace_full (state, 0, collectBacktrace, NULL, &trace); // Skip the first two frames
    
	bool functionOnStack = std::find(trace.begin(), trace.end(), functionName) != trace.end();

	return functionOnStack;
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
