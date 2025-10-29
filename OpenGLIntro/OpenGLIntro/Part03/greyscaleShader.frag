#version 330 core
in  vec2 UV;
out vec4 FragColor;
uniform sampler2D myTextureSampler;
void main(){
    vec4 tex = texture(myTextureSampler, UV);
    float mean = (tex.r + tex.g + tex.b)/3;
    tex.r = mean;
    tex.g = mean;
    tex.b = mean;
    FragColor = tex;
}