#include <stdio.h>
#include <stdlib.h>
#include <stdlib.h>

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


int simpleCalc(int a, char op, int b) {
  switch(op) {
    case '+': return a + b; break;
    case '-': return a - b; break;
    case '*': return a * b; break;
    case '/': return a / b; break;
    case '%': return a % b; break;
    default: puts("Invalid operand"); return -1;
  }
}

int* fiveInts() {
  int* ptr = (int *) malloc(5 * sizeof(int));
  for (int i = 0; i < 5; i++) {
    ptr[i] = i;
  }
  return ptr;
}

int main(int argc, char** argv) {

  if (argc != 4) {
    puts("Please provide a number, an operator and a number!");
    return -1;
  }

  printf("%d\n", simpleCalc(atoi(argv[1]), argv[2][0], atoi(argv[3])));
}
