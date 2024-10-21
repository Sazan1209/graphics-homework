#version 450

layout(push_constant) uniform params {
    float time;
};
layout(location = 0) in vec2 texCoord;
layout(location = 0) out vec4 color;

void main()
{
  vec2 coord = texCoord;
  coord.x += 20.0;
  coord = sin(coord * 50.0 + time);
  coord *= coord;
  float level = dot(coord, vec2(1));

  if (abs(level) <= 0.5)
  {
    color = vec4(1, .2, .2, 1.0);
  }
  else
  {
    color = vec4(1);
  }
}
