#include <GL/glew.h>
#include <GL/freeglut.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>

#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"
#include "common.h"
#include "clock.h"
#include "font.h"
#include "text_box.h"

#define print_vec3(V)  printf(#V": x=%f y=%f z=%f\n", V.x, V.y, V.z)

const char* vertex_shader =
"#version 330\n"

"layout(location = 0) in vec3 position;"
//"layout(location = 1) in vec4 in_color;"
"out vec4 vertex_color;"

"uniform mat4 model_to_world;"
"uniform mat4 pers_matrix;"
"uniform mat4 camera_trans;"

"void main()"
"{"
"    vec4 pos = camera_trans * model_to_world * vec4(position, 1);"
"    gl_Position = pers_matrix * pos;"
"    mat3 modelToCameraM = mat3(camera_trans * model_to_world);"
"    vec3 normCamSpace = normalize(modelToCameraM * position);"
"    float cosAngIncidence = dot(normCamSpace, position);"
"    cosAngIncidence = clamp(cosAngIncidence, 0, 1);"
"    float dist = length(position);"
"    vertex_color = cosAngIncidence * (1/pow(((dist/5) + 1), 2)) *  vec4(1, 1, 1, 1);"
"}";

const char* frag_shader =
"#version 330\n"

"out vec4 output_color;"
"in vec4 vertex_color;"
"uniform vec4 force_color;"
"void main()"
"{"
"   output_color = max(vertex_color, force_color);"
"}";


const char* window_space_vertex_shader =
"#version 330\n"

"layout(location = 0) in vec2 position;"
"layout(location = 1) in vec2 char_dimentions;"
"layout(location = 2) in vec2 tc;"
"out vec2 tex_coord;"
"out vec2 c_dimentions;"
"layout(std140) uniform shared {"
"   vec2 window_dimentions;"
"};"

"void main()"
"{"
"    gl_Position = vec4(-1 + position.x * 2 / window_dimentions[0],"
"                        1 - position.y * 2 / window_dimentions[1], 0, 1);"
"    tex_coord = tc;"
"}";

const char* font_fragment_shader =
"#version 330\n"

"out vec4 output_color;"
"in vec2 tex_coord;"
"uniform sampler2D sampler;"
"uniform vec2 texture_dimentions = vec2(256, 256);"
"void main()"
"{"
"   output_color = texture(sampler, vec2(tex_coord.x/texture_dimentions[0], tex_coord.y/texture_dimentions[1]));"
"}";

const char* ui_frag_shader =
"#version 330\n"

"out vec4 output_color;"
"uniform vec4 color = vec4(1, 1, 1, 1);"
"void main()"
"{"
"   output_color = color;"
"}";

GLuint shader_program;
GLuint ui_program;
GLuint window_space_program;
GLuint window_dimentions_uni;
GLuint mesh_buffer;
GLuint grid_buffer;
GLuint rotateTransUni;
GLuint force_color_uni;
GLuint camera_trans_uni;
GLuint pers_matrix_uni;
GLuint model_to_world;
GLuint trail_buffer;
GLuint text_vbo;
GLuint ui_vert_buf;
GLuint ui_idx_buf;

void camera_pan(vec3* look, float factor);
void camera_move_in(vec3* look, float factor);
void camera_y_translate(float factor);
void input_to_active_field(unsigned char key);

const float fFrustumScale = 2.0f, fzNear = 0.025f, fzFar = 10000.0f;
const float CAMERA_SPEED = 200.0f;
const int GRID_LENGTH = 1000;
const size_t TRAIL_BUF_SIZE = sizeof(float) * 3 * 500000;
vec3 camera_pos = {0, 3, 18};
bool key_buf[256];
struct { bool up, down, left, right; } arrow_key;
float y_rot_angle = 0;
float x_rot_angle = 0;
uint64_t last_sim_stamp = 0;
float pers_matrix[16];
size_t golf_mesh_vert_count;
int screen_width;
int screen_height;
int window_width;
int window_height;
bool window_has_focus = false;
bool free_cam_mode = false;
float y_init_launch = 150;
#define NUM_FILEDS 2
text_box_t fields[] = {
    {375, 80, 10, 175, false, "how are you doing"},
    {375, 80, 10, 300, false, "I'm different"},
};
text_box_t* active_field = NULL;

