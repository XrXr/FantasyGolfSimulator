#include <SOIL/SOIL.h>
#include <stdio.h>

int main(int argc, char **argv) {
    if (argc < 2) {
        printf("Usage: ./bitmap_img <path to image>\n");
        return 0;
    }

    int width, height;

    unsigned char* img = SOIL_load_image(argv[1], &width, &height, NULL, SOIL_LOAD_RGB);
    const size_t total_size = (size_t)width * (size_t)height * 3;
    printf("const struct {int width; int height; unsigned char img[%zu];} "
           "bitmap_font_img = {%d, %d, {\n", total_size, width, height);
    for (size_t i = 0; i < total_size - 1; ++i) {
        printf("%d,", img[i]);
    }
    printf("%d}};\n", img[total_size - 1]);
}