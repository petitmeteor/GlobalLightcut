#pragma once

#include <vector>
#include <random>
#include <glm/glm.hpp>
#include <embree2/rtcore.h>
#include <embree2/rtcore_ray.h>
#include "host_mesh.h"

struct point_light_t
{
    glm::vec3 position;
    glm::vec3 intensity;
    glm::vec3 direction;
};

struct area_light_t
{
    glm::vec3 aabb_min;
    glm::vec3 aabb_max;
    glm::vec3 intensity;
    glm::vec3 direction;
};

class raytracer
{
    RTCScene scene_;
    std::default_random_engine random_engine_;
    std::uniform_real_distribution<float> uniform_real_distribution_jitter_;
    std::uniform_real_distribution<float> uniform_real_distribution_01_;

    std::map<unsigned int, host_mesh_t> geom_id_to_mesh_;
public:
    static constexpr int kRecursionDepthHardLimit = 20; //to prevent stack overflow in recursive VPL algorithm

    raytracer();
    ~raytracer();
    raytracer( const raytracer &rhs ) = delete;
    raytracer( raytracer &&rhs ) = delete;
    raytracer &operator=( const raytracer &rhs ) = delete;
    raytracer &operator=( raytracer &&rhs ) = delete;

    void add_mesh( host_mesh_t mesh );
    void commit_scene();
    std::vector<point_light_t> compute_vpl( point_light_t &light, float root_intensity, unsigned int recursion_depth_left = kRecursionDepthHardLimit );
    std::vector<point_light_t> compute_vpl( area_light_t light, unsigned int light_sample_count );
};