vec3 normalize3(const vec3 v) {
    const float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    vec3 ret = {
        v.x / len,
        v.y / len,
        v.z / len
    };
    return ret;
};

vec3 calc_lookat(const vec3 eye, const float y_rotate, const float x_rotate) {
    const float rot_rad = y_rotate * (M_PI / 180);
    const float x_roate_rad = x_rotate * (M_PI / 180);
    const float cos_a = cos(rot_rad);
    const float sin_a = sin(rot_rad);
    const vec3 result = {
        sin_a,
        -sin(x_roate_rad),
        -cos_a
    };

    return normalize3(result);
}

vec3 cross(const vec3 a, const vec3 b) {
    const vec3 ret = {
        a.y * b.z - b.y * a.z,
        a.z * b.x - b.z * a.x,
        a.x * b.y - b.x * a.y
    };
    return ret;
}

void matrix_mul_2(float (*a)[16], float (*b)[16], float (*out)[16]) {
    memset(*out, 0, sizeof(*out));
    for (int row = 0; row < 4; row++) {
        float* a_base = (*a) + row;
        for (int col = 0; col < 4; col++) {
            float* b_base = (*b) + (4 * col);
            const size_t out_offset = col * 4 + row;
            for (int i = 0; i < 4; i++) {
                (*out)[out_offset] += a_base[i * 4] * b_base[i];
            }
        }
    }
}

void matrix_mul(float (**in)[16], size_t size, float (*out)[16]) {
    assert(size >= 2);

    float cur[16];
    float tmp[16];
    matrix_mul_2(in[0], in[1], &cur);
    for (size_t i = 2; i < size; i++) {
        matrix_mul_2(&cur, in[i], &tmp);
        memcpy(cur, tmp, sizeof(cur));
    }
    memcpy(*out, cur, sizeof(cur));
}

// Rotate about an axis by an angle in degrees using Rodrigues' rotation
// formula. Found on Wikipedia
mat4 rotate(float angle, vec3 axis) {
    const float a_rad = angle * (M_PI / 180);
    const float c = cos(a_rad);
    const float s = sin(a_rad);
    const float c_comp = 1 - c;
    const float xz = axis.x * axis.z;
    const float xy = axis.x * axis.y;
    const float yz = axis.y * axis.z;

    mat4 ret = { {0} };
    ret.value[0] = c + (axis.x * axis.x * c_comp);
    ret.value[1] = xy * c_comp + axis.z * s;
    ret.value[2] = (-axis.y) * s + xz * c_comp;

    ret.value[4] = xy * c_comp - axis.z * s;
    ret.value[5] = c + (axis.y * axis.y * c_comp);
    ret.value[6] = axis.x * s + yz * c_comp;

    ret.value[8] = axis.y * s + xz * c_comp;
    ret.value[9] = (-axis.x) * s + yz * c_comp;
    ret.value[10] = c + (axis.z * axis.z * c_comp);

    ret.value[15] = 1;
    return ret;
}

static void check_errors(const char* desc) {
    GLenum e = glGetError();
    if (e != GL_NO_ERROR) {
        fprintf(stderr, "OpenGL error in \"%s\": %#x\n", desc, e);
        exit(20);
    }
}

float dot3(const vec3 a, const vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}

float qudratic(const float t) {
    return -((600 * t) - ((t*t*t) / 3));
}

float flight_z(const float z) {
    return qudratic(z);
}

float flight_y(const float t) {
    return (y_init_launch * t) - (3.26667 * t * t * t);
}

float flight_time = 0;
bool flying = false;
bool space_released_once = true;
vec3 golf_ball_pos = {0};
const float TRAIL_SAMPLE_FREQ = 0.002f;
size_t trail_buf_offset = sizeof(vec3);
float last_trail_sample_time;
bool blinker_present = false;

void flight_init(void) {
    sscanf(fields[0].content, "%f", &y_init_launch);
    glBindBuffer(GL_ARRAY_BUFFER, trail_buffer);
    glBufferData(GL_ARRAY_BUFFER, TRAIL_BUF_SIZE, NULL, GL_DYNAMIC_DRAW);
    vec3 first_point = {0};
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(first_point), &first_point);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    trail_buf_offset = sizeof(first_point);

    golf_ball_pos = (vec3){0};
    flight_time = 0;
    last_trail_sample_time = 0;
}

