#include <algorithm>
#include <cstring>
#include <iostream>
#include <execinfo.h>

#define STACKTRACE 256

const std::string libcStartMain = "__libc_start_main";

// Calculate backtrace
std::vector<std::string> stackTrace() {
	std::vector<std::string> trace;
	void *array[STACKTRACE];
  	size_t size;

  	// get void*'s for all entries on the stack
  	size = backtrace(array, STACKTRACE);

  	char **traces = backtrace_symbols(array, size);
  	
  	// Skip the check and stackTrace
  	for(unsigned int i = 2; i < size; i++) {
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
  		
  		// Skip libc functions
  		if (funcName == libcStartMain) {
  			break;
  		}
  		
  		trace.push_back(funcName);
  	}
  	
  	std::reverse(trace.begin(), trace.end());
  	
	return trace;
} 

extern "C" bool check(char *validHash, bool hasToCheck) {
	return true;
}

extern "C" void report(bool valid) {
	
}

