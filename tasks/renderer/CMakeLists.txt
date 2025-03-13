add_subdirectory(scene/baker/)

add_executable(complete_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  TerrainGenerator.cpp
# StaticMeshRenderer.cpp
)

target_link_libraries(complete_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils)

target_add_shaders(complete_renderer
  shaders/static_mesh.frag
  shaders/static_mesh.vert
  shaders/resolve.comp

  shaders/terrain/perlin.comp
  shaders/terrain/normal.comp
  shaders/terrain/lightgen.comp
  shaders/terrain/terrain.vert
  shaders/terrain/terrain.tesc
  shaders/terrain/terrain.tese
  shaders/terrain/terrain.frag

  shaders/tonemap/downscale.comp
  shaders/tonemap/minmax.comp
  shaders/tonemap/hist.comp
  shaders/tonemap/equalize.comp
  shaders/tonemap/cumsum.comp
  shaders/tonemap/tonemap.comp

  shaders/cube.frag
  shaders/cube.vert
)
