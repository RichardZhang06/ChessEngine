#include <iostream>

#include "uci.h"

int main() {
  std::ios::sync_with_stdio(false);
  Uci uci([](const std::string& line) { std::cout << line << '\n' << std::flush; });
  uci.loop(std::cin);
  return 0;
}
