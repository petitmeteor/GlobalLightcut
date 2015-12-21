#include "main.h"

#include <iostream>
#include <string>
#include <list>
#include <random>
#include <ctime>
#include <algorithm>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/verbose_operator.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "types.h"
#include "utility.h"
#include "system_context.h"
#include "device_mesh.h"
#include "gl_snippets.h"
#include "raytracer.h"

int mouse_buttons = 0;
int mouse_old_x = 0;
int mouse_old_y = 0;

int render_mode = 0;	// 0: reference, 1: global_light_cut, 2: area_light_samples
int lightcut_type = 0 ;	// 0: max_local_cut, 1: used_node_count, 2: used_node_count_and_error
int indirect_only = 0 ;	// 0: normal, 1: indirect illumination only
int use_default_num_lights = 0 ;

system_context *context;

bool bStarted = true;//false ;

glm::mat4 get_global_mesh_world()
{
    return glm::mat4( 1.0 );
}

void update_title()
{
    //FPS calculation
    static unsigned long frame = 0;
    static double time_base = glfwGetTime();
    ++frame;
    double time_now = glfwGetTime();

    if( time_now - time_base > 1.0 ) //update title if a second passes
    {
		std::string title ;

		if ( 0 == render_mode )
		{
			title = utility::sprintfpp( "CS482 Instant Radiosity | FPS: %4.2f (%d samples, %d / %d vpls) - reference",
					frame / (time_now - time_base),
					kVplCount,
					context->vpls.size(),
					context->vpls.size() ) ;
		}
		else if ( 1 == render_mode )
		{
			title = utility::sprintfpp( "CS482 Instant Radiosity | FPS: %4.2f (%d samples, %d / %d vpls) - global_lightcut",
					frame / (time_now - time_base),
					kVplCount,
					context->lightcuts.m_vLightCut.size(),
					context->vpls.size() ) ;

			if ( 0 == lightcut_type )		title += " by max local lightcut" ;
			else if ( 1 == lightcut_type )	title += " by used node count" ;
			else if ( 2 == lightcut_type )	title += " by used node count & error" ;
		}
		else if ( 2 == render_mode )
		{
			title = utility::sprintfpp( "CS482 Instant Radiosity | FPS: %4.2f (%d samples, %d / %d vpls) - area_light_samples",
					frame / (time_now - time_base),
					kVplCount,
					context->area_light_vpls_index.size(),
					context->vpls.size() ) ;
		}

		if ( 1 == indirect_only )
		{
			title += " (indirect only)" ;
		}

		if ( 1 == use_default_num_lights )
		{
			title += " (default num lights clamping)" ;
		}

        glfwSetWindowTitle( context->window, title.c_str() );
        //reset per-second frame statistics for next update
        time_base = time_now;
        frame = 0;
    }
}

//from https://github.com/cforfang/opengl-shadowmapping/blob/master/src/vsmcube/main.cpp
std::pair<glm::mat4, glm::mat4> get_shadow_matrices( glm::vec3 light_pos, int dir )
{
    glm::mat4 v, p;
    p = glm::perspective( 90.0f, 1.0f, 0.1f, 1000.0f );
    switch( dir )
    {
    case 0:
        // +X
        v = glm::lookAt( light_pos, light_pos + glm::vec3( +1, +0, 0 ), glm::vec3( 0, -1, 0 ) );
        p *= v;
        break;
    case 1:
        // -X
        v = glm::lookAt( light_pos, light_pos + glm::vec3( -1, +0, 0 ), glm::vec3( 0, -1, 0 ) );
        p *= v;
        break;
    case 2:
        // +Y
        v = glm::lookAt( light_pos, light_pos + glm::vec3( 0, +1, 0 ), glm::vec3( 0, 0, -1 ) );
        p *= v;
        break;
    case 3:
        // -Y
        v = glm::lookAt( light_pos, light_pos + glm::vec3( 0, -1, 0 ), glm::vec3( 0, 0, -1 ) );
        p *= v;
        break;
    case 4:
        // +Z
        v = glm::lookAt( light_pos, light_pos + glm::vec3( 0, 0, +1 ), glm::vec3( 0, -1, 0 ) );
        p *= v;
        break;
    case 5:
        // -Z
        v = glm::lookAt( light_pos, light_pos + glm::vec3( 0, 0, -1 ), glm::vec3( 0, -1, 0 ) );
        p *= v;
        break;
    default:
        // Do nothing
        break;
    }
    return std::make_pair( v, p );
}

