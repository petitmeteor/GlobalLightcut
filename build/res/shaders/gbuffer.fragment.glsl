#version 330 core

in vec3 WorldPos ;
in vec3 WorldNormal ;

uniform mat4 u_modelMat ;
uniform mat4 u_viewMat ;
uniform mat4 u_perspMat ;
uniform vec3 u_diffuseColor;

layout (location = 0) out vec3 WorldPosOut ;
layout (location = 1) out vec3 WorldNormalOut ;
layout (location = 2) out vec3 DiffuseOut ;

void main()
{
	WorldPosOut = WorldPos ;
	WorldNormalOut = WorldNormal ;
	DiffuseOut = u_diffuseColor ;
}

