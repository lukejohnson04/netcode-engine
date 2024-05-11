
#version 430 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D _texture;

void main()
{
    FragColor = texture(_texture, TexCoord);
}
