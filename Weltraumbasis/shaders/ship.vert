#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec3 aColor;

uniform vec2  uOffset;
uniform float uScale;
uniform float uAngle;

out vec3 vColor;

void main()
{
    float c = cos(uAngle);
    float s = sin(uAngle);
    mat2 R = mat2(c, -s,
                  s,  c);

    vec2 p = R * aPos;
    p = p * uScale + uOffset;

    gl_Position = vec4(p, 0.0, 1.0);
    vColor = aColor;
}
