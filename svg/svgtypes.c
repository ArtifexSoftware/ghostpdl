#include "ghostsvg.h"

int svg_is_whitespace_or_comma(int c)
{
     return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA) || (c == ',');
}

int svg_is_whitespace(int c)
{
    return (c == 0x20) || (c == 0x9) || (c == 0xD) || (c == 0xA);
}

int svg_is_alpha(int c)
{
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z');
}

int svg_is_digit(int c)
{
    return (c >= '0' && c <= '9') ||
	(c == 'e') || (c == 'E') ||
	(c == '+') || (c == '-') || (c == '.');
}


/* Return length/coordinate in points */
float
svg_parse_length(char *str, float percent, float font_size)
{
    char *end;
    float val;
    
    val = strtof(str, &end);
    if (end == str)
	return 0; /* failed */
    
    if (!strcmp(end, "px")) return val;
    
    if (!strcmp(end, "pt")) return val * 1.0;
    if (!strcmp(end, "pc")) return val * 12.0;
    if (!strcmp(end, "mm")) return val * 2.83464567;
    if (!strcmp(end, "cm")) return val * 28.3464567;
    if (!strcmp(end, "in")) return val * 72.0;

    if (!strcmp(end, "em")) return val * font_size;
    if (!strcmp(end, "ex")) return val * font_size * 0.5;
    
    if (!strcmp(end, "%"))
	return val * percent * 0.01;
    
    if (end[0] == 0)
	return val;
    
    return 0;
}

/* Return angle in degrees */
float
svg_parse_angle(char *str)
{
    char *end;
    float val;
    
    val = strtof(str, &end);
    if (end == str)
	return 0; /* failed */
    
    if (!strcmp(end, "deg"))
	return val;

    if (!strcmp(end, "grad"))
	return val * 0.9;

    if (!strcmp(end, "rad"))
	return val * 57.2957795;

    return val;
}

