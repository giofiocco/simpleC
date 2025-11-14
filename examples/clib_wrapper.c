#include <ctype.h>
#include <stdio.h>

#define U16(__n) ((__n) & 0xFFFF)

void c_put_char(char c) {
  switch (c) {
    case 1: putchar('#'); break;
    case '\n': putchar('\n'); break;
    default:
      putchar(isprint(c) ? c : ' ');
  }
}

void c_print_int(int n) {
  printf("%d", n);
}

int c_rng_next(int *rng) {
  *rng = U16(U16(36969 * (*rng)) + 18000);
  return *rng;
}

int mod(int a, int b) {
  return a % b;
}
