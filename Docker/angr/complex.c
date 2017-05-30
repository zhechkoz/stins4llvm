#include <stdio.h>
#include <unistd.h>
#include <string.h>

int isValidLicenseKey(const char* input) {
  // Check for the correct content
  if ( input[0] != 'V') {return 0;}
  if ( input[1] != 'a') {return 0;}
  if ( input[2] != 'l') {return 0;}
  if ( input[3] != 'i') {return 0;}
  if ( input[4] != 'd') {return 0;}
  if ( input[5] != 'L') {return 0;}
  if ( input[6] != 'i') {return 0;}
  if ( input[7] != 'c') {return 0;}
  if ( input[8] != 'e') {return 0;}
  if ( input[9] != 'n') {return 0;}
  if (input[10] != 's') {return 0;}
  if (input[11] != 'e') {return 0;}
  if (input[12] != 'K') {return 0;}
  if (input[13] != 'e') {return 0;}
  if (input[14] != 'y') {return 0;}
  if (input[15] != 0) {return 0;}

  return 1;
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
