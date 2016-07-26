#include <GL/glew.h>
#include <GL/freeglut.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

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

"layout(location = 0) in vec4 position;"
"layout(location = 1) in vec4 in_color;"
"flat out vec4 vertex_color;"

"uniform vec2 offset;"
"uniform mat4 perspectiveMatrix;"
"uniform mat4 world_trans;" // camera magic happens here
"uniform mat4 rotate_trans;"
"uniform mat4 x_rotate_trans;"
"uniform float zoom;"

"void main()"
"{"
"    vec4 pos = world_trans * position;"
"    pos = rotate_trans * pos;"
"    pos = x_rotate_trans * pos;"
"    gl_Position = perspectiveMatrix * pos;"
"    vertex_color = in_color;"
"}";

const char* frag_shader =
"#version 330\n"

"out vec4 output_color;"
"flat in vec4 vertex_color;"
"void main()"
"{"
"   output_color = vertex_color;"
"}";

GLuint shader_program;
GLuint pos_buffer_obj;
GLuint zoom_uniform;
GLuint offsetUniform;
GLuint rotateTransUni;
GLuint xRotateTransUni;
GLuint worldTransUni;

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

float zoom = 1.0f;
vec3 translate_offset = {0, 0, 1.0f};
float y_rot_angle = 0;
float x_rot_angle = 0;

vec3 normalize3(const vec3 v) {
    const float len = sqrt(v.x * v.x + v.y * v.y + v.z * v.z);
    vec3 ret = {
        v.x / len,
        v.y / len,
        v.z / len
    };
    return ret;
};

vec3 calc_lookat(const vec3 eye, const float rot_deg) {
    const float rot_rad = rot_deg * (M_PI / 180);
    const float cos_a = cos(rot_rad);
    const float sin_a = sin(rot_rad);
    const vec3 result = {
        sin_a,
        0,
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
    for (int row = 0; row < 4; row++) {
        float* a_base = (*a) + row;
        for (int col = 0; col < 4; col++) {
            float* b_base = (*b) + 4 * col;
            for (int i = 0; i < 4; i++) {
                (*out)[row * 4 + col] += a_base[i * 4] + b_base[i];
            }
        }
    }
}

void matrix_mul(float (**in)[16], size_t size, float (*out)[16]) {
    assert(size >= 2);

    float cur[16];
    float tmp[16];
    matrix_mul_2(in[0], in[1], &cur);
    for (size_t i = 0; i < size; i++) {
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

void draw(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader_program);
    glUniform1f(zoom_uniform, zoom);

    float worldMat[16] = {0};
    worldMat[0] = 1;
    worldMat[5] = 1;
    worldMat[10] = 1;
    worldMat[15] = 1;

    worldMat[12] = -translate_offset.x;
    worldMat[13] = -translate_offset.y;
    worldMat[14] = -translate_offset.z;
    glUniformMatrix4fv(worldTransUni, 1, GL_FALSE, worldMat);

    float transformMat[16] = {0};
    float angle = y_rot_angle * (M_PI / 180);
    transformMat[0] = cos(angle);
    transformMat[2] = -sin(angle);
    transformMat[5] = 1;
    transformMat[8] = sin(angle);
    transformMat[10] = cos(angle);
    transformMat[15] = 1;
    glUniformMatrix4fv(rotateTransUni, 1, GL_FALSE, transformMat);

    // recover the lookat vector
    vec3 lookat = {0, 0, -1};
    //print_vec3(lookat);
    vec3 up = {0, 1, 0};
    vec3 up_down_axis = cross(lookat, up);

    print_vec3(up_down_axis);
    // rotate around that axis
    //angle = x_rot_angle * (M_PI / 180);
    //printf("xrotangle=%f\n", x_rot_angle);
    mat4 x_rot_mat = rotate(x_rot_angle, normalize3(up_down_axis));
    glUniformMatrix4fv(xRotateTransUni, 1, GL_FALSE, x_rot_mat.value);

    glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_obj);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    // the color. 64 = sizeof(float) * 4 dimension * 8 of them
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, 0, (void*)(sizeof(g_vertex_buffer_data) / 2));

    glDrawArrays(GL_TRIANGLE_FAN, 0, 8);

    glDisableVertexAttribArray(0);
    glDisableVertexAttribArray(1);
    glUseProgram(0);

    glutSwapBuffers();
    glutPostRedisplay();
}

void handle_key(unsigned char key, int x, int y) {
    // ctrl+q sends 17
    if (key == 17 && glutGetModifiers() == GLUT_ACTIVE_CTRL) {
        return glutLeaveMainLoop();
    }
    switch (key) {
        case 'e':
            y_rot_angle += 1.0f;
            break;
        case 'r':
            y_rot_angle -= 1.0f;
            break;
    }
}

void handle_special(int key, int x, int y) {
    const float step = 0.025f;

    switch (key) {
        case GLUT_KEY_HOME:
            translate_offset.z -= step;
            // zoom += step;
            break;
        case GLUT_KEY_END:
            translate_offset.z += step;
            // zoom = fmax(0, zoom - step);
            break;
        case GLUT_KEY_LEFT:
            translate_offset.x -= step;
            break;
        case GLUT_KEY_RIGHT:
            translate_offset.x += step;
            break;
        case GLUT_KEY_UP:
            translate_offset.y += step;
            break;
        case GLUT_KEY_DOWN:
            translate_offset.y -= step;
            break;
    }
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
    //glutIdleFunc(draw);
    glutReshapeFunc(handle_window_size_change);
    glutKeyboardFunc(handle_key);
    glutEntryFunc(handle_mouse_entry);
    glutSpecialFunc(handle_special);

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

    GLuint perspectiveMatrixUnif;

    offsetUniform = glGetUniformLocation(shader_program, "offset");
    perspectiveMatrixUnif = glGetUniformLocation(shader_program, "perspectiveMatrix");
    rotateTransUni = glGetUniformLocation(shader_program, "rotate_trans");
    xRotateTransUni = glGetUniformLocation(shader_program, "x_rotate_trans");
    worldTransUni = glGetUniformLocation(shader_program, "world_trans");
    zoom_uniform = glGetUniformLocation(shader_program, "zoom");

    float fFrustumScale = 2.0f; float fzNear = 0.025f; float fzFar = 10.0f;

    float theMatrix[16];
    memset(theMatrix, 0, sizeof(float) * 16);

    theMatrix[0] = fFrustumScale;
    theMatrix[5] = fFrustumScale;
    theMatrix[10] = (fzFar + fzNear) / (fzNear - fzFar);
    theMatrix[14] = (2 * fzFar * fzNear) / (fzNear - fzFar);
    theMatrix[11] = -1.0f;

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CW);

    glUseProgram(shader_program);
    glUniform2f(offsetUniform, 0.0f, 0.0f);
    glUniformMatrix4fv(perspectiveMatrixUnif, 1, GL_FALSE, theMatrix);
    glUniform1f(zoom_uniform, 100.0f);
    glUseProgram(0);

 //   glutFullScreen();
    glutMainLoop();
    return 0;
}