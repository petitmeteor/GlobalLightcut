
#ifndef	__TYPE_DEFINITION_H__
#define __TYPE_DEFINITION_H__

#include "gl_snippets.h"

constexpr float PI = 3.14159f;
constexpr float kFarPlane = 2000.f;
constexpr float kNearPlane = 0.1f;
constexpr int kShadowSize = 256;
constexpr int kVplCount = 128;
//constexpr int kVplCount = 32;
//constexpr int kVplCount = 1024;


enum gls_program_t
{
    kGlsProgramSceneDraw,
    kGlsProgramQuadDraw,
    kGlsProgramShadowMapping,
	kGlsProgramGbufferDraw,
    kGlsProgramMax
};

enum gls_buffer_t
{
    kGlsBufferQuadVertexBuffer,
    kGlsBufferQuadIndexBuffer,
    kGlsBufferMax
};

enum gls_vertex_array_t
{
    kGlsVertexArrayQuad,
    kGlsVertexArrayMax
};

enum gls_framebuffer_t
{
    kGlsFramebufferScreen,
    kGlsFramebufferSceneDraw,
    kGlsFramebufferAccumulate,
	kGlsFramebufferMax
};

enum gls_cubemap_framebuffer_t
{
    kGlsCubemapFramebufferShadow,
    kGlsCubemapFramebufferMax
};

enum gls_gbuffer_frambuffer_t
{
	kGlsGbufferFramebuffer,
	KGlsGbufferFramebufferMax
} ;

enum gls_texture_t
{
    kGlsTextureScene,
    kGlsTextureAccumulate,
    kGlsTextureMax
};

enum gls_gbuffer_texture_t
{
	kGlsGbufferTexturePosition,
	kGlsGbufferTextureNormal,
	kGlsGbufferTextureDiffuse,
//	kGlsGbufferTextureTexCoord,
	kGlsGbufferTextureMax
} ;

using my_gbuffer_texture = gls::gbuffer_texture< kGlsGbufferTextureMax > ;

#endif	// __TYPE_DEFINITION_H__
