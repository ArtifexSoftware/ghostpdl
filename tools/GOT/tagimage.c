#include <stdio.h>

#define UNKNOWN_TAG 0x1
#define TEXT_TAG 0x2
#define IMAGE_TAG 0x4
#define PATH_TAG 0x8
#define UNTOUCHED_TAG 0x10

const char red[]   =  { 0xff, 0x00, 0x00 };
const char green[] =  { 0x00, 0xff, 0x00 };
const char blue[]  =  { 0x00, 0x00, 0xff };
const char white[] =  { 0xff, 0xff, 0xff };
const char black[] =  { 0x00, 0x00, 0x00 };
const char yellow[] = { 0xff, 0xff, 0x00 };

static const char *
tag2color(int tag)
{
    switch (tag) {
    case UNKNOWN_TAG:
        return white;
    case TEXT_TAG:
        return red;
    case IMAGE_TAG:
        return green;
    case PATH_TAG:
        return blue;
    case UNTOUCHED_TAG:
        return yellow;
    default:
        return black;
    }
}

int
main() 
{
    printf("P6\n");
    printf("612 792\n");
    printf("255\n");
    while (!feof(stdin)) {
        int ch = getchar();
        char *color = tag2color(ch);
        putchar(color[0]); putchar(color[1]); putchar(color[2]);
        getchar(); getchar(); getchar();
    }
    return 0;
}
