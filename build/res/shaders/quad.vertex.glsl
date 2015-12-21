#version 330 core

uniform sampler2D u_Tex;

in vec3 Position;
in vec3 Normal;
in vec2 Texcoord;

out vec2 fs_Texcoord;

void main()
{
    fs_Texcoord = Texcoord;
    gl_Position = vec4( Position, 1.0f );
}