void draw(void) {
    // sim
    uint64_t t;
    bool ret = elapsed_ns(&t);
    const uint64_t since_last = t - last_sim_stamp;
    const float seconds_since_last = since_last / 1000000000.0f;
    const float step = CAMERA_SPEED * seconds_since_last;
    last_sim_stamp = t;

    blinker_present = ((t / 1000000000) % 2) == 0;

    if (free_cam_mode) {
        int in_out_dir = 0;
        int pan_dir = 0;
        int y_translate_dir = 0;

        pan_dir += (key_buf['a'] || arrow_key.left) * -1;
        pan_dir += (key_buf['d'] || arrow_key.right);

        in_out_dir += (key_buf['s'] || arrow_key.down) * -1;
        in_out_dir += (key_buf['w'] || arrow_key.up);

        y_translate_dir += key_buf['q'] * -1;
        y_translate_dir += key_buf['e'];

        vec3 look = calc_lookat(camera_pos, y_rot_angle, x_rot_angle);
        camera_pan(&look, pan_dir * step);
        camera_move_in(&look, in_out_dir * step);
        camera_y_translate(y_translate_dir * step);
    }

    const bool old_flying = flying;

    if (key_buf[' ']) {
        if (flying) {
            flying = !space_released_once;
        } else {
            flying = space_released_once;
        }
        space_released_once = false;
    } else {
        space_released_once = true;
    }

    if (flying) {
        if (golf_ball_pos.y <= 0) {
            flight_init();
        }
        flight_time += seconds_since_last;

        golf_ball_pos.z = flight_z(flight_time);
        golf_ball_pos.y = flight_y(flight_time);
    }

    // sim end

    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glUseProgram(shader_program);

    {  // compute and send the camera transform matrix
        float c_translate[16] = {0};
        c_translate[0] = 1;
        c_translate[5] = 1;
        c_translate[10] = 1;
        c_translate[15] = 1;

        c_translate[12] = -camera_pos.x;
        c_translate[13] = -camera_pos.y;
        c_translate[14] = -camera_pos.z;

        float c_y_rot[16] = {0};
        float angle = y_rot_angle * (M_PI / 180);
        c_y_rot[0] = cos(angle);
        c_y_rot[2] = -sin(angle);
        c_y_rot[5] = 1;
        c_y_rot[8] = sin(angle);
        c_y_rot[10] = cos(angle);
        c_y_rot[15] = 1;

        float c_x_rot[16] = {0};
        angle = x_rot_angle * (M_PI / 180);
        c_x_rot[0] = 1;
        c_x_rot[5] = cos(angle);
        c_x_rot[6] = sin(angle);
        c_x_rot[9] = -sin(angle);
        c_x_rot[10] = cos(angle);
        c_x_rot[15] = 1;

        float (*(mult_buf)[3])[16] = {&c_x_rot, &c_y_rot, &c_translate};
        float trans[16];
        matrix_mul(mult_buf, 3, &trans);
        glUniformMatrix4fv(camera_trans_uni, 1, GL_FALSE, trans);
    }

    {
        float mesh_model_trans[16] = {[0]=.2f, [5]=.2f, [10]=.2f, [15]=1};
        mesh_model_trans[12] = golf_ball_pos.x;
        mesh_model_trans[13] = golf_ball_pos.y;
        mesh_model_trans[14] = golf_ball_pos.z;
        glUniformMatrix4fv(model_to_world, 1, GL_FALSE, mesh_model_trans);
    }

    glUniform4f(force_color_uni, 0, 0, 0, 0);

    glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_TRIANGLES, 0, golf_mesh_vert_count);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    check_errors("draw mesh");

    float id[16] = {[0]=1, [5]=1, [10]=1, [15]=1};
    glUniformMatrix4fv(model_to_world, 1, GL_FALSE, id);
    glUniform4f(force_color_uni, 1, 1, 1, 1);  // the grid's color
    glBindBuffer(GL_ARRAY_BUFFER, grid_buffer);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawArrays(GL_LINES, 0, GRID_LENGTH / 10 * sizeof(vec3));
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    check_errors("draw grid");

    if (golf_ball_pos.z != 0) {  // draw the golf ball's trail
        glBindBuffer(GL_ARRAY_BUFFER, trail_buffer);

        const float dt = flight_time - last_trail_sample_time;
        const size_t n_verts = floor(dt / TRAIL_SAMPLE_FREQ);
        const size_t upload_size = n_verts * sizeof(vec3);
        const bool enough_space = trail_buf_offset + upload_size <= TRAIL_BUF_SIZE;
        if (flying && n_verts > 0 && enough_space) {
            vec3* points = malloc(upload_size);
            size_t i = 0;
            float t = last_trail_sample_time + TRAIL_SAMPLE_FREQ;
            for (; t <= flight_time && i < n_verts; t += TRAIL_SAMPLE_FREQ) {
                points[i++] = (vec3){0, flight_y(t), flight_z(t)};
            }
            last_trail_sample_time = t - TRAIL_SAMPLE_FREQ;
            if (i == n_verts) {
                glBufferSubData(GL_ARRAY_BUFFER, trail_buf_offset, upload_size,
                                points);
                trail_buf_offset += upload_size;
            } else {
                const size_t real_upload_size = i * sizeof(vec3);
                glBufferSubData(GL_ARRAY_BUFFER, trail_buf_offset,
                                real_upload_size, points);
                trail_buf_offset += real_upload_size;
                printf("Trail prediction missmatch pred:%zu actual %zu\n",
                       n_verts, i);
            }
            free(points);
        }
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        assert(trail_buf_offset % (sizeof(float) * 3) == 0);
        glDrawArrays(GL_LINE_STRIP, 0, trail_buf_offset / (sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        check_errors("draw trail");
    }

    glUseProgram(window_space_program);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    const int stride = sizeof(float) * 2 * 3;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    // TODO REMOVE
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void *) (sizeof(float) * 2));
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void *) (sizeof(float) * 2 * 2));

    char flight_time_str[100];
    sprintf(flight_time_str, "t %.4f", flight_time);
    size_t font_vert_buf_offset = 0;
    render_string(flight_time_str, 0, 0, &font_vert_buf_offset);

    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    for (int i = 0; i < NUM_FILEDS; i++)
        text_box_render_font(fields + i, &font_vert_buf_offset);
    glDrawArrays(GL_TRIANGLES, 0, font_vert_buf_offset / sizeof(float) / 6);
    check_errors("draw text");

    glUseProgram(ui_program);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vert_buf);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui_idx_buf);
    size_t vert_offset = 0;
    size_t idx_offset = 0;
    for (int i = 0; i < NUM_FILEDS; i++)
        text_box_render_frame(fields + i, &vert_offset, &idx_offset);
    check_errors("fill box frames buffer");
    if (blinker_present && active_field) {
        const size_t num_chars = strlen(active_field->content);
        const float blinker_x = active_field->x + TEXT_BOX_PADDING +
                                num_chars * 19;
        const float verts[] = {
            // TODO: build time insert named struct for x_advance
            blinker_x,
            active_field->y + TEXT_BOX_PADDING,

            blinker_x,
            active_field->y + TEXT_BOX_PADDING +
                active_field->height - TEXT_BOX_PADDING,
        };
        const unsigned short cur_idx = text_box_vert_idx(idx_offset);
        const unsigned short idxes[] = {cur_idx, cur_idx + 1};
        glBufferSubData(GL_ARRAY_BUFFER, vert_offset, sizeof(verts), verts);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, idx_offset, sizeof(idxes), idxes);
        vert_offset += sizeof(verts);
        idx_offset += sizeof(idxes);
    }
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glDrawElements(GL_LINE_STRIP, idx_offset / sizeof(unsigned short), GL_UNSIGNED_SHORT, 0);
    check_errors("draw text box frames");

    if (flying && golf_ball_pos.y < 0.0f) {
        printf("Final z %f. Flight time %f \n", golf_ball_pos.z, flight_time);
        flying = false;
    }

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);

    glutSwapBuffers();
    glutPostRedisplay();
}