void perlight_generate_shadow_map( int light_index )
{
    glDisable( GL_BLEND );
    glEnable( GL_DEPTH_TEST );
    context->gls_programs[kGlsProgramShadowMapping].bind();
    context->gls_cubemap_framebuffers[kGlsCubemapFramebufferShadow].set_viewport();

    for( int i = 0; i < 6; ++i )
    {
        context->gls_cubemap_framebuffers[kGlsCubemapFramebufferShadow].bind( i );
        glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );
        auto vp = get_shadow_matrices( context->vpls[light_index].position, i );
        context->gls_programs[kGlsProgramShadowMapping].set_uniforms(
            //"u_model", "u_cameraToShadowView" , "u_cameraToShadowProjector"
            gls::no_change,
            vp.first,
            vp.second );
        for( int i = 0; i < context->scene_meshes.size(); i++ )
        {
            context->scene_meshes[i].draw();
        }
    }
};

void perlight_accumulate( int light_index )
{
    glEnable( GL_BLEND ) ;
    glDisable( GL_DEPTH_TEST ) ;
    glBlendFunc( GL_ONE, GL_ONE ) ;	//perform additive blending

    context->gls_programs[ kGlsProgramQuadDraw ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferAccumulate ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferAccumulate ].set_viewport() ;
//	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    glActiveTexture( GL_TEXTURE0 ) ;
    context->gls_framebuffers[ kGlsFramebufferSceneDraw ].get_color_map().bind() ;
    context->quad_mesh.draw() ;
};

void perlight_draw( int light_index, bool bUseTotalIntensity, const glm::vec3& intensity )
{
	glm::vec3 totalIntensity = context->vpls[light_index].intensity ;
	if ( bUseTotalIntensity )
	{
		totalIntensity = intensity ;
	}

    glDisable( GL_BLEND );
    glEnable( GL_DEPTH_TEST );
    context->gls_programs[kGlsProgramSceneDraw].bind();
    context->gls_programs[kGlsProgramSceneDraw].set_uniforms(
        //"u_modelMat", "u_viewMat" , "u_perspMat", "u_vplPosition", "u_vplIntensity", "u_vplDirection", "u_numLights", "u_ambientColor", "u_diffuseColor", "u_shadowTex"
        gls::no_change,
        gls::no_change,
        gls::no_change,
        context->vpls[light_index].position,
        //context->vpls[light_index].intensity,
		totalIntensity,
        context->vpls[light_index].direction,
        gls::no_change,
        gls::no_change,
        gls::no_change,
        gls::no_change );
    context->gls_framebuffers[kGlsFramebufferSceneDraw].bind();
    context->gls_framebuffers[kGlsFramebufferSceneDraw].set_viewport();
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glActiveTexture( GL_TEXTURE0 );
    context->gls_cubemap_framebuffers[kGlsCubemapFramebufferShadow].get_color_map().bind();

    for( int i = 0; i < context->scene_meshes.size(); i++ )
    {
        context->gls_programs[kGlsProgramSceneDraw].set_uniforms(
            //"u_modelMat", "u_viewMat" , "u_perspMat", "u_vplPosition", "u_vplIntensity", "u_vplDirection", "u_numLights", "u_ambientColor", "u_diffuseColor", "u_shadowTex"
            gls::no_change,
            gls::no_change,
            gls::no_change,
            gls::no_change,
            gls::no_change,
            gls::no_change,
            gls::no_change,
            context->scene_meshes[i].ambient_color,
            context->scene_meshes[i].diffuse_color,
            gls::no_change );
        context->scene_meshes[i].draw();
    }
};

void gbuffer_draw()
{
	glDisable( GL_BLEND ) ;
    glEnable( GL_DEPTH_TEST ) ;

	context->gls_programs[ kGlsProgramGbufferDraw ].bind() ;
/*/
	context->gls_programs[ kGlsProgramGbufferDraw ].set_uniforms(
		// "u_modelMat", "u_viewMat" , "u_perspMat", "u_diffuseColor"
		gls::no_change,
		gls::no_change,
		gls::no_change,
		gls::no_change ) ;
//*/
	
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].bind() ;
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].set_viewport() ;
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;

    for ( int i = 0; i < context->scene_meshes.size(); i++ )
    {
		context->gls_programs[ kGlsProgramGbufferDraw ].set_uniforms(
			// "u_modelMat", "u_viewMat" , "u_perspMat", "u_diffuseColor"
			gls::no_change,
			gls::no_change,
			gls::no_change,
            context->scene_meshes[ i ].diffuse_color ) ;
		
		context->scene_meshes[ i ].draw() ;
    }
}

