add_subdirectory(scene/baker/)
add_subdirectory(scene/scene_manager/)

add_executable(complete_renderer
  main.cpp
  App.cpp
  Renderer.cpp
  WorldRenderer.cpp
  StaticMeshRenderer.cpp
  TerrainGenerator.cpp
  GrassRenderer.cpp
)

target_link_libraries(complete_renderer
  PRIVATE glfw etna glm::glm wsi gui scene render_utils cr_scene)

target_add_shaders(complete_renderer
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

  shaders/static/cull_group.comp
  shaders/static/cull_single.comp
  shaders/static/render.frag
  shaders/static/render_group.vert
  shaders/static/render_single.vert
  shaders/static/reset_group_comms.comp

  shaders/cube.frag
  shaders/cube.vert

  shaders/grass/grass.frag
  shaders/grass/grass.vert
)
