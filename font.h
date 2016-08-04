#pragma once

#include <stdint.h>
#include <GL/glew.h>

void render_char(const char c, const float x, const float y,
                 size_t* buf_offset);
void render_string(const char* s, float x, float y, size_t* buf_offset);
GLuint font_init(void);
