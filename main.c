#include <GL/glew.h>
#include <GL/freeglut.h>
#include <stdio.h>
#include <string.h>
#define _USE_MATH_DEFINES
#include <math.h>

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
"uniform mat4 transform;"
"uniform float zoom;"

"void main()"
"{"
"    vec4 cameraPos = position;"
"    cameraPos = transform * cameraPos;"
"    cameraPos = cameraPos + vec4(offset.x, offset.y, 2, 0.0);"
"    cameraPos = cameraPos * vec4(zoom, zoom, -1, 1);"
"    gl_Position = perspectiveMatrix * cameraPos;"
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
GLuint transformMatUnif;
float zoom = 1.0f;
struct {
    float x;
    float y;
} translate_offset = {0, 0};
float rot_angle = 0;

void draw(void) {
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT);

    glUseProgram(shader_program);
    glUniform1f(zoom_uniform, zoom);
    glUniform2f(offsetUniform, translate_offset.x, translate_offset.y);
    float transformMat[16] = {0.0f};
    float angle = rot_angle * (M_PI / 180);
    transformMat[0] = cos(angle);
    transformMat[8] = sin(angle);

    transformMat[5] = 1;

    transformMat[2] = -sin(angle);
    transformMat[10] = cos(angle);

    transformMat[15] = 1;
    glUniformMatrix4fv(transformMatUnif, 1, GL_FALSE, transformMat);

    glBindBuffer(GL_ARRAY_BUFFER, pos_buffer_obj);

    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, 0);
    // // the color. 64 = sizeof(float) * 4 dimension * 8 of them
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
            rot_angle += 1.0f;
            break;
        case 'r':
            rot_angle -= 1.0f;
            break;
    }
}

void handle_special(int key, int x, int y) {
    const float step = 0.025f;

    switch (key) {
        case GLUT_KEY_HOME:
            zoom += step;
            break;
        case GLUT_KEY_END:
            zoom = fmax(0, zoom - step);
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

void changeSize(int w, int h) {
    glViewport(0, 0, w, h);
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

    GLenum err = glewInit();
    if (err != GLEW_OK) {
        fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
        return 1;
    }

    glutInitDisplayMode(GLUT_RGBA|GLUT_SINGLE|GLUT_MULTISAMPLE);
    glutDisplayFunc(draw);
    //glutIdleFunc(draw);
    glutReshapeFunc(changeSize);
    glutKeyboardFunc(handle_key);
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
    transformMatUnif = glGetUniformLocation(shader_program, "transform");
    zoom_uniform = glGetUniformLocation(shader_program, "zoom");

    float fFrustumScale = 1.0f; float fzNear = 0.5f; float fzFar = 3.0f;

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