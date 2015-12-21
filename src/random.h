#ifndef _RANDOM_H_
#define _RANDOM_H_

#include <glm/glm.hpp>

namespace random
{
inline float radical_inverse_VdC( unsigned int bits )
{
    bits = ( bits << 16u ) | ( bits >> 16u );
    bits = ( ( bits & 0x55555555u ) << 1u ) | ( ( bits & 0xAAAAAAAAu ) >> 1u );
    bits = ( ( bits & 0x33333333u ) << 2u ) | ( ( bits & 0xCCCCCCCCu ) >> 2u );
    bits = ( ( bits & 0x0F0F0F0Fu ) << 4u ) | ( ( bits & 0xF0F0F0F0u ) >> 4u );
    bits = ( ( bits & 0x00FF00FFu ) << 8u ) | ( ( bits & 0xFF00FF00u ) >> 8u );
    return float( bits ) * 2.3283064365386963e-10f; // / 0x100000000
}

// produces a sample from 2D Hammersley sequence ([0-1], [0-1])
inline glm::vec2 hammersley2d( unsigned int i, unsigned int N )
{
    return glm::vec2( float( i ) / float( N ), radical_inverse_VdC( i ) );
}

// produces a jittered sample from 2D Hammersley sequence ([0-1], [0-1])
template<class JitterGeneratorType>
inline glm::vec2 hammersley2d_jittered( unsigned int i, unsigned int N, JitterGeneratorType jitter_generator )
{
    return glm::clamp( hammersley2d( i, N ) + glm::vec2( jitter_generator(), jitter_generator() ), glm::vec2( 0, 0 ), glm::vec2( 1, 1 ) );
}

/* Hammersley points on hemisphere:
* http://holger.dammertz.org/stuff/notes_HammersleyOnHemisphere.html */
template<class RandomGeneratorType>
glm::vec3 stratified_sampling( glm::vec3 normalVec, RandomGeneratorType random_01_generator )
{
    glm::vec2 hammersley( random_01_generator(), random_01_generator() );
    float phi = hammersley.y * 2.0f * PI;
    float cosTheta = 1.0f - hammersley.x;
    float sinTheta = sqrt( 1.0f - cosTheta * cosTheta );

    // point over hemisphere with normal vector (0, 0, 1)
    glm::vec3 point = glm::vec3( cos( phi ) * sinTheta, sin( phi ) * sinTheta, cosTheta );

    glm::vec3 b1( 0.f ), b2;
    if( glm::abs( normalVec.x ) < kRayTraceEpsilon )
        b1[0] = 1.f;
    else
        b1[1] = 1.f;
    b1 = glm::cross( b1, normalVec );
    b2 = glm::cross( normalVec, b1 );

    b1 = glm::normalize( b1 );
    b2 = glm::normalize( b2 );

    return point.x * b1 + point.y * b2 + point.z * normalVec;
}

template<class JitterGeneratorType>
glm::vec3 stratified_sampling( glm::vec3 bbMin, glm::vec3 bbMax, unsigned int i, unsigned int N, JitterGeneratorType jitter_generator )
{
    glm::vec2 hammersley = random::hammersley2d_jittered( i, N, jitter_generator );
    glm::vec3 samplePoint = glm::vec3( hammersley.x, 0, hammersley.y );
    return bbMin + ( bbMax - bbMin ) * samplePoint;
}
}

#endif
