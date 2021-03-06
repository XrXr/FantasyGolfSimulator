#include <GL/glew.h>
#include <GL/freeglut.h>
#define TINYOBJ_LOADER_C_IMPLEMENTATION
#include "tinyobj_loader_c.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>
#include <stdbool.h>

#include "common.h"
#include "clock.h"
#include "font.h"
#include "text_box.h"

// shaders
#include "build/mesh_vs.h"
#include "build/mesh_fs.h"
#include "build/window_space_vs.h"
#include "build/font_fs.h"
#include "build/ui_fs.h"
#include "build/grid_vs.h"
#include "build/grid_gs.h"
#include "build/grid_fs.h"

// asset inserts
#include "build/bitmap_font_meta.h"
#include "build/golf_ball_mesh.h"
#include "build/wind_arrow_mesh.h"

GLuint shader_program;
GLuint ui_program;
GLuint window_space_program;
GLuint grid_program;
GLuint golf_ball_buf;
GLuint wind_arrow_buf;
GLuint grid_buffer;
GLuint model_to_world;
GLuint trail_buffer;
GLuint text_vbo;
GLuint ui_vert_buf;
GLuint ui_idx_buf;
GLuint window_dimentions_uni;
GLuint post_pers_trans_uni;
GLuint force_color_uni;
GLuint camera_trans_uni;
GLuint pers_matrix_uni;
GLuint grid_num_hori_uni;
GLuint surface_color_uni;

void camera_pan(vec3* look, float factor);
void camera_move_in(vec3* look, float factor);
void camera_y_translate(float factor);
void input_to_active_field(unsigned char key);

const float fFrustumScale = 2.0f, fzNear = 0.025f, fzFar = 100000.0f;
const float CAMERA_SPEED = 200.0f;
const int GRID_LENGTH = 1000;
const size_t TRAIL_BUF_SIZE = sizeof(float) * 3 * 500000;
vec3 camera_pos = {0, 3, 18};
bool key_buf[256];
struct { bool up, down, left, right; } arrow_key;
float y_rot_angle = 0;
float x_rot_angle = 0;
uint64_t last_sim_stamp = 0;
uint64_t last_mouse_warp = 0;
float pers_matrix[16];
size_t golf_mesh_vert_count, wind_arrow_vert_count;
int screen_width;
int screen_height;
int window_width;
int window_height;
bool window_has_focus = false;
bool free_cam_mode = false;
float y_launch_speed = 150;
float z_launch_speed = 600;
const size_t NUM_FILEDS = 2;
const size_t ANGLE_FIELD = 0;
const size_t POWER_FIELD = 1;
const size_t WIND_ANGLE_FIELD = 2;
const size_t WIND_SPEED_FIELD = 3;
#define NUM_FILEDS 4
text_box_t fields[] = {
    {375, 50, 150, 175, false, 10, "25", text_box_positive_nums_only},
    {375, 50, 150, 260, false, 10, "500", text_box_positive_nums_only},
    // x for these calculated based on screen size
    {375, 50, 0, 175, false, 10, "0", text_box_positive_nums_only},
    {375, 50, 0, 260, false, 10, "1", text_box_positive_nums_only},
};
text_box_t* active_field = NULL;
float flight_time = 0;
bool flying = false;
bool space_released_once = true;
vec3 golf_ball_pos = {0};
const float TRAIL_SAMPLE_FREQ = 0.002f;
size_t trail_buf_offset = sizeof(vec3);
float last_trail_sample_time;
bool blinker_present = false;
float wind_angle = 0;
float hwi = 0;
float vwi = 0;
float wind_speed = 1;
int mouse_last_x;
int mouse_last_y;

vec3 normalize3(const vec3 v) {
    const float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    vec3 ret = {
        v.x / len,
        v.y / len,
        v.z / len
    };
    return ret;
}

