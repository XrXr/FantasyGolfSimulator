#pragma once

#include <stdbool.h>
#include <stddef.h>

#define TEXT_BOX_MAX_LEN 100

typedef struct {
    int width;
    int height;
    int x;
    int y;
    bool active;
    char content[TEXT_BOX_MAX_LEN];
} text_box_t;

void text_box_render_frame(text_box_t* t, size_t* vert_buf_offset,
                           size_t* idx_buf_offset);

void text_box_render_font(text_box_t* t, size_t* buf_offset);

void text_box_input(text_box_t* t, unsigned char c);
