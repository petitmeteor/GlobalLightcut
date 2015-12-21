#ifndef _HOST_MESH_H_
#define _HOST_MESH_H_

#include <vector>
#include <algorithm>
#include <glm/glm.hpp>
#include <tiny_obj_loader/tiny_obj_loader.h>

class host_mesh_t
{
public:
    std::vector<glm::vec3> vertices;
    std::vector<glm::vec3> normals;
    std::vector<glm::vec2> texcoords;
    std::vector<unsigned short> indices;
    std::string texture_name;
    glm::vec3 ambient_color;
    glm::vec3 diffuse_color;

public:
    host_mesh_t();
    host_mesh_t( const host_mesh_t &mesh ) = default;
    host_mesh_t( const std::vector<glm::vec3> &vertices,
                 const std::vector<glm::vec3> &normals,
                 const std::vector<glm::vec2> &texcoords,
                 const std::vector<unsigned short> &indices,
                 const std::string &texture_name,
                 const glm::vec3 &ambient_color,
                 const glm::vec3 &diffuse_color );
    explicit host_mesh_t( const tinyobj::shape_t &shape );
    host_mesh_t &operator=( const host_mesh_t & ) = default;
    ~host_mesh_t() = default;
    host_mesh_t( host_mesh_t && ) = default;
    host_mesh_t &operator=( host_mesh_t && ) = default;

    const std::pair<glm::vec3, glm::vec3> &get_aabb() const
    {
        update_aabb_();
        return aabb_;
    }

protected:
    mutable std::pair<glm::vec3, glm::vec3> aabb_;
    mutable bool aabb_cached_;

    void update_aabb_() const;
};

#endif