void idle(void) {
    if (window_has_focus && free_cam_mode) {
        glutWarpPointer(window_width / 2, window_height / 2);
    }
}

void camera_pan(vec3* look, float factor) {
    // unique 2d perpendicular vector
    camera_pos.x -= look->z * factor;
    camera_pos.z += look->x * factor;
}

void camera_move_in(vec3* look, float factor) {
    camera_pos.x += look->x * factor;
    camera_pos.y += look->y * factor;
    camera_pos.z += look->z * factor;
}

void camera_y_translate(float factor) {
    camera_pos.y += factor;
}

void enter_free_cam_mode(void) {
    glutSetCursor(GLUT_CURSOR_NONE);
    glutWarpPointer(window_width / 2, window_height / 2);
    free_cam_mode = true;
}

void leave_free_cam_mode(void) {
    glutSetCursor(GLUT_CURSOR_INHERIT);
    free_cam_mode = false;
}

void handle_key(unsigned char key, int x, int y) {
    // ctrl+q sends 17
    if (key == 17 && glutGetModifiers() == GLUT_ACTIVE_CTRL) {
        return glutLeaveMainLoop();
    } else if (key == 27) {
        leave_free_cam_mode();
    }
    input_to_active_field(key);
    key_buf[key] = true;
}

void handle_key_release(unsigned char key, int x, int y) {
    key_buf[key] = false;
}

