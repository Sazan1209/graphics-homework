#include "baker.hpp"
#include <iostream>

int main(int argc, char* argv[])
{
  if (argc != 3)
  {
    std::cerr << "Expected exactly two arguments\n";
    return 1;
  }
  Baker baker;
  baker.bakeScene(argv[1], argv[2]);
}
