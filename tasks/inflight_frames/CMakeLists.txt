#include(${PROJECT_SOURCE_DIR}/cmake/common.cmake)

add_executable(inflight_frames
  main.cpp
  App.cpp
)

target_link_libraries(inflight_frames
  PRIVATE glfw etna glm::glm wsi gui scene)


target_add_shaders(inflight_frames
  shaders/toy.frag
  shaders/quad.vert
  shaders/proc_texture.frag
)

