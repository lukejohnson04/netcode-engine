
#version 430 core
out vec4 FragColor;

in vec2 TexCoord1;
in vec2 TexCoord2;

uniform sampler2D _texture1;
uniform sampler2D _texture2;

void main()
{
    vec4 t1 = texture(_texture1, TexCoord1);
    vec4 t2 = texture(_texture2, TexCoord2);
    if (t1.a < 1.0) {
        FragColor = t1;
    } else {
        FragColor = t2;
    }
}
