#version 330 core
in  vec2 UV;
out vec4 FragColor;
uniform sampler2D myTextureSampler;
void main(){
    vec4 tex = texture(myTextureSampler, UV);
    tex.g = 0;
    tex.b = 0;
    FragColor = tex;
}