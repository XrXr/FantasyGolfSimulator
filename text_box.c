#include "text_box.h"
#include "font.h"
#include "common.h"

void text_box_render_frame(text_box_t* t, size_t* vert_buf_offset,
                           size_t* idx_buf_offset) {

    const short cur_idx = (*idx_buf_offset / sizeof(short)) / 6 * 4;

    float frame[] = {
        t->x, t->y,
        t->x, t->y + t->height,
        t->x + t->width, t->y + t->height,
        t->x + t->width, t->y,
    };
    short idxes[] = {
        cur_idx, cur_idx + 1, cur_idx + 2,
        cur_idx + 3, cur_idx, GOLF_RESTART_IDX
    };
    glBufferSubData(GL_ARRAY_BUFFER, *vert_buf_offset, sizeof(frame), frame);
    glBufferSubData(GL_ELEMENT_ARRAY_BUFFER, *idx_buf_offset,
                    sizeof(idxes), idxes);
    *vert_buf_offset += sizeof(frame);
    *idx_buf_offset += sizeof(idxes);
}

void text_box_render_font(text_box_t* t, size_t* buf_offset) {
    render_string(t->content, t->x + 5, t->y + 5, buf_offset);
}

void text_box_input(text_box_t* t, unsigned char c) {
    int insertion_point = -1;
    for (int i = 0; i < TEXT_BOX_MAX_LEN; ++i) {
        if (t->content[i] == '\0') {
            insertion_point = i;
            break;
        }
    }

    if (insertion_point == -1) return;
    if (c == 8) { // backspace
        if (insertion_point == 0) return;
        t->content[insertion_point - 1] = '\0';
    } else {
        if (insertion_point == TEXT_BOX_MAX_LEN - 1) return;
        t->content[insertion_point] = c;
        t->content[insertion_point + 1] = '\0';
    }
}
