#include "text_box.h"
#include "font.h"
#include "common.h"

void text_box_render_frame(text_box_t* t, size_t* vert_buf_offset,
                           size_t* idx_buf_offset) {

    const short cur_idx = text_box_vert_idx(*idx_buf_offset);

    float frame[] = {
        t->x, t->y,
        t->x, t->y + t->height,
        t->x + t->width, t->y + t->height,
        t->x + t->width, t->y,
    };
    unsigned short idxes[] = {
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
    render_string(t->content, t->x + TEXT_BOX_PADDING, t->y + TEXT_BOX_PADDING,
                  buf_offset);
}

unsigned short text_box_vert_idx(size_t idx_buf_offset) {
    // tied to the implementation of text_box_render_frame.
    // 6 indexes per frame and 4 verts per frame
    return (idx_buf_offset / sizeof(short)) / 6 * 4;
}

void text_box_input(text_box_t* t, unsigned char c) {
    if (t->can_accept && c != '\b' && !t->can_accept(t, c)) return;

    int insertion_point = -1;
    for (int i = 0; i < TEXT_BOX_MAX_LEN; ++i) {
        if (t->content[i] == 0) {
            insertion_point = i;
            break;
        }
    }
    if (insertion_point == -1) return;

    if (c == '\b') {
        if (insertion_point == 0) return;
        t->content[insertion_point - 1] = 0;
    } else {
        if (insertion_point == TEXT_BOX_MAX_LEN - 1) return;
        if (insertion_point + 1 > t->max_len) return;

        t->content[insertion_point] = c;
        t->content[insertion_point + 1] = 0;
    }
}

bool text_box_positive_nums_only(text_box_t* t, unsigned char c) {
    for (int i = 0; i < TEXT_BOX_MAX_LEN; ++i) {
        const char d = t->content[i];
        if (d == 0) break;
        if (d == '.' && c == '.') return false;
    }

    return c == '.' || (c >= '0' && c <= '9');
}
