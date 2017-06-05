#include <stdio.h>
#include <string.h>
#include <stdlib.h>

char* isValidLicenseKey(const char* input) {
	if(strcmp(input, "ValidLicenseKey") == 0) {
		return "#";
	}
	
	return "@";
}

short add(short a, short b) {
	return a < b ? a + b : b + a;
}

long sub(long a, long b) {
	return a - b;
}

int max(int a, int b, int c) {
	if (a > b) {
		if (a > c) {
			// a > b && a > c
			return a;
		} else {
			// a > b && a <= c;
			return c;
		}
	} else {
		if (b > c) {
			// b >= a && b > c
			return b;
		} else {
			// b >= a && b <= c
			return c;
		}
	}
}

int min(int a, int b, int c) {
	if (a < b) {
		if (a < c) {
			// a < b && a < c
			return a;
		} else {
			// a < b && a <= c;
			return c;
		}
	} else {
		if (b < c) {
			// b >= a && b > c
			return b;
		} else {
			// b >= a && b <= c
			return c;
		}
	}
}


void InterestingProcedure() {
    printf("\t This is an interesting procedure\n");
}

void print(char *message) {
	InterestingProcedure();
    printf("%s\n", message);
}

int main(int argc, char** argv) {

	InterestingProcedure();
	
	while(1) {}
    return 0;
}

