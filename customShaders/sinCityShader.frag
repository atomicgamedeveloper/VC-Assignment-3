#version 330 core
in  vec2 UV;
out vec4 FragColor;
uniform sampler2D myTextureSampler;

void main() {
    vec3 texColor = texture(myTextureSampler, UV).rgb;
    
    // Posterize
    texColor = floor(texColor * 4.0) / 4.0;
    
    float r = texColor.r;
    float g = texColor.g;
    float b = texColor.b;
    
    // Red mask
    bool isRed = (r > 0.49) && (g < 0.1) && (b < 0.1);
    
    // Greyscale
    float gray = 0.299 * r + 0.587 * g + 0.114 * b;
    
    // Black and white threshold
    float bw = (gray > 0.2) ? 1.0 : 0.0;
    
    // Apply colors
    if (isRed) {
        FragColor = vec4(1.0, 0.0, 0.0, 1.0);
    } else {
        FragColor = vec4(bw, bw, bw, 1.0);
    }
}