void set_arrow_key_state(const int key, const bool set_to) {
    switch (key) {
        case GLUT_KEY_LEFT:
            arrow_key.left = set_to;
            break;
        case GLUT_KEY_RIGHT:
            arrow_key.right = set_to;
            break;
        case GLUT_KEY_UP:
            arrow_key.up = set_to;
            break;
        case GLUT_KEY_DOWN:
            arrow_key.down = set_to;
            break;
    }
}

void handle_special(int key, int x, int y) {
    set_arrow_key_state(key, true);
}

void handle_special_release(int key, int x, int y) {
    set_arrow_key_state(key, false);
}

void mouse_movement(int x, int y) {
    if (free_cam_mode) {
        const int x_half = x / 2;
        const int y_half = y / 2;
        float dist_from_center = sqrt(x_half * x_half + y_half * y_half);
        if (dist_from_center > 0) {
            const int center_x = window_width / 2;
            const int center_y = window_height / 2;
            y_rot_angle += (float)(x - center_x) / screen_width * 365;
            x_rot_angle += (float)(y - center_y) / screen_height * 365;
        }
    }
}

bool activate_clicked_text_box(int x, int y) {
    active_field = NULL;
    for (int i = 0; i < NUM_FILEDS; ++i) {
        const text_box_t box = fields[i];
        fields[i].active = x >= box.x && x <= box.x + box.width &&
                           y >= box.y && y <= box.y + box.height;
        if (fields[i].active)
            active_field = fields + i;
    }
    return active_field != NULL;
}

void input_to_active_field(unsigned char key) {
    if (active_field != NULL)
        text_box_input(active_field, key);
}

void mouse_click(int button, int state, int x, int y) {
    if (button != GLUT_LEFT_BUTTON || state != GLUT_DOWN) return;

    if (!activate_clicked_text_box(x, y))
        enter_free_cam_mode();
}

void handle_mouse_entry(int state) {
    window_has_focus = state == GLUT_ENTERED;
}

void handle_window_resize(int w, int h) {
    pers_matrix[0] = fFrustumScale / (w / (float)h);
    pers_matrix[5] = fFrustumScale;

    struct { float w, h; } dimentions = {w, h};
    // we only have one GL_UNIFORM_BUFFER
    glBufferSubData(GL_UNIFORM_BUFFER, 0, sizeof(dimentions), &dimentions);

    glUseProgram(shader_program);
    glUniformMatrix4fv(pers_matrix_uni, 1, GL_FALSE, pers_matrix);
    glUseProgram(0);
    glViewport(0, 0, w, h);
    window_width = w;
    window_height = h;
}

