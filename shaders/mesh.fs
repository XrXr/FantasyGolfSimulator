#version 330 core

in vec3 nnormal;
in vec3 frag_world_pos;
in vec3 lp;
out vec4 output_color;
uniform vec4 force_color;
uniform vec3 light_color = vec3(1);
uniform vec3 surface_color;

void main() {
   vec3 norm = normalize(nnormal);
   vec3 light_dir = normalize(lp - frag_world_pos);
   float diff = max(dot(light_dir, norm), 0.0);
   vec3 diffuse = diff * light_color;
   output_color = max(vec4(diffuse * surface_color, 1.0), force_color);
}