void visualize_gbuffer( gls_gbuffer_texture_t eType )
{
    glDisable( GL_BLEND ) ;
    glDisable( GL_DEPTH_TEST ) ;

    context->gls_programs[ kGlsProgramQuadDraw ].bind() ;
    
	context->gls_framebuffers[ kGlsFramebufferScreen ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferScreen ].set_viewport() ;
//	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    
	glActiveTexture( GL_TEXTURE0 ) ;
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().bind( (int)eType ) ;

    context->quad_mesh.draw() ;
}

void visualize_gbuffer_depth()
{
	glDisable( GL_BLEND ) ;
    glDisable( GL_DEPTH_TEST ) ;

    context->gls_programs[ kGlsProgramQuadDraw ].bind() ;
    
	context->gls_framebuffers[ kGlsFramebufferScreen ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferScreen ].set_viewport() ;
//	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    
	glActiveTexture( GL_TEXTURE0 ) ;
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_depth_map().bind() ;
	
    context->quad_mesh.draw() ;
}

void visualize_accumulation()
{
    glDisable( GL_BLEND ) ;
    glDisable( GL_DEPTH_TEST ) ;

    context->gls_programs[ kGlsProgramQuadDraw ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferScreen ].bind() ;
    context->gls_framebuffers[ kGlsFramebufferScreen ].set_viewport() ;
//	glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT ) ;
    glActiveTexture( GL_TEXTURE0 ) ;
    context->gls_framebuffers[ kGlsFramebufferAccumulate ].get_color_map().bind() ;
    context->quad_mesh.draw() ;
}

void render_reference()
{
	if ( 0 == indirect_only )
	{
		// reference
		for( int light_index = 0; light_index < context->vpls.size(); ++light_index )
		{
			perlight_generate_shadow_map( light_index );
			perlight_draw( light_index, false, glm::vec3( 0 ) );
			perlight_accumulate( light_index );
		}
	}
	else
	{
		// reference indirect vpls only
		int light_index ;
		
		int iBouncedVplSize = (int)context->bounced_vpls_index.size() ;
		for( int i = 0; i < iBouncedVplSize; ++i )
		{
			light_index = context->bounced_vpls_index[ i ] ;

			perlight_generate_shadow_map( light_index );
			perlight_draw( light_index, false, glm::vec3( 0 ) );
			perlight_accumulate( light_index );
		}
	}

    visualize_accumulation() ;
}

void render_area_light_samples()
{
	if ( 0 == indirect_only )
	{
		// only area light sample vpls
		int iAreaLightSamples = (int)context->area_light_vpls_index.size() ;
		if ( iAreaLightSamples > 0 )
		{
			int light_index ;

			for( int i = 0; i < iAreaLightSamples; ++i )
			{
				light_index = context->area_light_vpls_index[ i ] ;

				perlight_generate_shadow_map( light_index );
				perlight_draw( light_index, false, glm::vec3( 0 ) );
				perlight_accumulate( light_index );
			}
		}
	}
	else
	{
		// render nothing
		// ...
	}

	visualize_accumulation() ;
}

void render_gbuffer_n_update_global_lightcut()
{
	double timeGBufferStart = glfwGetTime() ;

	gbuffer_draw() ;
	//visualize_gbuffer( kGlsGbufferTexturePosition ) ;
	//visualize_gbuffer( kGlsGbufferTextureNormal ) ;
	//visualize_gbuffer( kGlsGbufferTextureDiffuse ) ;
	//visualize_gbuffer_depth() ;

	double timeGBufferEnd = glfwGetTime() ;
	std::cout << "G-Buffer rendering : " << timeGBufferEnd - timeGBufferStart << std::endl ;

	float* pos = new float [ 3 * context->viewport.x * context->viewport.y ] ;
	float* normal = new float [ 3 * context->viewport.x * context->viewport.y ] ;

	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().bind( (int)kGlsGbufferTexturePosition ) ;
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, pos ) ;
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().unbind() ;

	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().bind( (int)kGlsGbufferTextureNormal ) ;
	glGetTexImage( GL_TEXTURE_2D, 0, GL_RGB, GL_FLOAT, normal ) ;
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().unbind() ;


	int x = context->viewport.x / 2 ;
	int y = context->viewport.y / 2 ;
	int index = y * context->viewport.x * 3 + x * 3 ;