GLuint compile_shader(GLenum shader_type, const char* shader_source) {
    GLuint shader = glCreateShader(shader_type);
    glShaderSource(shader, 1, &shader_source, NULL);

    glCompileShader(shader);

    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (status == GL_FALSE) {
        GLint info_length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &info_length);

        GLchar* info = malloc(sizeof(GLchar) * (info_length + 1));
        glGetShaderInfoLog(shader, info_length, NULL, info);

        fprintf(stderr, "Shader compilation failure :\n%s\n", info);
        free(info);
    }

    return shader;
}

GLuint make_shader_program(int n, ...) {
    va_list vl, vl_detach;
    size_t i;
    GLuint program = glCreateProgram();

    va_start(vl, n);
    va_copy(vl_detach, vl);

    for (i = 0; i < n; i++)
        glAttachShader(program, va_arg(vl, GLuint));

    glLinkProgram(program);

    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (status == GL_FALSE) {
        GLint info_length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &info_length);

        GLchar* info = malloc(sizeof(GLchar) * (info_length + 1));
        glGetProgramInfoLog(program, info_length, NULL, info);
        fprintf(stderr, "Linker failure: %s\n", info);
        free(info);
    }

    for (i = 0; i < n; i++)
        glDetachShader(program, va_arg(vl_detach, GLuint));

    check_errors("make_shader_program");

    va_end(vl);
    va_end(vl_detach);

    return program;
}

// TODO: embed the mesh into the binary
char* read_obj(const char* file_name, size_t* buf_len) {
    int fd = open(file_name, O_RDONLY);
    struct stat sd;

    fstat(fd, &sd);
    *buf_len = sd.st_size + 1;
    char* buf = malloc(sd.st_size + 1);
    read(fd, buf, sd.st_size);
    buf[sd.st_size] = 0;
    close(fd);
    return buf;
}

