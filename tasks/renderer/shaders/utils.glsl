bool Cull(vec3 minP, vec3 maxP, mat4 projMat)
{
  vec3 minV;
  vec3 maxV;
  {
    vec4 proj = projMat * vec4(minP, 1);
    proj /= abs(proj.w);
    minV = proj.xyz;
    maxV = proj.xyz;
  }
  for (uint mask = 1; mask < 8u; ++mask)
  {
    vec3 point;
    for (uint i = 0; i < 3; ++i)
    {
      point[i] = ((mask & (1u << i)) != 0) ? maxP[i] : minP[i];
    }
    vec4 corner = projMat * vec4(point, 1);
    corner /= abs(corner.w);
    minV = min(minV, corner.xyz);
    maxV = max(maxV, corner.xyz);
  }
  return any(lessThan(maxV, vec3(-1, -1, 0))) || any(greaterThan(minV, vec3(1, 1, 1)));
}
