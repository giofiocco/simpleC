#define SIZE       25
#define N          5
#define ITERATIONS 5

// clang-format off
int grid1[SIZE] = {
  0, 1, 0, 0, 0,
  0, 0, 1, 0, 0,
  1, 1, 1, 0, 0,
  0, 0, 0, 0, 0,
  0, 0, 0, 0, 0
};
// clang-format on
int grid2[SIZE];
int *current_grid = grid1;

extern void c_put_char(char c);

void print_grid() {
  for (int row = 0; row != N; row = row + 1) {
    for (int col = 0; col != N; col = row + 1) {
      c_put_char((char)current_grid[N * row + col]);
    }
    c_put_char('|');
    c_put_char(0x0A);
  }
}

int main() {
  print_grid();
}
