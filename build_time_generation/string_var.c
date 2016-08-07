#include <stdio.h>

int main(int argc, char** argv) {
    if (argc < 3) {
        printf("Usage: ./string_var <path to text file> <variable name>\n");
        return 0;
    }

    FILE* file = fopen(argv[1], "r");
    if (file == NULL) {
        fprintf(stderr, "Unable to open specified file\n");
        return 1;
    }

    printf("static const char* %s = \n\"", argv[2]);
    while (1) {
        int read = fgetc(file);
        if (read == EOF) break;
        if (read == '\n') {
            fputs("\\n\"\n\"", stdout);
        } else if (read == '\\') {
            fputs("\\\\", stdout);
        } else {
            putchar(read);
        }
    }
    puts("\";\n");
    fclose(file);
}
