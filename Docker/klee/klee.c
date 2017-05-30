#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "klee/klee.h"

void hicks(int a, int b, int c) {
  int x = 0;
  int y = 0;
  int z = 0;

  if (a != 0) {
    x -= 2;
  }
  if (b < 5) {
    if (a == 0 && c != 0) {
      y = 1;
    }
    z = 2;
  }
  assert(x + y + z != 3);
}

int main(void) {

  int a = klee_int("a");
  int b = klee_int("b");
  int c = klee_int("c");
  hicks(a, b, c);

  return 0;
}

