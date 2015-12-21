#include "device_mesh.h"
#include "host_mesh.h"
#include <GL/glew.h>

namespace
{

void setup_vertex_array( device_mesh_t &dm )
{
    dm.vertex_array.bind();
    dm.vertex_array.set_attribute( device_mesh_t::vertex_attributes::position, dm.vbo_vertices, 3, GL_FLOAT, false, 0, 0 );
    dm.vertex_array.set_attribute( device_mesh_t::vertex_attributes::normal, dm.vbo_normals, 3, GL_FLOAT, false, 0, 0 );
    dm.vertex_array.set_attribute( device_mesh_t::vertex_attributes::texcoord, dm.vbo_texcoords, 2, GL_FLOAT, false, 0, 0 );
    dm.vertex_array.unbind();
}

}

device_mesh_t::device_mesh_t( gls::buffer &&vbo_indices,
                              gls::buffer &&vbo_vertices,
                              gls::buffer &&vbo_normals,
                              gls::buffer &&vbo_texcoords,
                              glm::vec3 &&ambient_color,
                              glm::vec3 &&diffuse_color,
                              std::string &&texture_name
                            ) :
    vbo_indices( std::move( vbo_indices ) ),
    vbo_vertices( std::move( vbo_vertices ) ),
    vbo_normals( std::move( vbo_normals ) ),
    vbo_texcoords( std::move( vbo_texcoords ) ),
    ambient_color( std::move( ambient_color ) ),
    diffuse_color( std::move( diffuse_color ) ),
    texture_name( std::move( texture_name ) )
{
    setup_vertex_array( *this );
}

device_mesh_t::device_mesh_t( const host_mesh_t &mesh )
{
    vbo_indices = gls::buffer( GL_ELEMENT_ARRAY_BUFFER, GL_STATIC_DRAW );
    vbo_vertices = gls::buffer( GL_ARRAY_BUFFER, GL_STATIC_DRAW );
    vbo_normals = gls::buffer( GL_ARRAY_BUFFER, GL_STATIC_DRAW );
    vbo_texcoords = gls::buffer( GL_ARRAY_BUFFER, GL_STATIC_DRAW );

    //vertices
    vbo_vertices.bind();
    vbo_vertices.set_data( mesh.vertices );
    vbo_normals.bind();
    vbo_normals.set_data( mesh.normals );
    vbo_texcoords.bind();
    vbo_texcoords.set_data( mesh.texcoords );

    //indices
    vbo_indices.bind();
    vbo_indices.set_data( mesh.indices );

    texture_name = mesh.texture_name;
    ambient_color = mesh.ambient_color;
    diffuse_color = mesh.diffuse_color;

    setup_vertex_array( *this );
}

void device_mesh_t::draw()
{
    vertex_array.bind();
    vbo_indices.bind();
    glDrawElements( GL_TRIANGLES, GLsizei( vbo_indices.num_elements() ), GL_UNSIGNED_SHORT, 0 );
}