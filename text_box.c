#include "text_box.h"
#include "font.h"
#include "common.h"

void text_box_render_frame(text_box_t* t, size_t* vert_buf_offset,
                           size_t* idx_buf_offset) {

    const short cur_idx = *idx_buf_offset / sizeof(short);

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
