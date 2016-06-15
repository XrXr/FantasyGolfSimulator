#include <GL/freeglut.h>
#include <stdio.h>

void draw(void) {
    glClearColor(0.0f,0.0f,0.0f,1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    glFlush();
}

void handle_key(unsigned char key, int x, int y) {
    // ctrl+q sends 17
    if (key == 17 && glutGetModifiers() == GLUT_ACTIVE_CTRL)
        glutLeaveMainLoop();
}

int main(int argc, char **argv) {
    glutInit(&argc, argv);
    glutInitDisplayMode(GLUT_RGBA|GLUT_SINGLE);
    glutCreateWindow("Fantasy Golf Simulator");

    glutDisplayFunc(draw);
    glutKeyboardFunc(handle_key);
    glutFullScreen();

    glutMainLoop();
    return 0;
}