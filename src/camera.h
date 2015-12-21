#ifndef _CAMERA_H_
#define _CAMERA_H_

#include <glm/glm.hpp>

class camera_t
{
public:
    camera_t( glm::vec3 start_pos, glm::vec3 start_dir, glm::vec3 up, glm::mat4 perspective_matrix = glm::mat4() ) :
        pos( start_pos ), initUp( up ), initLookingAtDir( glm::normalize( start_dir ) ), perspective_matrix_( perspective_matrix )
    { }

    void rotate( glm::vec3 eulerAngleDelta );
    void translate( glm::vec3 distance );
    glm::mat4x4 get_view();
    glm::mat4 get_perspective()
    {
        return perspective_matrix_;
    }
    void set_perspective( const glm::mat4 &perspective_matrix )
    {
        perspective_matrix_ = perspective_matrix;
    }

    glm::vec3 eulerAngle;
    glm::vec3 pos;
    glm::vec3 initUp;
    glm::vec3 initLookingAtDir;

protected:
    glm::mat4 perspective_matrix_;
};

#endif
