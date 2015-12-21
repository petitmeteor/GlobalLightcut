#include "camera.h"

#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/transform2.hpp>
#include <glm/gtx/euler_angles.hpp>

// yaw-pitch-roll
void camera_t::rotate( glm::vec3 eulerAngleDelta )
{
    eulerAngle += eulerAngleDelta;
}

void camera_t::translate( glm::vec3 distance )
{
    pos += distance;
}

glm::mat4x4 camera_t::get_view()
{
    glm::mat4 rotMat = glm::orientate4( eulerAngle );
    glm::vec3 lookingAtDir = glm::vec3( glm::vec4( initLookingAtDir, 0 ) * rotMat );
    glm::vec3 up = glm::vec3( glm::vec4( initUp, 0 ) * rotMat );
    return glm::lookAt( pos, pos + lookingAtDir, up );
}