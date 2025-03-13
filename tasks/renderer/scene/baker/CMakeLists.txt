
add_executable(complete_renderer_baker main.cpp baker.cpp)
target_link_libraries(complete_renderer_baker PRIVATE tinygltf glm::glm tinygltf etna)
