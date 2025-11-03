#include <ctype.h>
#include <stdio.h>

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
