#version 330

in vec4 color;
out vec4 output_color;

void main() {
   output_color = color.a * color;
}