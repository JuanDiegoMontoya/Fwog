#version 460 core
#extension GL_GOOGLE_include_directive : enable

#include "Common.h"

layout(location = 0) in flat uint v_drawID;

layout (early_fragment_tests) in;
void main()
{
  drawCommands[v_drawID].instanceCount = 1;
}