#ifndef _DEVICE_MESH_H_
#define _DEVICE_MESH_H_

#include <glm/glm.hpp>
#include <string>
#include "gl_snippets.h"

class host_mesh_t;

class device_mesh_t
{
public:
    struct vertex_attributes
    {
        enum
        {
            position = 0,
            normal = 1,
            texcoord = 2
        };
    };

    gls::vertex_array vertex_array;
    gls::buffer vbo_indices;
    gls::buffer vbo_vertices;
    gls::buffer vbo_normals;
    gls::buffer vbo_texcoords;
    glm::vec3 ambient_color;
    glm::vec3 diffuse_color;
    std::string texture_name;

public:
    device_mesh_t() : vertex_array( nullptr ) {}
    device_mesh_t( gls::buffer &&vbo_indices,
                   gls::buffer &&vbo_vertices,
                   gls::buffer &&vbo_normals,
                   gls::buffer &&vbo_texcoords,
                   glm::vec3 &&ambient_color,
                   glm::vec3 &&diffuse_color,
                   std::string &&texture_name
                 );
    explicit device_mesh_t( const host_mesh_t &mesh );
    device_mesh_t( const device_mesh_t & ) = delete;
    device_mesh_t &operator=( const device_mesh_t & ) = delete;
    ~device_mesh_t() = default;
    device_mesh_t( device_mesh_t && ) = default;
    device_mesh_t &operator=( device_mesh_t && ) = default;

    void draw();
};

#endif
