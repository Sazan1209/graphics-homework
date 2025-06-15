#include "App.hpp"

#include <tracy/Tracy.hpp>
#include "gui/ImGuiRenderer.hpp"

static glm::vec2 randomGradient(glm::ivec2 coord)
{
  const uint32_t w = 32u;
  const uint32_t s = w / 2u; // rotation width
  uint32_t a = uint32_t(coord.x);
  uint32_t b = uint32_t(coord.y);
  a *= 3284157443u;
  b ^= a << s | a >> (w - s);
  b *= 1911520717u;
  a ^= b << s | b >> (w - s);
  a *= 2048419325u;
  float random = float(a) * (3.14159265 / float(~(~0u >> 1u))); // in [0, 2*Pi]
  glm::vec2 v;
  v.x = glm::cos(random);
  v.y = glm::sin(random);
  return v;
}

static float dotGridGradient(glm::vec2 pixCoord, glm::ivec2 gridCoord)
{
  glm::vec2 gradient = randomGradient(gridCoord);
  glm::vec2 offset = pixCoord - glm::vec2(gridCoord);

  return glm::dot(offset, gradient);
}

static float perlin(glm::vec2 pixCoord)
{
  int x0 = int(glm::floor(pixCoord.x));
  int x1 = x0 + 1;
  int y0 = int(glm::floor(pixCoord.y));
  int y1 = y0 + 1;

  glm::vec2 s = pixCoord - glm::vec2(x0, y0);
  s = (3.0f - s * 2.0f) * s * s;

  float n0 = dotGridGradient(pixCoord, glm::ivec2(x0, y0));
  float n1 = dotGridGradient(pixCoord, glm::ivec2(x1, y0));
  float ix0 = glm::mix(n0, n1, s.x);

  n0 = dotGridGradient(pixCoord, glm::ivec2(x0, y1));
  n1 = dotGridGradient(pixCoord, glm::ivec2(x1, y1));
  float ix1 = glm::mix(n0, n1, s.x);

  return glm::mix(ix0, ix1, s.y);
}

static float calc_terrain_height(glm::vec2 position)
{
  position.y = -position.y;
  position += glm::vec2(512.0f, 512.0f);
  position *= 4.0f;
  float res = glm::mix(perlin(position / 1024.0f), perlin(position / 128.0f), 0.01f);
  res = glm::mix(res, perlin(position / 16.0f), 0.005f);
  return res * 2.0f * 100.0f - 5.0f + 1.8f;
}

App::App()
{
  glm::uvec2 initialRes = {1280, 720};
  mainWindow = windowing.createWindow(OsWindow::CreateInfo{
    .resolution = initialRes,
  });

  renderer.reset(new Renderer(initialRes));

  auto instExts = windowing.getRequiredVulkanInstanceExtensions();
  renderer->initVulkan(instExts);

  auto surface = mainWindow->createVkSurface(etna::get_context().getInstance());

  renderer->loadScene(GRAPHICS_COURSE_RESOURCES_ROOT "/scenes_baked/Boombox/BoomBox_baked.gltf");
  renderer->initFrameDelivery(std::move(surface), [this]() { return mainWindow->getResolution(); });
  ImGuiRenderer::enableImGuiForWindow(mainWindow->native());

  mainCam.zFar = 1024.0f;
  mainCam.zNear = 0.05f;
  mainCam.lookAt({0, 0, 0}, {0, 0, -1}, {0, 1, 0});
}

void App::run()
{
  double lastTime = windowing.getTime();
  while (!mainWindow->isBeingClosed())
  {
    const double currTime = windowing.getTime();
    const float diffTime = static_cast<float>(currTime - lastTime);
    lastTime = currTime;

    windowing.poll();

    processInput(diffTime);

    drawFrame();

    FrameMark;
  }
}

void App::processInput(float dt)
{
  ZoneScoped;

  if (mainWindow->keyboard[KeyboardKey::kEscape] == ButtonState::Falling)
    mainWindow->askToClose();

  if (is_held_down(mainWindow->keyboard[KeyboardKey::kLeftShift]))
    camMoveSpeed = 10;
  else
    camMoveSpeed = 0.1;

  if (mainWindow->mouse[MouseButton::mbRight] == ButtonState::Rising)
    mainWindow->captureMouse = !mainWindow->captureMouse;

  moveCam(mainCam, mainWindow->keyboard, dt);
  mainCam.position.y = calc_terrain_height({mainCam.position.x, mainCam.position.z});
  if (mainWindow->captureMouse)
    rotateCam(mainCam, mainWindow->mouse, dt);

  renderer->debugInput(mainWindow->keyboard);
}

void App::drawFrame()
{
  ZoneScoped;

  renderer->update(FramePacket{
    .mainCam = mainCam,
    .currentTime = static_cast<float>(windowing.getTime()),
  });
  renderer->drawFrame();
}

void App::moveCam(Camera& cam, const Keyboard& kb, float dt)
{
  // Move position of camera based on WASD keys, and FR keys for up and down

  glm::vec3 dir = {0, 0, 0};

  if (is_held_down(kb[KeyboardKey::kS]))
    dir -= cam.forward();

  if (is_held_down(kb[KeyboardKey::kW]))
    dir += cam.forward();

  if (is_held_down(kb[KeyboardKey::kA]))
    dir -= cam.right();

  if (is_held_down(kb[KeyboardKey::kD]))
    dir += cam.right();

  if (is_held_down(kb[KeyboardKey::kF]))
    dir -= cam.up();

  if (is_held_down(kb[KeyboardKey::kR]))
    dir += cam.up();

  // NOTE: This is how you make moving diagonally not be faster than
  // in a straight line.
  cam.move(dt * camMoveSpeed * (length(dir) > 1e-9 ? normalize(dir) : dir));
}

void App::rotateCam(Camera& cam, const Mouse& ms, float /*dt*/)
{
  // Rotate camera based on mouse movement
  cam.rotate(camRotateSpeed * ms.capturedPosDelta.y, camRotateSpeed * ms.capturedPosDelta.x);

  // Increase or decrease field of view based on mouse wheel
  cam.fov -= zoomSensitivity * ms.scrollDelta.y;
  if (cam.fov < 1.0f)
    cam.fov = 1.0f;
  if (cam.fov > 120.0f)
    cam.fov = 120.0f;
}