vec3 calc_lookat(const float y_rotate, const float x_rotate) {
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

// Euler's angle rotation matrix. Arguments in degrees
mat4 rotate(float around_x, float around_y, float around_z) {
    const float x_rad = deg_to_rad(around_x);
    const float cx = cos(x_rad);
    const float sx = sin(x_rad);
    const float y_rad = deg_to_rad(around_y);
    const float cy = cos(y_rad);
    const float sy = sin(y_rad);
    const float z_rad = deg_to_rad(around_z);
    const float cz = cos(z_rad);
    const float sz = sin(z_rad);

    float y_rotate[16] = {
        [0] = cy,
        [2] = -sy,
        [5] = 1,
        [8] = sy,
        [10] = cy,
        [15] = 1
    };

    float x_rotate[16] = {
        [0] = 1,
        [5] = cx,
        [6] = sx,
        [9] = -sx,
        [10] = cx,
        [15] = 1
    };

    float z_rotate[16] = {
        [0] = cz,
        [1] = sz,
        [4] = -sz,
        [5] = cz,
        [10] = 1,
        [15] = 1,
    };

    float (*(mult_ins)[3])[16] = {&z_rotate, &x_rotate, &y_rotate};
    mat4 trans;
    matrix_mul(mult_ins, 3, &trans.value);
    return trans;
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

float flight_x(const float t) {
    return hwi * t * 30;
}

float flight_y(const float t) {
    return (y_launch_speed * t) - (3.26667 * t * t * t);
}

float flight_z(const float t) {
    return -((z_launch_speed * t) - (vwi * (t*t*t) / 3));
}

void flight_init(void) {
    float power, angle;
    int ret = sscanf(fields[ANGLE_FIELD].content, "%f", &angle);
    assert(ret == 1);
    ret = sscanf(fields[POWER_FIELD].content, "%f", &power);
    assert(ret == 1);

    const float rad_angle = deg_to_rad(angle);
    z_launch_speed = power * cos(rad_angle);
    y_launch_speed = power * sin(rad_angle);

    const float rad_wind_angle = deg_to_rad(wind_angle);
    hwi = -sin(rad_wind_angle) * wind_speed;
    vwi = -cos(rad_wind_angle) * wind_speed;

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
    if (!ret) {
        printf("Failed to get elapsed time\n");
        glutLeaveMainLoop();
        return;
    }
    const uint64_t since_last = t - last_sim_stamp;
    const float seconds_since_last = since_last / 1000000000.0f;
    const float step = CAMERA_SPEED * seconds_since_last;
    last_sim_stamp = t;

    blinker_present = ((t / 1000000000) % 2) == 0;

    float wind_arrow_y_rot = 0;
    if (wind_speed != 0) {
        size_t r_time = 10000000000 / wind_speed;
        // wind_arrow_y_rot = (float)(t % r_time) / (r_time - 1) * 365;
        wind_arrow_y_rot = (float)(t % r_time) / (r_time - 1) * 360.0f;
    }

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

        vec3 look = calc_lookat(y_rot_angle, x_rot_angle);
        camera_pan(&look, pan_dir * step);
        camera_move_in(&look, in_out_dir * step);
        camera_y_translate(y_translate_dir * step);
    }

    const bool old_flying = flying;

    if (key_buf[' '] && free_cam_mode) {
        if (flying) {
            flying = !space_released_once;
        } else {
            flying = space_released_once;
        }
        space_released_once = false;
    } else {
        space_released_once = true;
    }

    const bool paused_this_frame = old_flying && !flying;

    if (flying) {
        if (golf_ball_pos.y <= 0) {
            flight_init();
        }
        flight_time += seconds_since_last;

        golf_ball_pos.x = flight_x(flight_time);
        golf_ball_pos.y = flight_y(flight_time);
        golf_ball_pos.z = flight_z(flight_time);
    }

    if (flying && golf_ball_pos.y < 0.0f) {
        printf("Final z %f. Flight time %f \n", golf_ball_pos.z, flight_time);
        flying = false;
    }

    const bool display_ui = !free_cam_mode || !flying;
    // sim end

    const float id[16] = {[0]=1, [5]=1, [10]=1, [15]=1};
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glUseProgram(shader_program);
    glUniformMatrix4fv(post_pers_trans_uni, 1, GL_FALSE, id);

    float camera_trans[16];
    {  // compute and send the camera transform matrix
        float c_translate[16] = {0};
        c_translate[0] = 1;
        c_translate[5] = 1;
        c_translate[10] = 1;
        c_translate[15] = 1;

        c_translate[12] = -camera_pos.x;
        c_translate[13] = -camera_pos.y;
        c_translate[14] = -camera_pos.z;

        mat4 c_rotate = rotate(x_rot_angle, y_rot_angle, 0);

        float (*(mult_buf)[2])[16] = {&c_rotate.value, &c_translate};
        matrix_mul(mult_buf, 2, &camera_trans);
        glUniformMatrix4fv(camera_trans_uni, 1, GL_FALSE, camera_trans);
    }

    {
        float mesh_model_trans[16] = {[0]=.2f, [5]=.2f, [10]=.2f, [15]=1};
        mesh_model_trans[12] = golf_ball_pos.x;
        mesh_model_trans[13] = golf_ball_pos.y;
        mesh_model_trans[14] = golf_ball_pos.z;
        glUniformMatrix4fv(model_to_world, 1, GL_FALSE, mesh_model_trans);
    }

    glBindBuffer(GL_ARRAY_BUFFER, golf_ball_buf);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float), 0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(float),
                          (const void*) (sizeof(vec3)));
    glUniform3f(surface_color_uni, 1, 1, 1);
    glDrawArrays(GL_TRIANGLES, 0, golf_mesh_vert_count);
    check_errors("draw golf ball");

    if (golf_ball_pos.z != 0) {  // draw the golf ball's trail
        glBindBuffer(GL_ARRAY_BUFFER, trail_buffer);

        const float dt = flight_time - last_trail_sample_time;
        const size_t n_verts = floor(dt / TRAIL_SAMPLE_FREQ) + paused_this_frame;
        const size_t upload_size = n_verts * sizeof(vec3);
        const bool enough_space = trail_buf_offset + upload_size <= TRAIL_BUF_SIZE;
        if (old_flying && n_verts > 0 && enough_space) {
            vec3* points = malloc(upload_size);
            size_t i = 0;
            float t = last_trail_sample_time + TRAIL_SAMPLE_FREQ;
            for (; t <= flight_time && i < n_verts; t += TRAIL_SAMPLE_FREQ) {
                points[i++] = (vec3){flight_x(t), flight_y(t), flight_z(t)};
            }
            last_trail_sample_time = t - TRAIL_SAMPLE_FREQ;

            if (paused_this_frame) {
                last_trail_sample_time = flight_time;
                points[i++] = (vec3){
                    flight_x(flight_time),
                    flight_y(flight_time),
                    flight_z(flight_time)
                };
            }

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
        glUniformMatrix4fv(model_to_world, 1, GL_FALSE, id);
        glUniform4f(force_color_uni, 1, 1, 1, 1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        assert(trail_buf_offset % (sizeof(float) * 3) == 0);
        glDrawArrays(GL_LINE_STRIP, 0, trail_buf_offset / (sizeof(float) * 3));
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        check_errors("draw trail");
    }

    {  // wind arrow
        float translate[16] = {
            [0]=1,
            [5]=1,
            [10]=1,
            [14]=-15,
            [15]=1
        };
        mat4 rot = rotate(0, wind_arrow_y_rot, wind_angle);

        float (*(mult_ins)[2])[16] = {&translate, &rot.value};
        float mesh_model_trans[16];
        matrix_mul(mult_ins, 2, &mesh_model_trans);
        glUniformMatrix4fv(model_to_world, 1, GL_FALSE, mesh_model_trans);
        float post_trans[16] = {
            [0]=1,
            [5]=1,
            [10]=1,
            [12]=.8,
            [13]=-.65,
            [15]=1
        };
        glUniformMatrix4fv(post_pers_trans_uni, 1, GL_FALSE, post_trans);
    }
    if (display_ui) {
        glBindBuffer(GL_ARRAY_BUFFER, wind_arrow_buf);
        glEnableVertexAttribArray(0);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)0);
        glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 6 * sizeof(GLfloat), (GLvoid*)(3 * sizeof(GLfloat)));
        glUniform4f(force_color_uni, 0, 0, 0, 0);
        glUniformMatrix4fv(camera_trans_uni, 1, GL_FALSE, id);
        glUniform3f(surface_color_uni, .8, .337, .323);
        glDrawArrays(GL_TRIANGLES, 0, wind_arrow_vert_count);
        check_errors("draw wind arrow");
    }

    {
        glUseProgram(grid_program);
        glUniformMatrix4fv(glGetUniformLocation(grid_program, "camera_trans"),
                           1, GL_FALSE, camera_trans);
        glUniformMatrix4fv(glGetUniformLocation(grid_program, "pers_matrix"),
                           1, GL_FALSE, pers_matrix);

        const float around_camera[] = {
            camera_pos.x, camera_pos.y, camera_pos.z,
        };
        glBindBuffer(GL_ARRAY_BUFFER, trail_buffer);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
        glBufferSubData(GL_ARRAY_BUFFER, trail_buf_offset,
                        sizeof(around_camera), around_camera);
        glDisable(GL_BLEND);
        glDrawArrays(GL_POINTS, 0, trail_buf_offset / sizeof(vec3) + 1);
        glEnable(GL_BLEND);
        check_errors("draw grid");
    }

    glUseProgram(window_space_program);
    glBindBuffer(GL_ARRAY_BUFFER, text_vbo);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    const int stride = sizeof(float) * 2 * 2;
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, 0);
    // texture coord
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, stride,
                          (const void *) (sizeof(float) * 2));

    char flight_time_str[100];
    sprintf(flight_time_str, "t=%.4f", flight_time);
    size_t font_vert_buf_offset = 0;
    render_string(flight_time_str, 0, 0, &font_vert_buf_offset);

    if (display_ui) {
        for (int i = 0; i < NUM_FILEDS; i++)
            text_box_render_font(fields + i, &font_vert_buf_offset);
        render_string("Angle", 20, 180, &font_vert_buf_offset);
        render_string("Power", 20, 265, &font_vert_buf_offset);

        const float window_right = window_width -
            GOLF_CHAR_WIDTH * 10 - fields[0].width - 60;
        render_string("Wind Angle", window_right, 180, &font_vert_buf_offset);
        render_string("Wind Speed", window_right, 265, &font_vert_buf_offset);
    }

    // two for pos and two for texture coord
    glDrawArrays(GL_TRIANGLES, 0, font_vert_buf_offset / sizeof(float) / 4);
    check_errors("draw text");

    if (display_ui) {
        const int window_right = window_width - fields[0].width - 30;
        fields[WIND_ANGLE_FIELD].x = fields[WIND_SPEED_FIELD].x = window_right;

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
                                    num_chars * GOLF_CHAR_WIDTH;
            const float verts[] = {
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
        glDrawElements(GL_LINE_STRIP, idx_offset / sizeof(unsigned short),
                       GL_UNSIGNED_SHORT, 0);
        check_errors("draw text box frames");
    }

    glutSwapBuffers();
    glutPostRedisplay();
}

