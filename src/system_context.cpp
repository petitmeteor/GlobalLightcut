#include "system_context.h"

#include <sstream>
#include <iostream>
#include <vector>
#include "host_mesh.h"
#include "device_mesh.h"
#include "main.h"

std::unique_ptr<system_context> system_context::global_context_ = nullptr;

void system_context::load_mesh( const char *path )
{
    std::vector<tinyobj::shape_t> shapes;
    //read shapes using tinyobj first
    {
        std::string header, data, err;
        std::istringstream liness( path );

        getline( liness, header, '=' );
        getline( liness, data, '=' );
        if( header == "mesh" )
        {
            std::size_t found = data.find_last_of( "/\\" );
            std::string path = data.substr( 0, found + 1 );
            std::cout << "Loading: " << data << std::endl;
            err = tinyobj::LoadObj( shapes, data.c_str(), path.c_str() );
            if( !err.empty() )
                throw std::runtime_error( err.c_str() );
        }
    }

    //from tinyobj shapes, create mesh entries
    for( const auto &shape : shapes )
    {
        host_mesh_t mesh = host_mesh_t( shape );
        //std::cout << mesh.get_aabb().first.x << " " << mesh.get_aabb().first.y << " " << mesh.get_aabb().first.z << ", " << mesh.get_aabb().second.x << " " << mesh.get_aabb().second.y <<  " " << mesh.get_aabb().second.z << std::endl;
        scene_meshes.push_back( device_mesh_t( mesh ) );
        vpl_raytracer->add_mesh( mesh );
        if( shape.material.name == "light" )
        {
            // process light; use ambient color as intensity
            auto aabb = mesh.get_aabb();
            light.aabb_min = aabb.first;
            light.aabb_max = aabb.second;
            light.direction = glm::normalize( glm::vec3( 0, -1, 0 ) );
            light.intensity = glm::make_vec3( shape.material.ambient );
        }
    }
    vpl_raytracer->commit_scene();
    vpls = std::move( vpl_raytracer->compute_vpl( light, kVplCount ) );

	area_light_vpls_index.clear() ;
	bounced_vpls_index.clear() ;
	bounced_vpls.clear() ;

	glm::vec2 area = ( light.aabb_max - light.aabb_min ).xz();
	glm::vec3 vIntensity = light.intensity * ( area.x * area.y ) / float( kVplCount ) ;

	int iVplSize = (int)vpls.size() ;
	for ( int i = 0; i < iVplSize; ++i )
	{
		if ( vpls[ i ].intensity == vIntensity )
		{
			area_light_vpls_index.push_back( i ) ;
		}
		else
		{
			bounced_vpls_index.push_back( i ) ;
			bounced_vpls.push_back( vpls[ i ] ) ;
		}
	}
}


void	system_context::build_light_tree()
{
	if ( use_only_bounced_vpls )
	{
		lightcuts.build_light_tree( bounced_vpls ) ;
	}
	else
	{
		lightcuts.build_light_tree( vpls ) ;
	}
}