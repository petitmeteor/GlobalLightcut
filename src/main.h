#ifndef _MAIN_H_
#define _MAIN_H_

#include <vector>
#include <cstring>

#include <GL/glew.h>
#include <glm/glm.hpp>
#include <tiny_obj_loader/tiny_obj_loader.h>

#include "gl_snippets.h"

//GL shader/program definitions
GLS_PROGRAM_DEFINE(
    kProgramSceneDraw,
    "res/shaders/screen_draw.vertex.glsl",
    "res/shaders/screen_draw.fragment.glsl",
{ "Position", "Normal" },
{ "outColor" },
{ "u_modelMat", "u_viewMat" , "u_perspMat", "u_vplPosition", "u_vplIntensity", "u_vplDirection", "u_numLights", "u_ambientColor", "u_diffuseColor", "u_shadowTex" }
);

GLS_PROGRAM_DEFINE(
    kProgramQuadDraw,
    "res/shaders/quad.vertex.glsl",
    "res/shaders/quad.fragment.glsl",
{ "Position",  "Normal", "Texcoord" },
{ "outColor" },
{ "u_Tex" }
);

GLS_PROGRAM_DEFINE(
    kProgramShadowMapping,
    "res/shaders/shadow.vertex.glsl",
    "res/shaders/shadow.fragment.glsl",
{ "Position" },
{ "outColor" },
{ "u_model", "u_cameraToShadowView" , "u_cameraToShadowProjector" }
);

GLS_PROGRAM_DEFINE(
	kProgramGbufferDraw,
	"res/shaders/gbuffer.vertex.glsl",
	"res/shaders/gbuffer.fragment.glsl",
	{ "Position", "Normal" },
	{ "WorldPosOut", "WorldNormalOut", "DiffuseOut" },
	{ "u_modelMat", "u_viewMat" , "u_perspMat", "u_diffuseColor" }
) ;

#endif
