#include "baker.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 2)
  {
    std::cerr << "Expected exactly one argument\n";
    return 1;
  }
  Baker baker;
  baker.bakeScene(argv[1]);
}