//	std::cerr << "center position - " << "x: " << pos[ index ] << ", y: " << pos[ index + 1 ] << ", z: " << pos[ index + 2 ] << std::endl ;
//	std::cerr << "center normal - " << "x: " << normal[ index ] << ", y: " << normal[ index + 1 ] << ", z: " << normal[ index + 2 ] << std::endl ;

	int iVplSize = (int)context->vpls.size() ;
	if ( context->use_only_bounced_vpls )
	{
		iVplSize = (int)context->bounced_vpls.size() ;
	}

	//context->lightcuts.calculate_global_lightcut( pos, normal, context->viewport.x, context->viewport.y, iVplSize ) ;
	if ( 0 == lightcut_type )
	{
		context->lightcuts.calculate_global_lightcut_by_max_local_lightcut( pos, normal, context->viewport.x, context->viewport.y, iVplSize ) ;
	}
	else if ( 1 == lightcut_type )
	{
		context->lightcuts.calculate_global_lightcut_by_used_node_count( pos, normal, context->viewport.x, context->viewport.y, iVplSize ) ;
	}
	else if ( 2 == lightcut_type )
	{
		context->lightcuts.calculate_global_lightcut_by_used_node_count_with_error( pos, normal, context->viewport.x, context->viewport.y, iVplSize ) ;
	}

	delete [] pos ;		pos = NULL ;
	delete [] normal ;	normal = NULL ;
}

void render_global_lightcut()
{
	if ( context->use_only_bounced_vpls )
	{
		if ( 0 == indirect_only )
		{
			// render area light sample vpls
			int iAreaLightSampleSize = (int)context->area_light_vpls_index.size() ;
			if ( iAreaLightSampleSize > 0 )
			{
				int light_index ;

				for ( int i = 0; i < iAreaLightSampleSize; ++i )
				{
					light_index = context->area_light_vpls_index[ i ] ;

					perlight_generate_shadow_map( light_index ) ;
					perlight_draw( light_index, false, glm::vec3( 0 ) ) ;
					perlight_accumulate( light_index ) ;
				}
			}
		}

		// render global lightcut for bounced vpls
		int iCutSize = (int)context->lightcuts.m_vLightCut.size() ;
		if ( iCutSize > 0 )
		{
			int light_index ;

			for ( int i = 0; i < iCutSize; ++i )
			{
				const light_node_t& light = context->lightcuts.m_vLightCut[ i ] ;
				light_index = context->bounced_vpls_index[ light.m_iVplIndex ] ;

				perlight_generate_shadow_map( light_index ) ;
				perlight_draw( light_index, true, light.m_vIntensity ) ;
				perlight_accumulate( light_index ) ;
			}
		}
	}
	else
	{
		if ( 0 == indirect_only )
		{
			// render global lightcut for all vpls
			int iCutSize = (int)context->lightcuts.m_vLightCut.size() ;
			if ( iCutSize > 0 )
			{
				for ( int i = 0; i < iCutSize; ++i )
				{
					const light_node_t& light = context->lightcuts.m_vLightCut[ i ] ;

					perlight_generate_shadow_map( light.m_iVplIndex ) ;
					perlight_draw( light.m_iVplIndex, true, light.m_vIntensity ) ;
					perlight_accumulate( light.m_iVplIndex ) ;
				}
			}
		}
		else
		{
			// render indirect illumination of global lightcut for all vpls
			int iCutSize = (int)context->lightcuts.m_vLightCut.size() ;
			if ( iCutSize > 0 )
			{
				for ( int i = 0; i < iCutSize; ++i )
				{
					const light_node_t& light = context->lightcuts.m_vLightCut[ i ] ;

					for ( int j = 0; j < context->bounced_vpls_index.size(); ++j )
					{
						if ( context->bounced_vpls_index[ j ] == light.m_iVplIndex )
						{
							perlight_generate_shadow_map( light.m_iVplIndex ) ;
							perlight_draw( light.m_iVplIndex, true, light.m_vIntensity ) ;
							perlight_accumulate( light.m_iVplIndex ) ;

							break ;
						}
					}
				}
			}
		}
	}

	visualize_accumulation() ;
}

