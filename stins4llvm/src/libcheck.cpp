#include <algorithm>
#include <cstring>
#include <string>
#include <ctime>
#include <cstdlib>
#include <vector>

#include <execinfo.h>
#include <unistd.h>
#include <signal.h>

#define DEBUG 1
#define STACKTRACE 256
#define REPORT 2

const std::string libcStartMain = "__libc_start_main";

extern "C" void initRandom() {
	std::srand(std::time(0));
}

extern "C" bool cmpstr(char *first, char *second) {
	return std::strcmp(first, second);
}

// Calculate backtrace
extern "C" bool checkTrace(char *functionName) {
	std::vector<std::string> trace;
	void *array[STACKTRACE];
	size_t size;

	// get void*'s for all entries on the stack
	size = backtrace(array, STACKTRACE);

	char **traces = backtrace_symbols(array, size);

	for(unsigned int i = 0; i < size; i++) {
		char *begin = traces[i];
		for (char* p = traces[i]; *p; p++) {
			if (*p == '(')
				begin = p + 1;
			else if(*p == '+') {
				*p = '\0';
				break;
			}
		}
		std::string funcName(begin);

		// Skip everything after libc functions
		if (funcName == libcStartMain) { break; }

		trace.push_back(funcName);
	}

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
