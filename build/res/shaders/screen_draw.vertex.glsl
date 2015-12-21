#version 330 core

uniform mat4 u_modelMat;
uniform mat4 u_viewMat;
uniform mat4 u_perspMat;
uniform vec3 u_vplPosition;
uniform vec3 u_vplIntensity;
uniform vec3 u_vplDirection;
uniform int u_numLights;
uniform vec3 u_ambientColor;
uniform vec3 u_diffuseColor;

in  vec3 Position;
in  vec3 Normal;

out vec3 fs_ViewNormal;
out vec3 fs_ViewPosition;
out vec3 fs_ViewLightPos;
out vec3 fs_LightIntensity;

void main( void )
{
    fs_ViewNormal = ( u_viewMat * u_modelMat * vec4( Normal, 0.0 ) ).xyz;
    fs_ViewPosition = ( u_viewMat * u_modelMat * vec4( Position, 1.0 ) ).xyz;
    fs_ViewLightPos = ( u_viewMat * u_modelMat * vec4( u_vplPosition, 1.0 ) ).xyz;
    gl_Position = u_perspMat * u_viewMat * u_modelMat * vec4( Position, 1.0 );
}