void render()
{
	double timeStart = glfwGetTime() ;

    //clear accumulation framebuffer first
    context->gls_framebuffers[kGlsFramebufferAccumulate].bind();
    glClear( GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT );

    glm::mat4 model = get_global_mesh_world(); //assume that model matrix is constant across models!
    glm::mat4 view = context->camera.get_view();
    glm::mat4 perspective = context->camera.get_perspective();

	int iNumLights = (int)context->vpls.size() ;
	if ( 0 == use_default_num_lights )
	{
		if ( 1 == render_mode )
		{
			iNumLights = (int)context->lightcuts.m_vLightCut.size() ;

			if ( context->use_only_bounced_vpls )
			{
				iNumLights += (int)context->area_light_vpls_index.size() ;
			}
		}
		else if ( 2 == render_mode )
		{
			iNumLights = (int)context->area_light_vpls_index.size() ;
		}
	}
	
    //set global parameters
    context->gls_programs[kGlsProgramSceneDraw].bind();
    context->gls_programs[kGlsProgramSceneDraw].set_uniforms(
        //"u_modelMat", "u_viewMat" , "u_perspMat", "u_vplPosition", "u_vplIntensity", "u_vplDirection", "u_numLights", "u_ambientColor", "u_diffuseColor", "u_shadowTex"
        model,
        view,
        perspective,
        gls::no_change,
        gls::no_change,
        gls::no_change,
        iNumLights,
        gls::no_change,
        gls::no_change,
        0 );
    context->gls_programs[kGlsProgramQuadDraw].bind();
    context->gls_programs[kGlsProgramQuadDraw].set_uniforms(
        //"u_Tex"
        0 );
    context->gls_programs[kGlsProgramShadowMapping].bind();
    context->gls_programs[kGlsProgramShadowMapping].set_uniforms(
        //"u_model", "u_cameraToShadowView" , "u_cameraToShadowProjector"
        model,
        gls::no_change,
        gls::no_change );
	context->gls_programs[ kGlsProgramGbufferDraw ].bind() ;
	context->gls_programs[ kGlsProgramGbufferDraw ].set_uniforms(
		// "u_modelMat", "u_viewMat" , "u_perspMat", "u_diffuseColor"
		model,
		view,
		perspective,
		gls::no_change ) ;


	if ( 0 == render_mode )
	{
		// reference
		render_reference() ;
	}
	else if ( 1 == render_mode )
	{
		// global lightcut
		if ( context->need_to_update )
		{
			context->need_to_update = false ;
		
			render_gbuffer_n_update_global_lightcut() ;
		}

		render_global_lightcut() ;
	}
	else if ( 2 == render_mode )
	{
		// render area light samples
		render_area_light_samples() ;
	}

	double timeEnd = glfwGetTime() ;
	
	static int iLogCount = 0 ;
	if ( 0 == iLogCount % 25 )
	{
		std::cout << "render time : " << timeEnd - timeStart << std::endl ;
	}
	++iLogCount ;
}

