#version 410 core

in float alpha;
in float height;
out vec4 FragColor;

void main()
{
    // Creeaza o picatura elongata vertical pentru efect de ploaie
    vec2 coord = gl_PointCoord - vec2(0.5);
    
    float distY = coord.y * 0.3;
    float distX = coord.x;
    float dist = sqrt(distX * distX + distY * distY);
    
    float fade = 1.0 - smoothstep(0.3, 0.5, dist);
    
    vec3 rainColor = vec3(0.7, 0.75, 0.85);
    
    FragColor = vec4(rainColor, alpha * fade);
}
