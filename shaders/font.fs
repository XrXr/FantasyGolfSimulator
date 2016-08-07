#version 330

out vec4 output_color;
in vec2 tex_coord;
uniform sampler2D sampler;
uniform vec2 texture_dimentions = vec2(256, 256);
void main() {
   vec4 sample = texture(sampler, vec2(tex_coord.x/texture_dimentions[0], tex_coord.y/texture_dimentions[1]));
   float a = float(length(sample.xyz) > 0);
   output_color = vec4(sample.xyz, a);
}