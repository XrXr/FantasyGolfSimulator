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
#include "clock.h"

#define print_vec3(V)  printf(#V": x=%f y=%f z=%f\n", V.x, V.y, V.z)

const float g_vertex_buffer_data[] = {
    0.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 0.0f, -1.0f, 1.0f,
    -1.0f, 0.0f, -1.0f, 1.0f,
    -1.0f, 0.0f, 0.0f, 1.0f,
    -1.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 1.0f, -1.0f, 1.0f,
    0.0f, 0.0f, -1.0f, 1.0f,


    1.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    1.0f, 0.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 1.0f, 0.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f,
    0.0f, 0.0f, 1.0f, 1.0f,
};


const char* vertex_shader =
"#version 330\n"

"layout(location = 0) in vec3 position;"
//"layout(location = 1) in vec4 in_color;"
"out vec4 vertex_color;"

"uniform mat4 model_to_world;"
"uniform mat4 perspectiveMatrix;"
"uniform mat4 camera_trans;"

"void main()"
"{"
"    vec4 pos = camera_trans * model_to_world * vec4(position, 1);"
"    gl_Position = perspectiveMatrix * pos;"
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
"void main()"
"{"
"   output_color = vertex_color;"
"}";

GLuint shader_program;
GLuint pos_buffer_obj;
GLuint mesh_buffer;
GLuint offsetUniform;
GLuint rotateTransUni;
GLuint xRotateTransUni;
GLuint worldTransUni;
GLuint perspectiveMatrixUnif;
GLuint model_to_world;

typedef struct {
    float x;
    float y;
    float z;
} vec3;

typedef struct {
    float x;
    float y;
    float z;
    float w;
} vec4;

typedef struct {
    float value[16];
} mat4;

void camera_pan(vec3* look, float factor);
void camera_move_in(vec3* look, float factor);
void camera_y_translate(float factor);

vec3 camera_pos = {0, 0, 2.0f};
bool key_buf[256];
struct { bool up, down, left, right; } arrow_key;
float y_rot_angle = 0;
float x_rot_angle = 0;
uint64_t last_sim_stamp = 0;
float perspectiveMatrix[16];
const float fFrustumScale = 2.0f, fzNear = 0.025f, fzFar = 1000.0f;
const float CAMERA_STEP = 400.0f;
size_t golf_mesh_vert_count;

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

float dot3(const vec3 a, const vec3 b) {
    return a.x * b.x + a.y * b.y + a.z * b.z;
}
float flight_time = 0;
bool flying = false;
bool space_released_once = true;
vec3 golf_ball_pos = {0};
void draw(void) {
    // sim
    uint64_t t;
    int ret = elapsed_ns(&t);
    const uint64_t since_last = t - last_sim_stamp;
    const float step = CAMERA_STEP * (since_last / 1000000000.0f);
    last_sim_stamp = t;
    vec3 look = calc_lookat(camera_pos, y_rot_angle, x_rot_angle);

    int in_out_dir = 0;
    int pan_dir = 0;
    int y_translate_dir = 0;

    pan_dir += (key_buf['a'] || arrow_key.left) * -1;
    pan_dir += (key_buf['d'] || arrow_key.right);

    in_out_dir += (key_buf['s'] || arrow_key.down) * -1;
    in_out_dir += (key_buf['w'] || arrow_key.up);

    y_translate_dir += key_buf['q'] * -1;
    y_translate_dir += key_buf['e'];

    camera_pan(&look, pan_dir * step);
    camera_move_in(&look, in_out_dir * step);
    camera_y_translate(y_translate_dir * step);


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
        flight_time += since_last / 1000000000.0f;

        const float x = flight_time;

        golf_ball_pos.z = -((150 * x) - ((9 * x * x) / 2));
        golf_ball_pos.y = (150 * x) - (3.255557 * x * x * x);

        //printf("dist to ball %f\n", sqrt(golf_ball_pos.z * golf_ball_pos.z + golf_ball_pos.y * golf_ball_pos.y));
        //printf("flight time %f, y %f\n", flight_time, golf_ball_pos.y);

        if (golf_ball_pos.y <= 0) {
            printf("final z %f. flight time %f \n", golf_ball_pos.z, flight_time);
            golf_ball_pos.y = 0;
            flying = false;
            flight_time = 0;
        }
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
        glUniformMatrix4fv(worldTransUni, 1, GL_FALSE, trans);
    }

    {
        float model_world_trans[16] = {[0]=.2f, [5]=.2f, [10]=.2f, [15]=1};
        model_world_trans[12] = golf_ball_pos.x;
        model_world_trans[13] = golf_ball_pos.y;
        model_world_trans[14] = golf_ball_pos.z;
        glUniformMatrix4fv(model_to_world, 1, GL_FALSE, model_world_trans);
    }

    glBindBuffer(GL_ARRAY_BUFFER, mesh_buffer);

    glEnableVertexAttribArray(0);
    //glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, 0);
    // // the color. 64 = sizeof(float) * 4 dimension * 8 of them
    // glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)(sizeof(g_vertex_buffer_data) / 2));

    glDrawArrays(GL_TRIANGLES, 0, golf_mesh_vert_count);

    glBindBuffer(GL_ARRAY_BUFFER, 0);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);

    glutSwapBuffers();
    glutPostRedisplay();
}

