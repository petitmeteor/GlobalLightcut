#version 330 core

layout (location = 0) in vec3 Position ;
layout (location = 1) in vec3 Normal ; 

uniform mat4 u_modelMat ;
uniform mat4 u_viewMat ;
uniform mat4 u_perspMat ;
uniform vec3 u_diffuseColor;

out vec3 WorldPos ;
out vec3 WorldNormal ;

void main()
{
    gl_Position = u_perspMat * u_viewMat * u_modelMat * vec4( Position, 1.0 ) ;
	WorldPos = ( u_modelMat * vec4( Position, 1.0 ) ).xyz ;
	WorldNormal = ( u_modelMat * vec4( Normal, 0.0 ) ).xyz ;
}
