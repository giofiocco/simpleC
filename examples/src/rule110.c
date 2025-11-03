#define LEN        10
#define ITERATIONS 16

char row[LEN];
char table[] = {0x00, 0x01, 0x01, 0x01, 0x00, 0x01, 0x01, 0x00};

extern void c_put_char(char c);

void print_row() {
  for (int i = 0; i != LEN; i = i + 1) {
    c_put_char(row[i]);
  }
  c_put_char('|');
  c_put_char(0x0A);
}

int main() {
  row[LEN - 1] = 0x01;

  for (int iter = ITERATIONS; iter != 0; iter = iter - 1) {
    int row0 = row[0];
    int prev = row[LEN - 1];
    int curr = row0;
    char *rowi = row;

    print_row();

    for (int i = LEN - 1; i != 0; i = i - 1) {
      *rowi = table[(prev << 2) + (curr << 1) + *(rowi + 1)];

      prev = curr;
      rowi = rowi + 1;
      curr = (int)(*rowi);
    }

    *rowi = table[(prev << 2) + (curr << 1) + row0];
  }
}
