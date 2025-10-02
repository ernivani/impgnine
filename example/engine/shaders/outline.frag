#version 450

layout(location = 0) out vec4 outColor;

void main() {
    // White mask for selected entity
    outColor = vec4(1.0, 1.0, 1.0, 1.0);
}
