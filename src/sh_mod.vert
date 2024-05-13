
#version 430 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec2 aTexCoord1;
layout (location = 2) in vec2 aTexCoord2;

uniform mat4 model;
uniform mat4 projection;

out vec2 TexCoord1;
out vec2 TexCoord2;

void main()
{
    gl_Position = projection * model * vec4(aPos, 1.0);
    TexCoord1 = aTexCoord1;
    TexCoord2 = aTexCoord2;
}