void idle(void) {
    uint64_t t;
    bool ret = elapsed_ns(&t);
    if (!ret) return;

    const uint64_t WARP_INTERVAL = 150000000ull;

    if (window_has_focus && free_cam_mode && (t - last_mouse_warp) > WARP_INTERVAL) {
        last_mouse_warp = t;
        mouse_last_x = window_width / 2;
        mouse_last_y = window_height / 2;
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
        glutLeaveMainLoop();
        return;
    } else if (key == 27) {
        leave_free_cam_mode();
        if (active_field) {
            active_field->active = false;
            active_field = NULL;
        }
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

float clamp(float x, float lower, float higher) {
    if (x < lower) return lower;
    if (x > higher) return higher;
    return x;
}

void mouse_movement(int x, int y) {
    if (free_cam_mode) {
        y_rot_angle += (float)(x - mouse_last_x) / screen_width * 360;
        const float propose_x = x_rot_angle +
            (float)(y - mouse_last_y) / screen_height * 360;
        x_rot_angle = clamp(propose_x, -90, 90);

        mouse_last_x = x;
        mouse_last_y = y;
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
    if (active_field == NULL) return;

    text_box_input(active_field, key);

    if (fields[WIND_ANGLE_FIELD].active) {
        sscanf(fields[WIND_ANGLE_FIELD].content, "%f", &wind_angle);
    } else if (fields[WIND_SPEED_FIELD].active) {
        sscanf(fields[WIND_SPEED_FIELD].content, "%f", &wind_speed);
    }
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
    const GLint len[1] = {strlen(shader_source)};
    glShaderSource(shader, 1, &shader_source, len);

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
    int i;
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

GLuint read_mesh(const char* obj_text, size_t* vert_count_out) {
    GLuint gl_buf;
    glGenBuffers(1, &gl_buf);
    size_t len = strlen(obj_text) + 1;

    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes;
    tinyobj_material_t* materials;
    size_t nshape, nmat;
    tinyobj_parse_obj(&attrib, &shapes, &nshape, &materials, &nmat,
                      obj_text, len, TINYOBJ_FLAG_TRIANGULATE);

    *vert_count_out = attrib.num_face_num_verts * 3;
    const size_t mesh_size = *vert_count_out * sizeof(vec3) * 2;
    float* mesh = malloc(mesh_size);

    size_t mesh_offset = 0;
    for (size_t i = 0; i < attrib.num_face_num_verts; i++) {
        assert(attrib.face_num_verts[i] % 3 == 0); // all triangular faces
        tinyobj_vertex_index_t idx0 = attrib.faces[i * 3 + 0];
        tinyobj_vertex_index_t idx1 = attrib.faces[i * 3 + 1];
        tinyobj_vertex_index_t idx2 = attrib.faces[i * 3 + 2];

        int f0 = idx0.v_idx;
        int f1 = idx1.v_idx;
        int f2 = idx2.v_idx;

        int n0 = idx0.vn_idx;
        int n1 = idx1.vn_idx;
        int n2 = idx2.vn_idx;

        for (int k = 0; k < 3; k++) {
            mesh[mesh_offset + 0 + k] = attrib.vertices[3 * (size_t)f0 + k];
            mesh[mesh_offset + 3 + k] = attrib.normals[3 * (size_t)n0 + k];

            mesh[mesh_offset + 6 + k] = attrib.vertices[3 * (size_t)f1 + k];
            mesh[mesh_offset + 9 + k] = attrib.normals[3 * (size_t)n1 + k];

            mesh[mesh_offset + 12 + k] = attrib.vertices[3 * (size_t)f2 + k];
            mesh[mesh_offset + 15 + k] = attrib.normals[3 * (size_t)n2 + k];
        }

        mesh_offset += 18;
    }

    tinyobj_attrib_free(&attrib);
    tinyobj_shapes_free(shapes, nshape);
    tinyobj_materials_free(materials, nmat);

    glBindBuffer(GL_ARRAY_BUFFER, gl_buf);
    glBufferData(GL_ARRAY_BUFFER, mesh_size, mesh, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    free(mesh);
    return gl_buf;
}

int main(int argc, char **argv) {
    glutInitContextVersion(3, 3);
    glewExperimental = GL_TRUE;

    glutInit(&argc, argv);
    screen_width = glutGet(GLUT_SCREEN_WIDTH);
    screen_height = glutGet(GLUT_SCREEN_HEIGHT);
    glutInitWindowSize(1366, 768);
    glutInitWindowPosition(0, 0);
    glutCreateWindow("Fantasy Golf Simulator");
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

    GLuint vs = compile_shader(GL_VERTEX_SHADER, MESH_VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, MESH_FS);
    GLuint window_vs = compile_shader(GL_VERTEX_SHADER, WINDOW_SPACE_VS);
    GLuint font_fs = compile_shader(GL_FRAGMENT_SHADER, FONT_FS);
    GLuint ui_fs = compile_shader(GL_FRAGMENT_SHADER, UI_FS);

    GLuint grid_vs = compile_shader(GL_VERTEX_SHADER, GRID_VS);
    GLuint grid_gs = compile_shader(GL_GEOMETRY_SHADER, GRID_GS);
    GLuint grid_fs = compile_shader(GL_FRAGMENT_SHADER, GRID_FS);

    shader_program = make_shader_program(2, vs, fs);
    ui_program = make_shader_program(2, window_vs, ui_fs);
    window_space_program = make_shader_program(2, window_vs, font_fs);
    grid_program = make_shader_program(3, grid_gs, grid_vs, grid_fs);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    pers_matrix_uni = glGetUniformLocation(shader_program, "pers_matrix");
    camera_trans_uni = glGetUniformLocation(shader_program, "camera_trans");
    model_to_world = glGetUniformLocation(shader_program, "model_to_world");
    force_color_uni = glGetUniformLocation(shader_program, "force_color");
    post_pers_trans_uni = glGetUniformLocation(shader_program, "post_pers_trans");
    surface_color_uni = glGetUniformLocation(shader_program, "surface_color");
    window_dimentions_uni = glGetUniformLocation(window_space_program,
                                                 "window_dimentions");
    grid_num_hori_uni = glGetUniformLocation(grid_program, "num_hori_line");

    pers_matrix[0] = fFrustumScale;
    pers_matrix[5] = fFrustumScale;
    pers_matrix[10] = (fzFar + fzNear) / (fzNear - fzFar);
    pers_matrix[14] = (2 * fzFar * fzNear) / (fzNear - fzFar);
    pers_matrix[11] = -1.0f;

    glUseProgram(shader_program);
    glUniformMatrix4fv(pers_matrix_uni, 1, GL_FALSE, pers_matrix);
    glUseProgram(0);

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

    glBindBuffer(GL_ARRAY_BUFFER, trail_buffer);
    glBufferData(GL_ARRAY_BUFFER, TRAIL_BUF_SIZE, NULL, GL_DYNAMIC_DRAW);

    GLuint ubo;  // for window dimentions
    glGenBuffers(1, &ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(float) * 2, NULL, GL_DYNAMIC_DRAW);
    glBindBufferBase(GL_UNIFORM_BUFFER, 0, ubo);

    golf_ball_buf = read_mesh(GOLF_BALL_MESH, &golf_mesh_vert_count);
    wind_arrow_buf = read_mesh(WIND_ARROW_MESH, &wind_arrow_vert_count);

    glEnable(GL_PRIMITIVE_RESTART);
    glEnable(GL_DEPTH_TEST);
    glPrimitiveRestartIndex(GOLF_RESTART_IDX);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);


    #ifdef _WIN32
        HWND win_handle = FindWindow(0, "Fantasy Golf Simulator");
        if (!win_handle) {
            printf("!!! Failed FindWindow\n");
            return -1;
        }
        ShowWindowAsync(win_handle, SW_SHOWMAXIMIZED);
    #endif

    check_errors("init");

    glutMainLoop();
    return 0;
}