void generate_cube_depth_map()
{
	std::cout << "generate all cube shadow maps for vpls ..." << std::endl ;

	// for statistics
	double timeStart = glfwGetTime() ;
	double timeRenderStart, timeRenderEnd ;
	double timeSMStart, timeSMEnd ;
	double timeFaceStart, timeFaceEnd ;
	double avgRender = 0 ;
	double avgSM = 0 ;
	double avgFace = 0 ;

	// set shader program for shadow mapping
	glm::mat4 model = get_global_mesh_world(); //assume that model matrix is constant across models!

	context->gls_programs[kGlsProgramShadowMapping].bind();
	context->gls_programs[kGlsProgramShadowMapping].set_uniforms(
        //"u_model", "u_cameraToShadowView" , "u_cameraToShadowProjector"
        model,
        gls::no_change,
        gls::no_change ) ;


	// generate all shadow maps for vpls
	int iVplIndex ;
	int iVplSize = (int)context->vpls.size() ;
	if ( context->use_only_bounced_vpls )
	{
		iVplSize = (int)context->bounced_vpls.size() ;
	}

	context->lightcuts.cube_depth_map.clear() ;
	context->lightcuts.cube_depth_map.reserve( iVplSize ) ;

	std::size_t size = 3 * kShadowSize * kShadowSize ;
//	float* depth = new float [ size ] ;

	for ( int i = 0; i < iVplSize; ++i )
	{
		timeSMStart = glfwGetTime() ;
		timeRenderStart = timeSMStart ;
		//std::cout << "  render cube shadow map (" << i << ")" << std::endl ;
		
		iVplIndex = i ;
		if ( context->use_only_bounced_vpls )
		{
			iVplIndex = context->bounced_vpls_index[ i ] ;
		}

		perlight_generate_shadow_map( iVplIndex ) ;

		timeRenderEnd = glfwGetTime() ;
		avgRender += timeRenderEnd - timeRenderStart ;
		//std::cout << "  success to render cube shadow map (" << i << ") - time : " << timeEnd - timeStart << std::endl ;

		context->gls_cubemap_framebuffers[ kGlsCubemapFramebufferShadow ].get_color_map().bind() ;

//		cube_depth_map_t cube_depth_map( kShadowSize ) ;	// TODO : allocate heap memory ?
		cube_depth_map_t* cube_depth_map = new cube_depth_map_t( kShadowSize ) ;

		//std::cout << "  copy depth map -" ;

		for ( int j = 0; j < 6; ++j )
		{
			//std::cout << " " << j ;

			float* depth = new float [ size ] ;

			timeFaceStart = glfwGetTime() ;
			
			glGetTexImage( GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, 0, GL_RGB, GL_FLOAT, depth ) ;
			//timeEnd = glfwGetTime() ;

			//std::cout << "(" << timeEnd - timeStart << ", " ;

			//timeStart = glfwGetTime() ;
			cube_depth_map->setCubeDepthMap( GL_TEXTURE_CUBE_MAP_POSITIVE_X + j, depth, size ) ;
			
			timeFaceEnd = glfwGetTime() ;
			avgFace += timeFaceEnd - timeFaceStart ;

			//std::cout << timeEnd - timeStart << ")" ;
		}

		//std::cout << std::endl ;

		context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ].get_color_map().unbind() ;

		context->lightcuts.cube_depth_map.push_back( cube_depth_map ) ;

		timeSMEnd = glfwGetTime() ;
		avgSM += timeSMEnd - timeSMStart ;
	}

//	delete [] depth ;
//	depth = NULL ;

	double timeEnd = glfwGetTime() ;
	std::cout << "total time : " << timeEnd - timeStart << std::endl ;
	std::cout << "per shadow map : " << avgSM / iVplSize << std::endl ;
	std::cout << " - render : " << avgRender / iVplSize << ", each face : " << avgFace / iVplSize / 6 << std::endl ;

	std::cout << "success to generate all cube shadow maps for vpls ..." << std::endl ;
}

void window_callback_mouse_button( GLFWwindow *window, int button, int action, int mods )
{
    if( action == GLFW_PRESS )
    {
        mouse_buttons |= 1 << button;
    }
    else if( action == GLFW_RELEASE )
    {
        mouse_buttons = 0;
    }
    {
        double x, y;
        glfwGetCursorPos( window, &x, &y );

        mouse_old_x = int( x );
        mouse_old_y = int( y );
    }
}

void window_callback_cursor_pos( GLFWwindow *window, double x, double y )
{
    float dx, dy;
    dx = -( float )( x - mouse_old_x );
    dy = ( float )( y - mouse_old_y );
    float sensitivity = 0.001f;

    if( mouse_buttons & 1 << GLFW_MOUSE_BUTTON_LEFT )
    {
        context->camera.rotate( glm::vec3( dy * sensitivity, 0, dx * sensitivity ) );

		context->need_to_update = true ;
    }

    mouse_old_x = int( x );
    mouse_old_y = int( y );
}