int main(int argc, char **argv) {
    glutInitContextVersion(3, 3);
    glewExperimental = GL_TRUE;
    glutInit(&argc, argv);
    glutInitWindowSize(320, 320);
    glutCreateWindow("Fantasy Golf Simulator");
    screen_width = glutGet(GLUT_SCREEN_WIDTH);
    screen_height = glutGet(GLUT_SCREEN_HEIGHT);
    glutPassiveMotionFunc(mouse_movement);
    glutMouseFunc(mouse_click);

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return 1;
    }
    glGetError();

    glutInitDisplayMode(GLUT_RGBA|GLUT_SINGLE|GLUT_MULTISAMPLE);
    glutDisplayFunc(draw);
    glutIdleFunc(idle);
    glutReshapeFunc(handle_window_resize);
    glutKeyboardFunc(handle_key);
    glutKeyboardUpFunc(handle_key_release);
    glutEntryFunc(handle_mouse_entry);
    glutSpecialFunc(handle_special);
    glutSpecialUpFunc(handle_special_release);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_shader);
    GLuint window_vs = compile_shader(GL_VERTEX_SHADER, window_space_vertex_shader);
    GLuint font_fs = compile_shader(GL_FRAGMENT_SHADER, font_fragment_shader);
    GLuint ui_fs = compile_shader(GL_FRAGMENT_SHADER, ui_frag_shader);
    shader_program = make_shader_program(2, vs, fs);
    ui_program = make_shader_program(2, window_vs, ui_fs);
    window_space_program = make_shader_program(2, window_vs, font_fs);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    pers_matrix_uni = glGetUniformLocation(shader_program, "pers_matrix");
    camera_trans_uni = glGetUniformLocation(shader_program, "camera_trans");
    model_to_world = glGetUniformLocation(shader_program, "model_to_world");
    force_color_uni = glGetUniformLocation(shader_program, "force_color");
    window_dimentions_uni = glGetUniformLocation(window_space_program,
                                                 "window_dimentions");

    pers_matrix[0] = fFrustumScale;
    pers_matrix[5] = fFrustumScale;
    pers_matrix[10] = (fzFar + fzNear) / (fzNear - fzFar);
    pers_matrix[14] = (2 * fzFar * fzNear) / (fzNear - fzFar);
    pers_matrix[11] = -1.0f;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glUseProgram(shader_program);
    glUniformMatrix4fv(pers_matrix_uni, 1, GL_FALSE, pers_matrix);
    glUseProgram(0);

    size_t filesize;
    char* buf = read_obj("golf_ball.obj", &filesize);

    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes;
    tinyobj_material_t* materials;
    size_t nshape, nmat;
    tinyobj_parse_obj(&attrib, &shapes, &nshape, &materials, &nmat,
                      buf, filesize, TINYOBJ_FLAG_TRIANGULATE);

    glGenBuffers(1, &mesh_buffer);
    golf_mesh_vert_count = attrib.num_face_num_verts * 3 * 3;
    const size_t mesh_size = attrib.num_face_num_verts * sizeof(float) * 3 * 3;
    float* mesh = malloc(mesh_size);

    size_t mesh_offset = 0;
    size_t face_offset = 0;
    for (size_t i = 0; i < attrib.num_face_num_verts; i++) {
        assert(attrib.face_num_verts[i] % 3 == 0); // all triangular faces
        for (size_t f = 0; f < (size_t)attrib.face_num_verts[i]; f+=3) {
            tinyobj_vertex_index_t idx0 = attrib.faces[face_offset + 3 * f + 0];
            tinyobj_vertex_index_t idx1 = attrib.faces[face_offset + 3 * f + 1];
            tinyobj_vertex_index_t idx2 = attrib.faces[face_offset + 3 * f + 2];

            for (int k = 0; k < 3; k++) {
                int f0 = idx0.v_idx;
                int f1 = idx1.v_idx;
                int f2 = idx2.v_idx;

                mesh[mesh_offset + 0 + k] = attrib.vertices[3 * (size_t)f0 + k];
                mesh[mesh_offset + 3 + k] = attrib.vertices[3 * (size_t)f1 + k];
                mesh[mesh_offset + 6 + k] = attrib.vertices[3 * (size_t)f2 + k];
            }

            mesh_offset += 9;
        }
        face_offset += (size_t)attrib.face_num_verts[i];
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, nshape);
    tinyobj_materials_free(materials, nmat);

    // for (int i = 0; i < attrib.num_face_num_verts; i++) {
    //     for (int j = 0; j < 3; j++) {
    //         printf("x=%f y=%f z%f\n", mesh[i * 9 + j * 3 + 0], mesh[i * 9 + j * 3 + 1], mesh[i * 9 + j * 3 + 2]);
    //     }
    // }

    // printf("%d\n", golf_mesh_vert_count);

    glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer);
    glBufferData(GL_ARRAY_BUFFER, mesh_size, mesh, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(mesh);

    // make the grid
    glGenBuffers(1, &grid_buffer);
    size_t cur = 0;
    vec3 gird_points[GRID_LENGTH * 2 / 20 * 4];
    int i;
    for (i = -GRID_LENGTH; i < GRID_LENGTH; i+=20) {
        gird_points[cur++] = (vec3){GRID_LENGTH, 0, i};
        gird_points[cur++] = (vec3){-GRID_LENGTH, 0, i};
        gird_points[cur++] = (vec3){i, 0, GRID_LENGTH};
        gird_points[cur++] = (vec3){i, 0, -GRID_LENGTH};
    }
    glBindBuffer(GL_ARRAY_BUFFER, grid_buffer);
    glBufferData(GL_ARRAY_BUFFER, sizeof(gird_points), gird_points,
                 GL_STATIC_DRAW);

    glGenBuffers(1, &trail_buffer);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glGenBuffers(1, &text_vbo);

    font_init();  // binds a GL_TEXTURE_2D

    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glBufferData(GL_ARRAY_BUFFER, 9000 * 3 * sizeof(float), NULL, GL_DYNAMIC_DRAW);

    glGenBuffers(1, &ui_vert_buf);
    glGenBuffers(1, &ui_idx_buf);
    glBindBuffer(GL_ARRAY_BUFFER, ui_vert_buf);
    glBufferData(GL_ARRAY_BUFFER, 300 * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ui_idx_buf);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, 200 * sizeof(unsigned short),
                 NULL, GL_DYNAMIC_DRAW);

    GLuint ubo;  // for window dimentions
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 2, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    glEnable(GL_PRIMITIVE_RESTART);
    glPrimitiveRestartIndex(GOLF_RESTART_IDX);

    check_errors("init");

 //   glutFullScreen();
    glutMainLoop();
    return 0;
}