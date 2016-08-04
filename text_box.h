#pragma once

#include <stdbool.h>
#include <stddef.h>

#define TEXT_BOX_MAX_LEN 100
#define TEXT_BOX_PADDING 5

typedef struct text_box {
    int width;
    int height;
    int x;
    int y;
    bool active;
    int max_len;
    char content[TEXT_BOX_MAX_LEN];
    bool (*can_accept) (struct text_box*, unsigned char);
} text_box_t;

void text_box_render_frame(text_box_t* t, size_t* vert_buf_offset,
                           size_t* idx_buf_offset);

void text_box_render_font(text_box_t* t, size_t* buf_offset);

void text_box_input(text_box_t* t, unsigned char c);

unsigned short text_box_vert_idx(size_t idx_buf_offset);

bool text_box_positive_nums_only(text_box_t* t, unsigned char c);
