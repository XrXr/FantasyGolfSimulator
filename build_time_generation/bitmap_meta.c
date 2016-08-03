#include <stdio.h>
#define MAX_LINE_LEN 1000

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./bitmap_meta <path to .fnt>\n");
        return 0;
    }

    FILE* font_info = fopen(argv[1], "r");
    if (font_info == NULL) {
        fprintf(stderr, "Unable to open specified file\n");
        return 1;
    }

    // assumes monospace
    char dummy[MAX_LINE_LEN];
    fgets(dummy, MAX_LINE_LEN, font_info);
    fgets(dummy, MAX_LINE_LEN, font_info);
    fgets(dummy, MAX_LINE_LEN, font_info);

    int n;
    int num_read = fscanf(font_info, "chars count=%d\n", &n);
    if (num_read != 1 || n <= 0) {
        fprintf(stderr, "Input file is in unexpected format\n");
        return 1;
    }

    for (int i = 0; i < n; ++i) {
        int id, x, y, width, height, xoffset, yoffset, xadvance, page, chnl;
        num_read = fscanf(font_info,
            "char id=%d x=%d y=%d width=%d height=%d xoffset=%d "
            "yoffset=%d xadvance=%d page=%d chnl=%d\n",
            &id, &x, &y, &width, &height, &xoffset, &yoffset, &xadvance,
            &page, &chnl);

        if (num_read != 10) {
            fprintf(stderr, "Input file is in unexpected format\n");
            return 1;
        }

        if (i == 0) {
            printf("const struct{ int x_advance;"
                   "int num_char; struct {int x, y, width, height, x_offset, "
                                      "y_offset;} chars[%d];} "
                   "bitmap_font_meta = {%d, %d, {\n", n, xadvance, n);
        }

        printf("{%d, %d, %d, %d, %d, %d}", x, y, width, height, xoffset,
               yoffset);
        if (i != n - 1) {
            putchar(',');
        }
    }
    puts("}};\n");
}