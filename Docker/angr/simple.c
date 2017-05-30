#include <stdio.h>
#include <unistd.h>
#include <string.h>

int isValidLicenseKey(const char* input) {
  return strcmp(input, "ValidLicenseKey") == 0;
}

int main(int argc, char** argv) {

  if (argc != 2) {
    puts("Please provide your license key as an argument!");
    return -1;
  }

  if (isValidLicenseKey(argv[1])) {
    puts("This license is valid!");
  } else {
    puts("Nope ...");
    return 1;
  }
}
