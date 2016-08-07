#version 330

layout(location = 0) in vec2 position;
layout(location = 1) in vec2 tc;
out vec2 tex_coord;
out vec2 c_dimentions;
layout(std140) uniform shared {
   vec2 window_dimentions;
};

void main() {
    gl_Position = vec4(-1 + position.x * 2 / window_dimentions[0],
                        1 - position.y * 2 / window_dimentions[1], 0, 1);
    tex_coord = tc;
}