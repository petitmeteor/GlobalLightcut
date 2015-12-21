#version 330 core

uniform sampler2D u_Tex;

in vec2 fs_Texcoord;

out vec4 outColor;

void main()
{
    outColor = texture2D( u_Tex, fs_Texcoord );
}

