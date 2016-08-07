#version 330 core

layout(points) in;
layout(line_strip, max_vertices=256)out;
uniform mat4 pers_matrix;
uniform mat4 camera_trans;
const int num_hori_line = 4;
out vec4 color;

mat4 trans;
float grid_x;
float grid_z;

void horizontal_line(int i) {
       gl_Position = trans * vec4(grid_x - 60, 0, grid_z + 20 * i, 1);
       color = vec4(1, 1, 1, 0);
       EmitVertex();
       gl_Position = trans * vec4(grid_x, 0, grid_z + 20 * i, 1);
       color = vec4(1, 1, 1, 1);
       EmitVertex();
       gl_Position = trans * vec4(grid_x + 60, 0, grid_z + 20 * i, 1);
       color = vec4(1, 1, 1, 0);
       EmitVertex();
       EndPrimitive();
}

void main() {
   float x = gl_in[0].gl_Position.x;
   float z = gl_in[0].gl_Position.z;
   grid_x = trunc(x / 20.0) * 20;
   grid_z = trunc(z / 20.0) * 20;
   trans = pers_matrix * camera_trans;
   for (int i = 0; i < num_hori_line; i++) {
       horizontal_line(i);
       horizontal_line(-i);
   }
   for (int i = -40; i <= 40; i += 20) {
      vec4 lcolor = vec4(1, 1, 1, 1.0 / (abs(i) / 10 + 1));
      float xi = grid_x + i;
      color = lcolor;
      gl_Position = trans * vec4(xi, 0, grid_z - 80, 1);
      EmitVertex();
      color = lcolor;
      gl_Position = trans * vec4(xi, 0, grid_z + 80, 1);
      EmitVertex();
      EndPrimitive();
   }
}