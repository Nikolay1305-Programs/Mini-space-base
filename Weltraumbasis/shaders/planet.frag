#version 330 core
out vec4 FragColor;

in vec2 vLocal;

uniform float uAspect; // width/height
uniform float uTime;

float hash(vec2 p) {
    // прост шум
    return fract(sin(dot(p, vec2(127.1, 311.7))) * 43758.5453);
}

float noise(vec2 p) {
    vec2 i = floor(p);
    vec2 f = fract(p);
    float a = hash(i);
    float b = hash(i + vec2(1,0));
    float c = hash(i + vec2(0,1));
    float d = hash(i + vec2(1,1));
    vec2 u = f*f*(3.0-2.0*f);
    return mix(a,b,u.x) + (c-a)*u.y*(1.0-u.x) + (d-b)*u.x*u.y;
}

void main()
{
    // корекция за aspect, за да е кръг (не елипса)
    vec2 p = vec2(vLocal.x * uAspect, vLocal.y);
    float r = length(p);

    // Планетата е диск с anti-alias край
    float edge = smoothstep(1.0, 0.985, r);
    if (edge <= 0.0) discard;

    // Осветление: светлина от горе-ляво (в screen space)
    vec2 lightDir = normalize(vec2(-0.6, 0.7));

    // "Нормал" на сферата (приближение): z = sqrt(1-r^2)
    float z = sqrt(max(0.0, 1.0 - r*r));
    vec3 n = normalize(vec3(p, z));
    vec3 l = normalize(vec3(lightDir, 0.6));

    float diff = clamp(dot(n, l), 0.0, 1.0);

    // Пояси (Jupiter bands) + лек шум
    float bands = 0.5 + 0.5 * sin((vLocal.y * 14.0) + uTime * 0.2);
    float clouds = noise(vLocal * 6.0 + vec2(uTime * 0.05, 0.0));
    float pattern = mix(bands, clouds, 0.35);

    // Цветова палитра (jupiter-ish)
    vec3 base1 = vec3(0.82, 0.72, 0.60);
    vec3 base2 = vec3(0.65, 0.52, 0.38);
    vec3 col = mix(base1, base2, pattern);

    // Малко "Great Red Spot" (лек намек)
    vec2 spotP = vLocal - vec2(-0.15, -0.05);
    float spot = smoothstep(0.16, 0.0, length(vec2(spotP.x*uAspect, spotP.y)));
    col = mix(col, vec3(0.75, 0.32, 0.22), spot * 0.7);

    // Шейдинг + атмосферен ръб
    float rim = smoothstep(0.2, 1.0, z);          // по-светъл ръб
    col *= (0.25 + 0.85 * diff);
    col += vec3(0.08, 0.08, 0.10) * (1.0 - rim);

    FragColor = vec4(col, edge);
}
