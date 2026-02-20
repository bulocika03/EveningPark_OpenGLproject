#version 410 core

layout (location = 0) in vec3 aPos;

uniform mat4 projection;
uniform mat4 view;

out float alpha;
out float height;

void main()
{
    gl_Position = projection * view * vec4(aPos, 1.0);
    gl_PointSize = 4.0;
    
    height = aPos.y;
    alpha = clamp(0.4 + (1.0 - aPos.y / 50.0) * 0.4, 0.3, 0.8);
}