void window_callback_key( GLFWwindow *window, int key, int scancode, int action, int mods )
{
    float tx = 0;
    float ty = 0;
    float tz = 0;
    if( action == GLFW_RELEASE ) //no need to process key up events
        return;
    float speed = 10.f;
    switch( key )
    {
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose( window, true );
        break;
    case( 'W' ) :
        tz = speed;
        break;
    case( 'S' ) :
        tz = -speed;
        break;
    case( 'D' ) :
        tx = -speed;
        break;
    case( 'A' ) :
        tx = speed;
        break;
    case( 'Q' ) :
        ty = speed;
        break;
    case( 'Z' ) :
        ty = -speed;
        break;
	case( 'R' ) :
		{
			++render_mode ;
			if ( render_mode > 2 )
			{
				render_mode = 0 ;
			}
		}
		break ;
	case( 'L' ) :
		{
			++lightcut_type ;
			if ( lightcut_type > 2 )
			{
				lightcut_type = 0 ;
			}

			context->need_to_update = true ;
		}
		break ;
	case( 'I' ) :
		indirect_only = 1 - indirect_only ;
		break ;
	case( ' ' ):
		use_default_num_lights = 1 - use_default_num_lights ;
        break;
	case( 'G' ) :
		bStarted = true ;
		break ;
    }

    if( abs( tx ) > 0 ||  abs( tz ) > 0 || abs( ty ) > 0 )
    {
        context->camera.translate( glm::vec3( tx, ty, tz ) );

		context->need_to_update = true ;
    }
}

device_mesh_t init_quad_mesh()
{
    std::vector<glm::vec3> positions
    {
        glm::vec3( -1, 1, 0 ),
        glm::vec3( -1, -1, 0 ),
        glm::vec3( 1, -1, 0 ),
        glm::vec3( 1, 1, 0 )
    };

    std::vector<glm::vec3> normals
    {
        glm::vec3( 0, 0, 0 ),
        glm::vec3( 0, 0, 0 ),
        glm::vec3( 0, 0, 0 ),
        glm::vec3( 0, 0, 0 )
    };

    std::vector<glm::vec2> texcoords
    {
        glm::vec2( 0, 1 ),
        glm::vec2( 0, 0 ),
        glm::vec2( 1, 0 ),
        glm::vec2( 1, 1 )
    };

    std::vector<unsigned short> indices{ 0, 1, 2, 0, 2, 3 };

    host_mesh_t hm( positions, normals, texcoords, indices, "", glm::vec3( 0, 0, 0 ), glm::vec3( 0, 0, 0 ) );
    return device_mesh_t( hm );
}

void init()
{
    //GL parameter initialization
    glEnable( GL_TEXTURE_CUBE_MAP_SEAMLESS );
    glClearColor( 0.0f, 0.0f, 0.0f, 1.0f );

    //context gls object initialization
    context->gls_programs.resize( kGlsProgramMax ) ;
    context->gls_buffers.resize( kGlsBufferMax ) ;
    context->gls_vertex_arrays.resize( kGlsVertexArrayMax ) ;
    context->gls_framebuffers.resize( kGlsFramebufferMax ) ;
    context->gls_cubemap_framebuffers.resize( kGlsCubemapFramebufferMax ) ;
	context->gls_gbuffer_framebuffers.resize( KGlsGbufferFramebufferMax ) ;

    //shaders
    context->gls_programs[kGlsProgramSceneDraw] = gls::program( kProgramSceneDraw );
    context->gls_programs[kGlsProgramQuadDraw] = gls::program( kProgramQuadDraw );
    context->gls_programs[kGlsProgramShadowMapping] = gls::program( kProgramShadowMapping );
	context->gls_programs[kGlsProgramGbufferDraw] = gls::program( kProgramGbufferDraw ) ;

    //framebuffers
    context->gls_framebuffers[kGlsFramebufferScreen] = gls::framebuffer<gls::texture, gls::texture>( context->viewport.x, context->viewport.y, true ); //default screen framebuffer
    context->gls_framebuffers[kGlsFramebufferSceneDraw] = gls::framebuffer<gls::texture, gls::texture>( context->viewport.x, context->viewport.y );
    context->gls_framebuffers[kGlsFramebufferAccumulate] = gls::framebuffer<gls::texture, gls::texture>( context->viewport.x, context->viewport.y );

    //cubemap framebuffers
    context->gls_cubemap_framebuffers[kGlsCubemapFramebufferShadow] = gls::cubemap_framebuffer<gls::texture, gls::texture>( kShadowSize ); //default screen framebuffer

	// gbuffer framebuffers
	context->gls_gbuffer_framebuffers[ kGlsGbufferFramebuffer ] = gls::framebuffer< my_gbuffer_texture, gls::texture >( context->viewport.x, context->viewport.y ) ;

    //quad geometry; used for various texture-to-texture operations
    context->quad_mesh = std::move( init_quad_mesh() );
}

