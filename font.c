#include <GL/glew.h>
#include "font.h"
#include <assert.h>

#include "build/bitmap_font_img.h"
#include "build/bitmap_font_meta.h"

void render_string(const char* s, float x, float y, size_t* buf_offset) {
    size_t i = 0;
    while (s[i] != '\0') {
        render_char(s[i], x, y, buf_offset);
        x += bitmap_font_meta.x_advance;
        i++;
    }
}

void render_char(const char c, const float x, const float y,
                 size_t* buf_offset) {
    if ((int) c - '!' < 0) return;

    int width = bitmap_font_meta.chars[c - '!'].width;
    int height = bitmap_font_meta.chars[c - '!'].height;
    int tex_x = bitmap_font_meta.chars[c - '!'].x;
    int tex_y = bitmap_font_meta.chars[c - '!'].y;
    int x_offset = bitmap_font_meta.chars[c - '!'].x_offset;
    int y_offset = bitmap_font_meta.chars[c - '!'].y_offset;

    int top_left_x = x + x_offset;
    int top_left_y = y + y_offset;

    int bottom_left_x = x + x_offset;
    int bottom_left_y = y + y_offset + height;

    int top_right_x = x + x_offset + width;
    int top_right_y = y + y_offset;

    int bottom_right_x = top_right_x;
    int bottom_right_y = bottom_left_y;

    float full_quad[] = {
        top_left_x, top_left_y,
        tex_x, tex_y,

        top_right_x, top_right_y,
        tex_x + width, tex_y,

        bottom_left_x, bottom_left_y,
        tex_x, tex_y + height,

        top_right_x, top_right_y,
        tex_x + width, tex_y,

        bottom_right_x, bottom_right_y,
        tex_x + width, tex_y + height,

        bottom_left_x, bottom_left_y,
        tex_x, tex_y + height,
    };
    glBufferSubData(GL_ARRAY_BUFFER, *buf_offset, sizeof(full_quad),
                    full_quad);
    *buf_offset += sizeof(full_quad);
}

GLuint font_init(void) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, bitmap_font_img.width,
                 bitmap_font_img.height, 0, GL_RGB, GL_UNSIGNED_BYTE,
                 bitmap_font_img.img);
    glGenerateMipmap(GL_TEXTURE_2D);
    return texture;
}
