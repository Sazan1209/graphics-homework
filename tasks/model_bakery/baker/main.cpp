#include "baker.hpp"
#include <iostream>

int main(int, char*[])
{
  Baker baker;
  baker.bakeScene(GRAPHICS_COURSE_RESOURCES_ROOT "/scenes/low_poly_dark_town/scene.gltf");
}
