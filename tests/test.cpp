#include <cstdio>
#include <cstdlib>

int compute(int a, int b) {
  int sum  = a + b;
  int diff = a - b;
  
  if (sum > 10) {
    return sum * diff;
  }
  
  return sum + diff;
}

int main(int argc, char *argv[]) {
  if (argc < 3) {
    std::printf("Usage: %s <a> <b>\n", argv[0]);
    return 1;
  }

  int a = std::atoi(argv[1]);
  int b = std::atoi(argv[2]);

  int result = compute(a, b);
  
  std::printf("Result: %d\n", result);
  
  return 0;
}