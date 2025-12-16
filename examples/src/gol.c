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
int *back_grid = grid2;

extern void c_put_char(char c);
extern void c_print_int(int n);

int at(int row, int col) {
  if (row == -1 || row == N || col == -1 || col == N) {
    return 0;
  } else {
    return current_grid[row * N + col];
  }
}

void print_grid() {
  for (int row = 0; row != N; row = row + 1) {
    for (int col = 0; col != N; col = col + 1) {
      c_put_char((char)at(row, col));
    }
    c_put_char(0x0A);
  }
}

void iteration() {
  for (int row = 0; row != N; row = row + 1) {
    for (int col = 0; col != N; col = col + 1) {
      int n = at(row - 1, col - 1)
              + at(row - 1, col)
              + at(row - 1, col + 1)
              + at(row, col - 1)
              + at(row, col + 1)
              + at(row + 1, col - 1)
              + at(row + 1, col)
              + at(row + 1, col + 1);
      if (at(row, col)) {
        back_grid[row * N + col] = n == 2 || n == 3;
      } else {
        back_grid[row * N + col] = n == 3;
      }
    }
  }
  int *tmp = current_grid;
  current_grid = back_grid;
  back_grid = tmp;
}

int main() {
  print_grid();

  for (int i = 0; i != ITERATIONS; i = i + 1) {
    iteration();
    print_grid();
  }
}
