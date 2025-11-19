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
  for (int i = 0; i != SIZE; i = i + 1) {
    c_put_char((char)current_grid[i]);
  }
}

int main() {
  print_grid();
}
