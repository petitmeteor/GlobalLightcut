#version 330 core

in vec4 v_position;
out vec4 outColor;

void main()
{
    float depth = length( vec3( v_position ) );
    outColor = vec4( depth, 0.f, 0.f, 0.f );
};