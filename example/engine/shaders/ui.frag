#version 450
layout(location = 0) in vec4 fragColor;
layout(location = 1) in vec2 fragUV;
layout(binding = 0) uniform sampler2D texSampler;
layout(location = 0) out vec4 outColor;
void main() {
    // If UV is (0,0), render solid color (UI elements)
    // Otherwise sample texture (text)
    if (fragUV.x == 0.0 && fragUV.y == 0.0) {
        outColor = fragColor;
    } else {
        float alpha = texture(texSampler, fragUV).r;
        outColor = vec4(fragColor.rgb, fragColor.a * alpha);
    }
}

