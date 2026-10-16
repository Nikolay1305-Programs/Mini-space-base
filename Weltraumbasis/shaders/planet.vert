#version 330 core
layout (location = 0) in vec2 aPos;

uniform vec2  uOffset;
uniform float uScale;

out vec2 vLocal; // [-1..1] локални координати в диска

void main()
{
    vLocal = aPos;
    vec2 p = aPos * uScale + uOffset;
    gl_Position = vec4(p, 0.0, 1.0);
}
