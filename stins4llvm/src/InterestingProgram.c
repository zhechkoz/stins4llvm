#include <stdio.h>
#include <string.h>
#include <stdlib.h>
void InterestingProcedure() {
    printf("\t This is an interesting procedure\n");
}

void a();
void b();
void c();
void d();
void e();

void a() {
	b();
}

void b() {
	c();
}

void c() {
	d();
}

void d() {
	e();
}

void e() {
	InterestingProcedure();
}
void print(char *message) {
	InterestingProcedure();
    printf("%s\n", message);
}

int main(int argc, char** argv) {
	char inp[8];
	InterestingProcedure();
	a();
	gets(inp);
    return 0;
}

