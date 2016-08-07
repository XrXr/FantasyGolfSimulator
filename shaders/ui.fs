#version 330

out vec4 output_color;
uniform vec4 color = vec4(1, 1, 1, 1);

void main() {
   output_color = color;
}