#version 330 core

in  vec2 UV;
out vec4 FragColor;

uniform vec2 resolution;

float weights[9] = float[](0.1, 0.2, 0.3, 0.4, 0.5, 0.4, 0.3, 0.2, 0.1);

uniform sampler2D myTextureSampler;
uniform vec2 direction;

void main(){
    FragColor = vec4(0.0, 0.0, 0.0, 0.0);
    vec2 texelSize = 1.0 / resolution;
    
    for (int i = -4; i <= 4; i++) {
        vec2 offset = direction * float(i) * texelSize;
        FragColor += texture(myTextureSampler, UV + offset) * weights[i + 4];
    }
}