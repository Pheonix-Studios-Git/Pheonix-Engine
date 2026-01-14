#include <stdlib.h>
#include <stdbool.h>

#include <rendering-sys.h>

#include <GL/gl.h>
#include <GL/glu.h>

void px_rs_init_ui(int w, int h, int x, int y) {
    glViewport(x, y, w, h);
    glDisable(GL_LIGHTING);
    glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluOrtho2D(0, w, 0, h);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
}

void px_rs_draw_panel(int w, int h, int x, int y, float r, float g, float b) { 
    glColor3f(r, g, b);
    glBegin(GL_QUADS);
        glVertex2f(x, y);
        glVertex2f(x + w, y);
        glVertex2f(x + w, y + h);
        glVertex2f(x, y + h);
    glEnd();
}
