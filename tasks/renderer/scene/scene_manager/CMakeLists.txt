add_library(cr_scene SceneLoader.cpp)

target_include_directories(cr_scene PUBLIC ..)

target_link_libraries(cr_scene PUBLIC glm::glm tinygltf etna)
