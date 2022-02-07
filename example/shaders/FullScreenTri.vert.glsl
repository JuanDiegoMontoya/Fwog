#version 460 core

layout(location = 0) out vec2 v_uv;

void main()
{
  vec2 pos = vec2(gl_VertexID == 0, gl_VertexID == 2);
  v_uv = pos.xy * 2.0;
  gl_Position = vec4(pos * 4.0 - 1.0, 0.0, 1.0);
}