int previous_time = 0;
void idle(void) {
    int current_time = glutGet(GLUT_ELAPSED_TIME);
    int frame_time = current_time - previous_time;
    if (frame_time == 0) return;
    previous_time = current_time;

    printf("%2.2f\r", 1000.0f / (float)(frame_time));
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

void handle_key(unsigned char key, int x, int y) {
    // ctrl+q sends 17
    if (key == 17 && glutGetModifiers() == GLUT_ACTIVE_CTRL) {
        return glutLeaveMainLoop();
    }
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


int lx = 0, ly = 0;
int screen_width;
int screen_height;
void mouse_movement(int x, int y) {
    if (lx != 0 || ly != 0) {
        y_rot_angle += (float)(x - lx) / (float)(screen_width) * 365;
        x_rot_angle += (float)(y - ly) / (float)(screen_height) * 365;
    }

    lx = x;
    ly = y;
}

void handle_mouse_entry(int state) {
    lx = ly = 0;
}

void handle_window_size_change(int w, int h) {
    perspectiveMatrix[0] = fFrustumScale / (w / (float)h);
    perspectiveMatrix[5] = fFrustumScale;

    glUseProgram(shader_program);
    glUniformMatrix4fv(perspectiveMatrixUnif, 1, GL_FALSE, perspectiveMatrix);
    glUseProgram(0);
    glViewport(0, 0, w, h);
    lx = ly = 0;
}

GLuint compile_shader(GLenum eShaderType, const char* shader_source) {
    GLuint shader = glCreateShader(eShaderType);
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
        glDetachShader(program, va_arg(vl, GLuint));

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

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    glutInitDisplayMode(GLUT_RGBA|GLUT_SINGLE|GLUT_MULTISAMPLE);
    glutDisplayFunc(draw);
    //glutIdleFunc(idle);
    glutReshapeFunc(handle_window_size_change);
    glutKeyboardFunc(handle_key);
    glutKeyboardUpFunc(handle_key_release);
    glutEntryFunc(handle_mouse_entry);
    glutSpecialFunc(handle_special);
    glutSpecialUpFunc(handle_special_release);

    GLuint vs = compile_shader(GL_VERTEX_SHADER, vertex_shader);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, frag_shader);
    shader_program = make_shader_program(2, vs, fs);

    glGenBuffers(1, &pos_buffer_obj);

    glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_obj);
    glBufferData(GL_ARRAY_BUFFER, sizeof(g_vertex_buffer_data),
                 g_vertex_buffer_data, GL_STATIC_DRAW);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    GLuint vao;
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    offsetUniform = glGetUniformLocation(shader_program, "offset");
    perspectiveMatrixUnif = glGetUniformLocation(shader_program, "perspectiveMatrix");
    rotateTransUni = glGetUniformLocation(shader_program, "rotate_trans");
    xRotateTransUni = glGetUniformLocation(shader_program, "x_rotate_trans");
    worldTransUni = glGetUniformLocation(shader_program, "camera_trans");
    model_to_world = glGetUniformLocation(shader_program, "model_to_world");

    perspectiveMatrix[0] = fFrustumScale;
    perspectiveMatrix[5] = fFrustumScale;
    perspectiveMatrix[10] = (fzFar + fzNear) / (fzNear - fzFar);
    perspectiveMatrix[14] = (2 * fzFar * fzNear) / (fzNear - fzFar);
    perspectiveMatrix[11] = -1.0f;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glUseProgram(shader_program);
    glUniform2f(offsetUniform, 0.0f, 0.0f);
    glUniformMatrix4fv(perspectiveMatrixUnif, 1, GL_FALSE, perspectiveMatrix);
    glUseProgram(0);

    size_t filesize;
    char* buf = read_obj("golf_ball.obj", &filesize);

    tinyobj_attrib_t attrib;
    tinyobj_shape_t* shapes;
    tinyobj_material_t* materials;
    size_t nshape, nmat;
    tinyobj_parse_obj(&attrib, &shapes, &nshape, &materials, &nmat, buf, filesize,
                      TINYOBJ_FLAG_TRIANGULATE);



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

 //   glutFullScreen();
    glutMainLoop();
    return 0;
}