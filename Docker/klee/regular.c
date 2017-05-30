#include <stdio.h>
#include <stdlib.h>
#include <assert.h>

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

int main(int argc, char** argv) {
  if (argc != 4) {
    puts("Error: Please provide four integers");
    return -1;
  }

  int a = atoi(argv[1]);
  int b = atoi(argv[2]);
  int c = atoi(argv[3]);
  hicks(a, b, c);

  return 0;
}

