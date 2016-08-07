#version 330 core

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 normal_in;
out vec3 nnormal;
out vec3 frag_world_pos;
out vec3 lp;

uniform mat4 model_to_world;
uniform mat4 pers_matrix;
uniform mat4 camera_trans;
uniform vec3 light_pos = vec3(0, 0, 2);
uniform mat4 post_pers_trans;

void main() {
    vec4 pos = camera_trans * model_to_world * vec4(position, 1);
    gl_Position = post_pers_trans * pers_matrix * pos;
    frag_world_pos = vec3(model_to_world * vec4(position, 1));
    nnormal = mat3(transpose(inverse(model_to_world))) * normal_in;
    lp = light_pos;
}