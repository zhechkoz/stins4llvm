#include <algorithm>
#include <cstring>
#include <iostream>
#include <execinfo.h>

#define STACKTRACE 256

const std::string libcStartMain = "__libc_start_main";

// Calculate backtrace
extern "C" bool checkTrace(std::string functionName) {
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
  	
  	bool functionOnStack = std::find(trace.begin(), trace.end(), functionName) != trace.end();
  	
  	return functionOnStack;
} 

extern "C" void report() {
	
}

