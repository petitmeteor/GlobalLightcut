#include "raytracer.h"
#include <memory>
#include <glm/gtc/type_ptr.hpp>
#include "random.h"

namespace
{
std::unique_ptr<bool> raytracer_singleton;
void error_handler( const RTCError code, const char *str )
{
    printf( "Embree error: " );
    switch( code )
    {
    case RTC_UNKNOWN_ERROR:
        printf( "RTC_UNKNOWN_ERROR" );
        break;
    case RTC_INVALID_ARGUMENT:
        printf( "RTC_INVALID_ARGUMENT" );
        break;
    case RTC_INVALID_OPERATION:
        printf( "RTC_INVALID_OPERATION" );
        break;
    case RTC_OUT_OF_MEMORY:
        printf( "RTC_OUT_OF_MEMORY" );
        break;
    case RTC_UNSUPPORTED_CPU:
        printf( "RTC_UNSUPPORTED_CPU" );
        break;
    case RTC_CANCELLED:
        printf( "RTC_CANCELLED" );
        break;
    default:
        printf( "invalid error code" );
        break;
    }
    if( str )
    {
        printf( " (" );
        while( *str ) putchar( *str++ );
        printf( ")\n" );
    }
    exit( 1 );
}
}

constexpr float PI = 3.14159265358979f;
constexpr float kJitterEpsilon = 0.2f;
constexpr float kRayTraceEpsilon = 0.01f;

struct Vertex
{
    float x, y, z, r;
};
struct Triangle
{
    int v0, v1, v2;
};

raytracer::raytracer(): uniform_real_distribution_jitter_( -kJitterEpsilon, kJitterEpsilon ), uniform_real_distribution_01_( 0.f, 1.f )
{
    assert( raytracer_singleton.get() == nullptr ); // assure only one instance of raytracer can be there

    rtcInit( NULL );
#ifdef _DEBUG
    rtcSetErrorFunction( error_handler );
#endif
    scene_ = rtcNewScene( RTC_SCENE_STATIC, RTC_INTERSECT1 );
}

raytracer::~raytracer()
{
    rtcDeleteScene( scene_ );
    rtcExit();
}

void raytracer::commit_scene()
{
    rtcCommit( scene_ );
}

void raytracer::add_mesh( host_mesh_t mesh )
{
    unsigned int geom_id = rtcNewTriangleMesh( scene_, RTC_GEOMETRY_STATIC,
                           mesh.indices.size() / 3, mesh.vertices.size() );

    Vertex *vertices = ( Vertex * ) rtcMapBuffer( scene_, geom_id, RTC_VERTEX_BUFFER );
    unsigned int vertexIdx = 0;
    for( auto vertex : mesh.vertices )
    {
        vertices[vertexIdx].x = vertex.x;
        vertices[vertexIdx].y = vertex.y;
        vertices[vertexIdx].z = vertex.z;
        vertexIdx++;
    }
    rtcUnmapBuffer( scene_, geom_id, RTC_VERTEX_BUFFER );

    Triangle *triangles = ( Triangle * ) rtcMapBuffer( scene_, geom_id, RTC_INDEX_BUFFER );
    for( int triIdx = 0; triIdx < mesh.indices.size() / 3; ++triIdx )
    {
        triangles[triIdx].v0 = mesh.indices[triIdx * 3];
        triangles[triIdx].v1 = mesh.indices[triIdx * 3 + 2];
        triangles[triIdx].v2 = mesh.indices[triIdx * 3 + 1];
    }
    rtcUnmapBuffer( scene_, geom_id, RTC_INDEX_BUFFER );

    geom_id_to_mesh_[geom_id] = mesh;
}

std::vector<point_light_t> raytracer::compute_vpl( point_light_t &light, float root_intensity, unsigned int recursion_depth_left )
{
    std::vector<point_light_t> res;
    res.push_back( light );	// add itself as the VPL first

    if( recursion_depth_left <= 0 ) //preventing stack overflow
        return res;

    // should be a proper stratified quasirandom sampling direction
    glm::vec3 random_dir = random::stratified_sampling( light.direction, [&]()
    {
        return uniform_real_distribution_01_( random_engine_ );
    } );

    // Ray init.
    RTCRay ray;
    std::memcpy( ray.org, glm::value_ptr( light.position ), sizeof( ray.org ) );
    std::memcpy( ray.dir, glm::value_ptr( random_dir ), sizeof( ray.dir ) );
    ray.tnear = kRayTraceEpsilon;
    ray.tfar = INFINITY;
    ray.geomID = RTC_INVALID_GEOMETRY_ID;
    ray.primID = RTC_INVALID_GEOMETRY_ID;
    ray.instID = RTC_INVALID_GEOMETRY_ID;
    ray.mask = 0xFFFFFFFF;
    ray.time = 0.f;

    // Intersect!
    rtcIntersect( scene_, ray );

    // skip if missed
    if( ray.geomID == RTC_INVALID_GEOMETRY_ID )
        return res;
    // skip if russian roulette failed
    //float rr_probability = ( geom_id_to_mesh_[ray.geomID].diffuse_color.x + geom_id_to_mesh_[ray.geomID].diffuse_color.y + geom_id_to_mesh_[ray.geomID].diffuse_color.z ) / 3;
    float rr_probability = glm::length(light.intensity) / root_intensity;
    if( rr_probability <= uniform_real_distribution_01_( random_engine_ ) )
        return res;

    //trace new one
    glm::vec3 ray_org = glm::make_vec3( ray.org );
    glm::vec3 ray_dir = glm::make_vec3( ray.dir );
    glm::vec3 ray_ng = glm::normalize( glm::make_vec3( ray.Ng ) );

	point_light_t lightSample;
	lightSample.position = ray_org + ray_dir * ray.tfar;
	lightSample.intensity = light.intensity             // light color
		* geom_id_to_mesh_[ray.geomID].diffuse_color    // diffuse only
		* glm::dot(-ray_dir, ray_ng)					// Lambertian cosine term
		/ rr_probability;								// Russian Roulette weight
	lightSample.direction = ray_ng;

    // recurse to make a global illumination
    std::vector<point_light_t> vpls = compute_vpl( lightSample, root_intensity, recursion_depth_left - 1 );
    res.insert( res.end(), vpls.begin(), vpls.end() );

    return res;
}

std::vector<point_light_t> raytracer::compute_vpl( area_light_t light, unsigned int light_sample_count )
{
    std::vector<point_light_t> res;

    for( unsigned int light_sample_idx = 0; light_sample_idx < light_sample_count; ++light_sample_idx )
    {
        glm::vec3 light_pos = random::stratified_sampling( light.aabb_min, light.aabb_max, light_sample_idx, light_sample_count, [&]()
        {
            return uniform_real_distribution_jitter_( random_engine_ );
        } );
        point_light_t light_sample;
        light_sample.position = light_pos;
        light_sample.direction = light.direction;

        glm::vec2 area = ( light.aabb_max - light.aabb_min ).xz();
        light_sample.intensity =
            light.intensity					// L(x)
            * ( area.x * area.y )			// 1/pdf of light sampling
            / float( light_sample_count );	// Monte Carlo integration divisor

        std::vector<point_light_t> vpls = compute_vpl( light_sample, glm::length(light_sample.intensity) );
        res.insert( res.end(), vpls.begin(), vpls.end() );
    }

    return res;
}