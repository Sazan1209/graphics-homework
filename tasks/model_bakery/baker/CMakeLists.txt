
add_executable(model_bakery_baker main.cpp baker.cpp)
  target_link_libraries(model_bakery_baker PRIVATE tinygltf glm::glm tinygltf etna)