namespace
{
//opengl initialization: GLFW, GLEW and our application window
class opengl_initializer_t
{
public:
    opengl_initializer_t();
    opengl_initializer_t( const opengl_initializer_t & ) = delete;
    opengl_initializer_t &operator=( const opengl_initializer_t & ) = delete;
    ~opengl_initializer_t();
    opengl_initializer_t( opengl_initializer_t && ) = delete;
    opengl_initializer_t &operator=( opengl_initializer_t && ) = delete;
};

opengl_initializer_t::opengl_initializer_t()
{
    //initialize glfw
    if( !glfwInit() )
        throw std::runtime_error( "glfwInit() failed" );

    try
    {
        //create window
        glfwWindowHint( GLFW_RESIZABLE, GL_FALSE );
        if( !( context->window = glfwCreateWindow( context->viewport.x, context->viewport.y, "InstantRadiosity", NULL, NULL ) ) )
            throw std::runtime_error( "glfw window creation failed" );


        glfwMakeContextCurrent( context->window );

        //set callbacks
        glfwSetKeyCallback( context->window, window_callback_key );
        glfwSetCursorPosCallback( context->window, window_callback_cursor_pos );
        glfwSetMouseButtonCallback( context->window, window_callback_mouse_button );

        //initialize glew
        glewExperimental = GL_TRUE;
        if( glewInit() != GLEW_OK )
            throw std::runtime_error( "glewInit() failed" );

        //check version requirement
        //TODO: update correct requirement later
        if( !GLEW_VERSION_3_3 )
            throw std::runtime_error( "This program requires OpenGL 3.3 class graphics card." );
        else
        {
            std::cerr << "Status: Using GLEW " << glewGetString( GLEW_VERSION ) << std::endl;
            std::cerr << "OpenGL version " << glGetString( GL_VERSION ) << " supported" << std::endl;
        }
    }
    catch( ... )
    {
        glfwTerminate();
        throw;
    }
}
opengl_initializer_t::~opengl_initializer_t()
{
    glfwTerminate();
}
}
int main( int argc, char *argv[] )
{
    //Step 0: Initialize our system context
    {
        glm::uvec2 viewport( 1280, 720 );
        camera_t default_camera(
            glm::vec3( 300, 300, -500 ),
            glm::vec3( 0, 0, 1 ),
            glm::vec3( 0, 1, 0 ),
            glm::perspective(
                45.0f,
                float( viewport.x ) / float( viewport.y ),
                kNearPlane,
                kFarPlane
            )
        );
        context = system_context::initialize( default_camera, viewport );
    }

    //Step 1: Initialize GLFW & GLEW
    //RAII initialization of GLFW, GLEW and our application window
    std::unique_ptr<opengl_initializer_t> opengl_initializer;
    try
    {
        opengl_initializer.reset( new opengl_initializer_t );
    }
    catch( const std::exception &e )
    {
        std::cerr << "OpenGL initialization failed. Reason: " << e.what() << "\nAborting.\n";
        return EXIT_FAILURE;
    }

    //Step 2: Load mesh into memory
    if( argc > 1 )
    {
        try
        {
            context->load_mesh( argv[1] ) ;

			context->build_light_tree() ;
        }
        catch( const std::exception &e )
        {
            std::cerr << "Mesh load failed. Reason: " << e.what() << "\nAborting.\n";
            return EXIT_FAILURE;
        }
    }
    else
    {
        std::cerr << utility::sprintfpp( "Usage: %s mesh=[obj file]\n", argv[0] );
        return EXIT_SUCCESS;
    }

    //Step 3: Initialize objects
    try
    {
        init();

		// TODO : for test
		generate_cube_depth_map() ;
    }
    catch( const std::exception &e )
    {
        std::cerr << "Object initialization failed. Reason: " << e.what() << "\nAborting.\n";
        return EXIT_FAILURE;
    }

	//Step 4: Main loop
    while( !glfwWindowShouldClose( context->window ) )
    {
		if ( bStarted )
		{
			render();
			update_title();

			glfwSwapBuffers( context->window );
		}

        glfwPollEvents();
    }

    return EXIT_SUCCESS;
}