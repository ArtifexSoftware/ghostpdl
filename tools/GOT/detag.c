#include <stdio.h>

/* detag bit rgb and produce ppmraw output */

int
main() {
    printf("P6\n");
    printf("612 792\n");
    printf("255\n");
    while (!feof(stdin)) {
        getchar();
        putchar(getchar());
        putchar(getchar());
        putchar(getchar());
    }
    return 0;
}

