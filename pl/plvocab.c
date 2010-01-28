/* Portions Copyright (C) 2001 artofcode LLC.
   Portions Copyright (C) 1996, 2001 Artifex Software Inc.
   Portions Copyright (C) 1988, 2000 Aladdin Enterprises.
   This software is based in part on the work of the Independent JPEG Group.
   All Rights Reserved.

   This software is distributed under license and may not be copied, modified
   or distributed except as expressly authorized under the terms of that
   license.  Refer to licensing information at http://www.artifex.com/ or
   contact Artifex Software, Inc., 101 Lucas Valley Road #110,
   San Rafael, CA  94903, (415)492-9861, for further information. */
/*$Id$ */

/* plvocab.c */
/* Maps between glyph vocabularies */
#include "stdpre.h"
#include "plvocab.h"

/* Define an entry in a glyph mapping table. */
/* Glyph mapping tables are sorted by key. */
typedef struct pl_glyph_mapping_s {
  ushort key;
  ushort value;
} pl_glyph_mapping_t;

/* Map from MSL to Unicode.  Derived from the HP book of characters
   and the HP comparison guide appendix D. */
static const pl_glyph_mapping_t pl_map_m2u[] = {
    {0, 0x0020}, /* space code */
    {1, 0x0021}, /* exclamation mark */ /* u */
    {2, 0x0022}, /* Neutral Double Quote */ /* u */
    {3, 0x0023}, /* Number Sign */ /* u */
    {4, 0x0024}, /* dollar sign */ /* u */
    {5, 0x0025}, /* Per Cent Sign */ /* u */
    {6, 0x0026}, /* ampersand */ /* u */
    {8, 0x2019}, /* Single Close Quote (9) */ /* u */
    {9, 0x0028}, /* Left Parenthesis */ /* u */
    {10, 0x0029}, /* Right Parenthesis */ /* u */
    {11, 0x002A}, /* asterisk */ /* u */
    {12, 0x002B}, /* Plus Sign */ /* u */
    {13, 0x002C}, /* Comma, Decimal Separator */ /* u */
    {14, 0x002D}, /* Hyphen */ /* u */
    {15, 0x002E}, /* period */ /* u */
    {16, 0x002F}, /* Solidus, Slash */ /* u */
    {17, 0x0030}, /* Numeral Zero */ /* u */
    {18, 0x0031}, /* Numeral One */ /* u */
    {19, 0x0032}, /* Numeral Two */ /* u */
    {20, 0x0033}, /* Numeral Three */ /* u */
    {21, 0x0034}, /*  Numeral Four */ /* u */
    {22, 0x0035}, /* Numeral Five */ /* u */
    {23, 0x0036}, /* Numeral Six */ /* u */
    {24, 0x0037}, /* Numeral Seven */ /* u */
    {25, 0x0038}, /* Numeral Eight */ /* u */
    {26, 0x0039}, /* Numeral Nine */ /* u */
    {27, 0x003A}, /* colon */ /* u */
    {28, 0x003B}, /* Semicolon */ /* u */
    {29, 0x003C}, /* Less Than Sign */ /* u */
    {30, 0x003D}, /* Equals Sign */ /* u */
    {31, 0x003E}, /* Greater Than Sign */ /* u */
    {32, 0x003F}, /* question mark */ /* u */
    {33, 0x0040}, /* Commercial At Sign */ /* u */
    {34, 0x0041}, /* Uppercase A */  /* u */
    {35, 0x0042}, /* Uppercase B */  /* u */
    {36, 0x0043}, /* Uppercase C */  /* u */
    {37, 0x0044}, /* Uppercase D */  /* u */
    {38, 0x0045}, /* Uppercase E */  /* u */
    {39, 0x0046}, /* Uppercase F */  /* u */
    {40, 0x0047}, /* Uppercase G */  /* u */
    {41, 0x0048}, /* Uppercase H */  /* u */
    {42, 0x0049}, /* Uppercase I */  /* u */
    {43, 0x004A}, /* Uppercase J */  /* u */
    {44, 0x004B}, /* Uppercase K */  /* u */
    {45, 0x004C}, /* Uppercase L */  /* u */
    {46, 0x004D}, /* Uppercase M */  /* u */
    {47, 0x004E}, /* Uppercase N */  /* u */
    {48, 0x004F}, /* Uppercase O */  /* u */
    {49, 0x0050}, /* Uppercase P */  /* u */
    {50, 0x0051}, /* Uppercase Q */  /* u */
    {51, 0x0052}, /* Uppercase R */  /* u */
    {52, 0x0053}, /* Uppercase S */  /* u */
    {53, 0x0054}, /* Uppercase T */  /* u */
    {54, 0x0055}, /* Uppercase U */  /* u */
    {55, 0x0056}, /* Uppercase V */  /* u */
    {56, 0x0057}, /* Uppercase W */  /* u */
    {57, 0x0058}, /* Uppercase X */  /* u */
    {58, 0x0059}, /* Uppercase Y */  /* u */
    {59, 0x005A}, /* Uppercase Z */  /* u */
    {60, 0x005B}, /* Left Bracket */ /* u */
    {61, 0x005C}, /* Rev Solidus, Backslash */ /* u */
    {62, 0x005D}, /* Right Bracket */ /* u */
    {63, 0x005E}, /* ASCII Circumflex */ /* u */

    {64, 0x005F}, /* Underline, Underscore */ /* u */
    {66, 0x2018}, /* Single Open Quote (6) */ /* u */
    {67, 0x0061}, /* Lowercase A */  /* u */
    {68, 0x0062}, /* Lowercase B */  /* u */
    {69, 0x0063}, /* Lowercase C */  /* u */
    {70, 0x0064}, /* Lowercase D */  /* u */
    {71, 0x0065}, /* Lowercase E */  /* u */
    {72, 0x0066}, /* Lowercase F */  /* u */
    {73, 0x0067}, /* Lowercase G */  /* u */
    {74, 0x0068}, /* Lowercase H */  /* u */
    {75, 0x0069}, /* Lowercase I */  /* u */
    {76, 0x006A}, /* Lowercase J */  /* u */
    {77, 0x006B}, /* Lowercase K */  /* u */
    {78, 0x006C}, /* Lowercase L */  /* u */
    {79, 0x006D}, /* Lowercase M */  /* u */
    {80, 0x006E}, /* Lowercase N */  /* u */
    {81, 0x006F}, /* Lowercase O */  /* u */
    {82, 0x0070}, /* Lowercase P */  /* u */
    {83, 0x0071}, /* Lowercase Q */  /* u */
    {84, 0x0072}, /* Lowercase R */  /* u */
    {85, 0x0073}, /* Lowercase S */  /* u */
    {86, 0x0074}, /* Lowercase T */  /* u */
    {87, 0x0075}, /* Lowercase U */  /* u */
    {88, 0x0076}, /* Lowercase V */  /* u */
    {89, 0x0077}, /* Lowercase W */  /* u */
    {90, 0x0078}, /* Lowercase X */  /* u */
    {91, 0x0079}, /* Lowercase Y */  /* u */
    {92, 0x007A}, /* Lowercase Z */  /* u */
    {93, 0x007B}, /* Left Brace */ /* u */
    {94, 0x007C}, /* Long Vertical Mark */ /* u */
    {95, 0x007D}, /* Right Brace */ /* u */
    {96, 0x007E}, /* 1 Wavy Line Approx */ /* u */
    {97, 0x2592}, /* Medium Shading */ /* u */
    {99, 0x00C0}, /* Uppercase A Grave */ /* u */
    {100, 0x00C2}, /* Uppercase A Circumflex */ /* u */
    {101, 0x00C8}, /* Uppercase E Grave */ /* u */
    {102, 0x00CA}, /* Uppercase E Circumflex */ /* u */
    {103, 0x00CB}, /* Uppercase E Dieresis */ /* u */
    {104, 0x00CE}, /* Uppercase I Circumflex */  /* u */
    {105, 0x00CF}, /* Uppercase I Dieresis */  /* u */
    {106, 0x00B4}, /* Lowercase Acute */  /* u */
    {107, 0x0060}, /* Lowercase Grave */ /* u */
    {108, 0x02C6}, /* Lowercase Circumflex *//* u */ /* ? 5E ? */
    {109, 0x00A8}, /* Lowercase Dieresis */  /* u */
    {110, 0x02DC}, /* Lowercase Tilde */ /* u */ /* ? 7E ? */
    {111, 0x00D9}, /* Uppercase U Grave */  /* u */
    {112, 0x00DB}, /* Uppercase U Circumflex */  /* u */
    {113, 0x00AF}, /* Overline, Overscore */  /* u */
    {114, 0x00DD}, /* Uppercase Y Acute */  /* u */
    {115, 0x00FD}, /* Lowercase Y Acute */  /* u */
    {116, 0x00B0}, /* Degree Sign */  /* u */
    {117, 0x00C7}, /* Uppercase C Cedilla */ /* u */
    {118, 0x00E7}, /* Lowercase C Cedilla */  /* u */
    {119, 0x00D1}, /* Uppercase N tilde */  /* u */
    {120, 0x00F1}, /* Lowercase N tilde */  /* u */
    {121, 0x00A1}, /* Inverted Exclamation Mark */  /* u */
    {122, 0x00BF}, /* Inverted Question Mark */  /* u */
    {123, 0x00A4}, /* Currency Symbol */ /* u */
    {124, 0x00A3}, /* Pound Sterling Sign */  /* u */
    {125, 0x00A5}, /* Yen Sign */  /* u */
    {126, 0x00A7}, /* Section Mark */  /* u */
    {127, 0x0192}, /* Florin Sign */  /* u */
    {128, 0x00A2}, /* Cent Sign */  /* u */
    {129, 0x00E2}, /* Lowercase A Circumflex */  /* u */
    {130, 0x00EA}, /* Lowercase E Circumflex */  /* u */
    {131, 0x00F4}, /* Lowercase O Circumflex */  /* u */
    {132, 0x00FB}, /* Lowercase U Circumflex */  /* u */
    {133, 0x00E1}, /* Lowercase A Acute */  /* u */
    {134, 0x00E9}, /* Lowercase E Acute */  /* u */
    {135, 0x00F3}, /* Lowercase O Acute */  /* u */
    {136, 0x00FA}, /* Lowercase U Acute */  /* u */
    {137, 0x00E0}, /* Lowercase A Grave */  /* u */
    {138, 0x00E8}, /* Lowercase E Grave */  /* u */
    {139, 0x00F2}, /* Lowercase O Grave */  /* u */
    {140, 0x00F9}, /* Lowercase U Grave */  /* u */
    {141, 0x00E4}, /* Lowercase A Dieresis */  /* u */
    {142, 0x00EB}, /* Lowercase E Dieresis */  /* u */
    {143, 0x00F6}, /* Lowercase O Dieresis */  /* u */
    {144, 0x00FC}, /* Lowercase U Dieresis */  /* u */
    {145, 0x00C5}, /* Uppercase A Ring */ /* u */
    {146, 0x00EE}, /* Lowercase I Circumflex */  /* u */
    {147, 0x00D8}, /* Uppercase O Oblique */  /* u */
    {148, 0x00C6}, /* Uppercase AE Diphthong */ /* u */
    {149, 0x00E5}, /* Lowercase A Ring */  /* u */
    {150, 0x00ED}, /* Lowercase I Acute */  /* u */
    {151, 0x00F8}, /* Lowercase O Oblique */  /* u */
    {152, 0x00E6}, /* Lowercase AE Diphthong */  /* u */
    {153, 0x00C4}, /* Uppercase A Dieresis */ /* u */
    {154, 0x00EC}, /* Lowercase I Grave */  /* u */
    {155, 0x00D6}, /* Uppercase O Dieresis */  /* u */
    {156, 0x00DC}, /* Uppercase U Dieresis */  /* u */
    {157, 0x00C9}, /* Uppercase E Acute */  /* u */
    {158, 0x00EF}, /* Lowercase I Dieresis */  /* u */
    {159, 0x00DF}, /* Lowercase Es-zet Ligature */  /* u */
    {160, 0x00D4}, /* Uppercase O Circumflex */  /* u */
    {161, 0x00C1}, /* Uppercase A Acute */ /* u */
    {162, 0x00C3}, /* Uppercase A Tilde */ /* u */
    {163, 0x00E3}, /* Lowercase A Tilde */  /* u */
    {164, 0x00D0}, /* Uppercase Eth, D-stroke *//* u */
    {165, 0x00F0}, /* Lowercase Eth */  /* u */
    {166, 0x00CD}, /* Uppercase I Acute */  /* u */
    {167, 0x00CC}, /* Uppercase I Grave */  /* u */
    {168, 0x00D3}, /* Uppercase O Acute */   /* u */
    {169, 0x00D2}, /* Uppercase O Grave */  /* u */
    {170, 0x00D5}, /* Uppercase O Tilde */  /* u */
    {171, 0x00F5}, /* Lowercase O Tilde */  /* u */
    {172, 0x0160}, /* Uppercase S Hacek */  /* u */
    {173, 0x0161}, /* Lowercase S Hacek */  /* u */
    {174, 0x00DA}, /* Uppercase U Acute */  /* u */
    {175, 0x0178}, /* Uppercase Y Dieresis */  /* u */
    {176, 0x00FF}, /* Lowercase Y Dieresis */  /* u */
    {177, 0x00DE}, /* Uppercase Thorn */  /* u */
    {178, 0x00FE}, /* Lowercase Thorn */  /* u */
    {179, 0x00B7}, /* Lowercase Middle Dot */  /* u */
    {180, 0x00B5}, /* Lowercase Mu, Micro */  /* u */
    {181, 0x00B6}, /* Pilcrow, Paragraph */  /* u */
    {182, 0x00BE}, /* Vulgar Fraction 3/4 */  /* u */
    {183, 0x2212}, /* Minus Sign */  /* u */
    {184, 0x00BC}, /* Vulgar Fraction 1/4 */  /* u */
    {185, 0x00BD}, /* Vulgar Fraction 1/2 */  /* u */
    {186, 0x00AA}, /* Female Ordinal */  /* u */
    {187, 0x00BA}, /* Mail Ordinal */  /* u */
    {188, 0x00AB}, /* Dbl lft point Ang Quote */  /* u */
    {189, 0x25A0}, /* Medium Solid Box */
    {190, 0x00BB}, /* Dbl rt point Ang Quote */  /* u */
    {191, 0x00B1}, /* Plus Over Minus Sign */  /* u */
    {192, 0x00A6}, /* Broken Vertical Mark */  /* u */
    {193, 0x00A9}, /* Copyright Sign, Sup */  /* u */
    {194, 0x00AC}, /* Not Sign */  /* u */
    {195, 0x00AD}, /* Soft Hyphen */  /* u */
    {196, 0x00AE}, /* Registered Sign, Sup */  /* u */
    {197, 0x00B2}, /* Superior Numeral 2 */  /* u */
    {198, 0x00B3}, /* Superior Numeral 3 */  /* u */
    {199, 0x00B8}, /* Lowercase Cedilla */  /* u */
    {200, 0x00B9}, /* Superior Numeral 1 */  /* u */
    {201, 0x00D7}, /* Multiply Sign */  /* u */
    {202, 0x00F7}, /* Divide Sign */  /* u */
    {203, 0x263A}, /* Open Smiling Face */
    {204, 0x263B}, /* Solid Smiling Face */
    {205, 0x2665}, /* Solid Heart */
    {206, 0x2666}, /* Solid Diamond */
    {207, 0x2663}, /* Solid Club */
    {208, 0x2660}, /* Solid Spade */
    {209, 0x2022}, /* Medium Solid Bullet */ /* (PC), 0x25CF (other) */ 
    {210, 0x25DB}, /* Large Solid Sq/Open Dot */
    {211, 0x25CB}, /* Large Open Bullet */		/* (PC only) */
    {212, 0x25D9}, /* Large Solid Sq/Open Ring */
    {213, 0x2642}, /* Male Symbol */
    {214, 0x2640}, /* Female Symbol */
    {215, 0x266A}, /* Musical Note */
    {216, 0x266B}, /* Pair of Musical Notes */
    {217, 0x263C}, /* Compass, 8 Pointed Sun */
    {218, 0x25BA}, /* Right Solid Arrowhead */
    {219, 0x25C4}, /* Left Solid Arrowhead */
    {220, 0x2195}, /* Up/Down Arrow */
    {221, 0x203C}, /* Double Exclamation Mark */  /* u */
    {222, 0x25AC}, /* Thick Horizontal Mark */
    {223, 0x21A8}, /* Up/Down Arrow Baseline */
    {224, 0x2191}, /* Up Arrow */
    {225, 0x2193}, /* Down Arrow */
    {226, 0x2192}, /* Right Arrow */
    {227, 0x2190}, /* Left Arrow */
    {228, 0x221F}, /*  Tick Up at Left */
    {229, 0x2194}, /* Left/Right Arrow */
    {230, 0x25B2}, /* Up Solid Arrowhead */
    {231, 0x25BC}, /* Down Solid Arrowhead */
    {232, 0x20A7}, /* Pesetas */  /* u */
    {233, 0x2310}, /* Tick Down at Left */  /* u */
    {234, 0x2591}, /* Light Shading */
    {235, 0x2593}, /* Dark Shading */
    {236, 0x2502}, /* Vertical 1 */
    {237, 0x2524}, /* Left 1 Vertical 1 */
    {238, 0x2561}, /* Left 2 Vertical 1 */
    {239, 0x2562}, /* Left 1 Vertical 2 */
    {240, 0x2556}, /* Left 1 Down 2 */
    {241, 0x2555}, /* Left 2 Down 1 */
    {242, 0x2563}, /* Left 2 Verical 2 */
    {243, 0x2551}, /* Vertical 2 */
    {244, 0x2557}, /* Left 2 Down 2 */
    {245, 0x255d}, /* Left 2 Up 2 */
    {246, 0x255c}, /* Left 1 Up 2 */
    {247, 0x255b}, /* Left 2 Up 1 */
    {248, 0x2510}, /* Left 1 Down 1 */
    {249, 0x2514}, /* Right 1 Up 1 */
    {250, 0x2534}, /* Horizontal 1 Up 1 */
    {251, 0x252c}, /* Horizontal 1 Down 1 */
    {252, 0x251c}, /* Right 1 Vertical 1 */
    {253, 0x2500}, /* Horizontal 1 */
    {254, 0x253c}, /* Horizontal 1 Vertical 1 */
    {255, 0x255e}, /* Right 2 Vertical 1 */
    {256, 0x255f}, /* Right 1 Vertical 2 */
    {257, 0x255a}, /* Right 2 Up 2 */
    {258, 0x2554}, /* Right 2 Down 2 */
    {259, 0x2569}, /* Horizontal 2 Up 2 */
    {260, 0x2566}, /* Horizontal 2 Down 2 */
    {261, 0x2560}, /* Right 2 Vertical 2 */
    {262, 0x2550}, /* Horizontal 2 */
    {263, 0x256c}, /*  Horizontal 2 Vertical 2 */
    {264, 0x2567}, /* Horizontal 2 Up 1 */
    {265, 0x2568}, /* Horizontal 1 Up 2 */
    {266, 0x2564}, /* Horizontal 2 Down 1 */
    {267, 0x2565}, /* Horizontal 1 Down 2 */
    {268, 0x2559}, /* Right 1 Up 2 */
    {269, 0x2558}, /* Right 2 Up 1 */
    {270, 0x2552}, /* Right 2 Down 1 */
    {271, 0x2553}, /* Right 1 Down 2 */
    {272, 0x256b}, /* Horizontal 1 Vertical 2 */
    {273, 0x256a}, /* Horizontal 2 Vertical 1 */
    {274, 0x2518}, /* Left 1 Up 1 */
    {275, 0x250c}, /* Right 1 Down 1 */
    {276, 0x2588}, /* Solid Full Rectangle */
    {277, 0x2584}, /* Bottom 1/2 Solid Rectangle */
    {278, 0x258c}, /* Left 1/2 Solid Rectangle */
    {279, 0x2590}, /* Right 1/2 Solid Rectangle */
    {280, 0x2580}, /* Top 1/2 Solid Rectangle */
    {281, 0x03B1}, /* Lowercase Greek Alpha */  /* u */
    {283, 0x0393}, /* Uppercase Greek Gamma */  /* u */
    {284, 0x03C0}, /* Lowercase Greek Pi */  /* u */
    {285, 0xFFFF}, /* Uppercase Greek Sigma */
    {286, 0x03C3}, /* Lowercase Greek Sigma */  /* u */
    {287, 0x03C4}, /* Lowercase Greek Tau */  /* u */
    {288, 0x03A6}, /* Uppercase Greek Phi */  /* u */
    {289, 0x0398}, /* Uppercase Greek Theta */  /* u */
    {290, 0x2126}, /* Uppercase Omega, Ohms */ /* u */
    {291, 0x03B4}, /* Lowercase Greek Delta */  /* u */
    {292, 0x221E}, /* Infinity Sign */  /* u */
    {293, 0x03C6}, /* Lowercase Greek Phi */  /* u */
    {294, 0x03B5}, /* Lowercase Greek Epsilon */  /* u */
    {295, 0x2229}, /* Set Intersection */  /* u */
    {296, 0x2261}, /* Exactly Equals */  /* u */
    {297, 0x2265}, /* Greater Than/Equal To */  /* u */
    {298, 0x2264}, /* Less Than/Equal To */  /* u */
    {299, 0x2320}, /* Top Segment Integral */
    {300, 0x2321}, /* Bottom Segment Integral */
    {301, 0x2248}, /* 2 Wavy Line Approx */  /* u */
    {302, 0x2219}, /* Center Period */  /* u */
    {303, 0x221A}, /* Radical Symbol */  /* u */
    {305, 0x25A0}, /* Small Solid Box */		/* (PC), 0x25AA (other) */ 
    {306, 0x013F}, /* Uppercase L Dot */  /* u */
    {307, 0x0140}, /* Lowercase L-Dot */ /* u */
    {308, 0x2113}, /* Litre Symbol */  /* u */
    {309, 0x0149}, /* Lowercase Apostrophe-N */  /* u */
    {310, 0x2032}, /* Prime */  /* u */
    {311, 0x2033}, /* Double Prime */  /* u */
    {312, 0x2020}, /* Dagger Symbol */ /* u */
    {313, 0x2122}, /* Trademark Sign, Sup */ /* u */
    {314, 0x2017}, /* Double Underline */
    {315, 0x02C7}, /* Lowercase Hacek */ /* u */
    {316, 0x02DA}, /* Lowercase Ring */  /* u */
    {317, 0x02CA}, /* Uppercase Acute */
    {318, 0x02CB}, /* Uppercase Grave */
    {319, 0x02C6}, /* Uppercase Circumflex */
    {320, 0x00A8}, /* Uppercase Dieresis */ 
    {321, 0x02DC}, /* Uppercase Tilde */
    {322, 0x02C7}, /* Uppercase Hacek */
    {323, 0x02DA}, /* Uppercase Ring */
    {324, 0x2215}, /* Vulgar Fraction Bar */  /* u */
    {325, 0x2014}, /* Em Dash */  /* u */
    {326, 0x2013}, /* En Dash */  /* u */
    {327, 0x2021}, /* Double Dagger Symbol */  /* u */
    {328, 0x0131}, /* Lowercase Undotted I */ /* u */
    {329, 0x0027}, /* Neutral Single Quote */ /* u */
    {330, 0x00B8}, /* Uppercase Cedilla */
    {331, 0x2022}, /* Small Solid Bullet */ /* u */
    {332, 0x207F}, /* Superior Lowercase N */  /* u */
    {333, 0x2302}, /* Home Plate */
    {335, 0x0138}, /* Lowercase Greenlandic Kra */ /* u */
    {338, 0x0166}, /* Uppercase T-Stroke */  /* u */
    {339, 0x0167}, /* Lowercase T-Stroke */  /* u */
    {340, 0x014A}, /* Uppercase Eng */  /* u */
    {341, 0x014B}, /* Lowercase Eng */  /* u */
    {342, 0x0111}, /* Lowercase D-Stroke */ /* u */
    {400, 0x0102}, /* Uppercase A Breve */ /* u */
    {401, 0x0103}, /* Lowercase A Breve */ /* u */
    {402, 0x0100}, /* Uppercase A Macron */ /* u */
    {403, 0x0101}, /* Lowercase A Macron */ /* u */
    {404, 0x0104}, /* Uppercase A Ogonek */ /* u */
    {405, 0x0105}, /* Lowercase A Ogonek */ /* u */
    {406, 0x0106}, /* Uppercase C Acute */ /* u */
    {407, 0x0107}, /* Lowercase C Acute */ /* u */
    {410, 0x010C}, /* Uppercase C Hacek */ /* u */
    {411, 0x010D}, /* Lowercase C Hacek */ /* u */
    {414, 0x010E}, /* Uppercase D Hacek */ /* u */
    {415, 0x010F}, /* Lowercase D Hacek */ /* u */
    {416, 0x011A}, /* Uppercase E Hacek */  /* u */
    {417, 0x011B}, /* Lowercase E Hacek */  /* u */
    {418, 0x0116}, /* Uppercase E Overdot */ /* u */
    {419, 0x0117}, /* Lowercase E Overdot */ /* u */
    {420, 0x0112}, /* Uppercase E Macron */  /* u */
    {421, 0x0113}, /* Lowercase E Macron */  /* u */
    {422, 0x0118}, /* Uppercase E Ogonek */  /* u */
    {423, 0x0119}, /* Lowercase E Ogonek */  /* u */
    {428, 0x0122}, /* Uppercase G Cedilla */  /* u */
    {429, 0x0123}, /* Lowercase G Cedilla */ /* u */
    {432, 0x012E}, /* Uppercase I Ogonek */  /* u */
    {433, 0x012F}, /* Lowercase I Ogonek */  /* u */
    {434, 0x012A}, /* Uppercase I Macron */  /* u */
    {435, 0x012B}, /* Lowercase I Macron */  /* u */
    {438, 0x0136}, /* Uppercase K Cedilla */  /* u */
    {439, 0x0137}, /* Lowercase K Cedilla */ /* u */
    {440, 0x0139}, /* Uppercase L Acute */  /* u */
    {441, 0x013A}, /* Lowercase L Acute */  /* u */
    {442, 0x013D}, /* Uppercase L Hacek */  /* u */
    {443, 0x013E}, /* Lowercase L Hacek */  /* u */
    {444, 0x013B}, /* Uppercase L Cedilla */  /* u */
    {445, 0x013C}, /* Lowercase L Cedilla */  /* u */
    {446, 0x0143}, /* Uppercase N Acute */ /* u */
    {447, 0x0144}, /* Lowercase N Acute */ /* u */
    {448, 0x0147}, /* Uppercase N Hacek */  /* u */
    {449, 0x0148}, /* Lowercase N Hacek */  /* u */
    {450, 0x0145}, /* Uppercase N Cedilla */  /* u */
    {451, 0x0146}, /* Lowercase N Cedilla */  /* u */
    {452, 0x0150}, /* Uppercase O Dbl Acute */  /* u */
    {453, 0x0151}, /* Lowercase O Dbl Acute */  /* u */
    {454, 0x014C},       /* Uppercase O Macron */  /* u */
    {455, 0x014D},       /* Lowercase O Macron */ /* u */
    {456, 0x0154}, /* Uppercase R Acute */ /* u */
    {457, 0x0155}, /* Lowercase R Acute */ /* u */
    {458, 0x0158}, /* Uppercase R Hacek */ /* u */
    {459, 0x0159}, /* Lowercase R Hacek */ /* u */
    {460, 0x0156},       /* Uppercase R Cedilla */  /* u */
    {461, 0x0157},       /* Lowercase R Cedilla */  /* u */
    {462, 0x015A}, /* Uppercase S Acute */ /* u */
    {463, 0x015B}, /* Lowercase S Acute */ /* u */
    {466, 0x0164}, /* Uppercase T Hacek */ /* u */
    {467, 0x0165}, /* Lowercase T Hacek */ /* u */
    {468, 0x0162}, /* Uppercase T Cedilla */ /* u */
    {469, 0x0163}, /* Lowercase T Cedilla */ /* u */
    {470, 0x0168}, /* Uppercase U Tilde */  /* u */
    {471, 0x0169}, /* Lowercase U Tilde */ /* u */
    {474, 0x0170}, /* Uppercase U Double Acute */ /* u */
    {475, 0x0171}, /* Lowercase U Double Acute */ /* u */
    {476, 0x016E}, /* Uppercase U Ring */ /* u */
    {477, 0x016F}, /* Lowercase U Ring */ /* u */
    {478, 0x016A}, /* Uppercase U Macron */  /* u */
    {479, 0x016B}, /* Lowercase U Macron */  /* u */
    {480, 0x0172}, /* Uppercase U Ogonek */  /* u */
    {481, 0x0173}, /* Lowercase U Ogonek */ /* u */
    {482, 0x0179}, /* Uppercase Z Acute */ /* u */
    {483, 0x017A}, /* Lowercase Z Acute */ /* u */
    {484, 0x017B}, /* Uppercase Z Overdot */ /* u */
    {485, 0x017C}, /* Lowercase Z Overdot */ /* u */
    {486, 0x0128}, /* Uppercase I Tilde */  /* u */
    {487, 0x0129}, /* Lowercase I Tilde */  /* u */
    {500, 0x221A}, /* Radical Segment */
    {501, 0x221D}, /* Proportional To Symbol */
    {502, 0x212F}, /* Lowercase Italic E */
    {503, 0xFFFF}, /* Alternate LC Epsilon */
    {504, 0x22C5}, /* Therefore */
    {505, 0x0393}, /* Uppercase Greek Gamma */
    {506, 0x2206}, /* Uppercase Greek Delta */ /* u */
    {507, 0xFFFF}, /* Uppercase Greek Theta */
    {508, 0xFFFF}, /* Uppercase Greek Lambda */
    {509, 0xFFFF}, /* Uppercase Greek Xi */
    {510, 0xFFFF}, /* Uppercase Greek Pi */
    {511, 0xFFFF}, /* Uppercase Greek Sigma */

    {512, 0x03a5}, /* Uppercase Greek Upsilon */
    {513, 0xFFFF}, /* Uppercase Greek Phi */
    {514, 0xFFFF}, /* Uppercase Greek Psi */
    {515, 0xFFFF}, /* Uppercase Greek Omega */
    {516, 0xFFFF}, /* Nabla Symbol */
    {517, 0x2202}, /* Partial Diff Symbol */  /* u */
    {518, 0xFFFF}, /* Terminal Symbol */
    {519, 0x2260}, /* Not Equal To Symbol */  /* u */
    {520, 0xFFFF}, /* Underline, Composite */
    {521, 0xFFFF}, /* Because */
    {522, 0x03B1}, /* Lowercase Greek Alpha */
    {523, 0x03B2}, /* Lowercase Greek Beta */
    {524, 0x03B3}, /* Lowercase Greek Gamma */
    {525, 0x03B4}, /* Lowercase Greek Delta */
    {526, 0x03B5}, /* Lowercase Greek Epsilon */
    {527, 0x03B6}, /* Lowercase Greek Zeta */
    {528, 0x03B7}, /* Lowercase Greek Eta */
    {529, 0x03B8}, /* Lowercase Greek Theta */
    {530, 0x03B9}, /* Lowercase Greek Iota */
    {531, 0x03BA}, /* Lowercase Greek Kappa */
    {532, 0x03BB}, /* Lowercase Greek Lambda */
    {533, 0x03BC}, /* Lowercase Greek Mu */
    {534, 0x03BD}, /* Lowercase Greek Nu */
    {535, 0x03BE}, /* Lowercase Greek Xi */
    {536, 0x03BF}, /* Lowercase Greek Omicron */
    {537, 0x03C0}, /* Lowercase Greek Pi */ /* u */
    {538, 0x03C1}, /* Lowercase Greek Rho */
    {539, 0x03C3}, /* Lowercase Greek Sigma */
    {540, 0x03C4}, /* Lowercase Greek Tau */
    {541, 0x03C5}, /* Lowercase Greek Upsilon */
    {542, 0x03C6}, /* Lowercase Greek Phi */
    {543, 0x03C7}, /* Lowercase Greek Chi */
    {544, 0x03C8}, /* Lowercase Greek Psi */
    {545, 0x03C9}, /* Lowercase Greek Omega */
    {546, 0x03D1}, /* Open LC Greek Theta */
    {547, 0x03D5}, /* Open LC Greek Phi */
    {548, 0x03D6}, /* Alternate LC Pi */
    {549, 0x2243}, /* Wavy/Straight Approx */
    {550, 0x2262}, /* Not Exactly Equal To */
    {551, 0xFFFF}, /* Up Arrow Dbl Stroke */
    {552, 0x21D2}, /* Right Arrow Dbl Stroke */
    {553, 0x21D3}, /* Down Arrow Dbl Stroke */
    {554, 0x21D0}, /* Left Arrow Dbl Stroke */
    {555, 0x21D5}, /* Up/Dn Arrow Dbl Stroke */
    {556, 0x21D4}, /* Lft/Rt Arrow Dbl Stroke */
    {557, 0x21C4}, /* Right Over Left Arrow */
    {558, 0x21C6}, /* Left Over Right Arrow */ 
    {559, 0xFFFF}, /* Vector Symbol */
    {560, 0xFFFF}, /* Overline, Composite */
    {561, 0x2200}, /* For All Symbol */
    {562, 0xFFFF}, /* There Exists Symbol */
    {563, 0xFFFF}, /* Top Symbol */
    {564, 0xFFFF}, /* Bottom Symbol */
    {565, 0x222A}, /* Set Union Symbol */
    {566, 0x2208}, /* Element Of Symbol */
    {567, 0x220B}, /* Contains Symbol */
    {568, 0x2209}, /* Not Element Of Symbol */
    {569, 0x2282}, /* Proper Subset Symbol */
    {570, 0x2283}, /* Proper Superset Symbol */
    {571, 0x2284}, /* Not Proper Subset Symbol */
    {572, 0x2285}, /* Not Proper Superset Symbol */
    {573, 0x2286}, /* Subset Symbol */
    {574, 0x2287}, /* Superset Symbol */
    {575, 0x2295}, /* Plus In Circle Symbol */
    {576, 0x2299}, /* Dot In Circle */
    {577, 0x2297}, /* Times In Circle */
    {578, 0x2296}, /* Minus In Circle */
    {579, 0x2298}, /*  Slash In Circle */
    {580, 0x2227}, /* Logical And Symbol */
    {581, 0x2228}, /* Logical Or Symbol */
    {582, 0x22BB}, /* Exclusive Or Symbol */
    {583, 0xFFFF}, /* Funct. Composition Sym */
    {584, 0xFFFF}, /* Large Open Circle */
    {585, 0xFFFF}, /* Assertion Symbol */
    {586, 0xFFFF}, /* Backwards Assertion Symbol */
    {587, 0x222B}, /* Integral Symbol */  /* u */
    {588, 0x222E}, /* Curvilinear Integral Sym */
    {589, 0x2220}, /* Angle Symbol */
    {590, 0x2205}, /* Empty Set Symbol */
    {591, 0x05D0}, /* Hebrew Aleph */
    {592, 0x05D1}, /* Hebrew Beth */
    {593, 0xFFFF}, /* Hebrew Gimmel */
    {594, 0xFFFF}, /* Fraktur C */
    {595, 0xFFFF}, /* Fraktur I */
    {596, 0xFFFF}, /* Fraktur R */
    {597, 0xFFFF}, /* Fraktur Z */
    {598, 0xFFFF}, /* Top Left Bracket */
    {599, 0xFFFF}, /* Bottom Left Bracket */
    {600, 0xFFFF}, /* Top Left Brace */
    {601, 0xFFFF}, /* Middle Left Brace */
    {602, 0xFFFF}, /* Bottom Left Brace */
    {603, 0xFFFF}, /* Middle Curv. Integral */
    {604, 0xFFFF}, /* Top Left Summation */
    {605, 0x2016}, /* Dbl Verical Line, Comp */
    {606, 0xFFFF}, /* Bottom Left Summation */
    {607, 0xFFFF}, /* Bottom Diag. Summation */
    {608, 0xFFFF}, /* Top Right Bracket */
    {609, 0xFFFF}, /* Bottom Right Bracket */
    {610, 0xFFFF}, /* Top Right Brace */
    {611, 0xFFFF}, /* Middle Right Brace */
    {612, 0xFFFF}, /* Bottom Right Brace */
    {613, 0xFFFF}, /* Thick Vert Line, Comp */
    {614, 0xFFFF}, /* Thin Vert Line, Comp */
    {615, 0xFFFF}, /* Bottom Radical, Vert */
    {616, 0xFFFF}, /* Top Right Summation */
    {617, 0xFFFF}, /* Middle Summation */
    {618, 0xFFFF}, /* Bottom Right Summation */
    {619, 0xFFFF}, /* Top Diagonal Summation */
    {620, 0xFFFF}, /* Minus Over Plus Sign */
    {621, 0xFFFF}, /* Left Angle Bracket */
    {622, 0xFFFF}, /* Right Angle Bracket */
    {623, 0xFFFF}, /* Mask Symbol, Sup */
    {624, 0x2245}, /* Wavy/2 Straight Approx */
    {625, 0x2197}, /* 45 degree Arrow */
    {626, 0x2198}, /* -45 degree Arrow */
    {627, 0x2199}, /* -135 degree Arrow */
    {628, 0x2196}, /* 135 degree Arrow */
    {629, 0x2196}, /* Up Open Triangle */
    {630, 0xFFFF}, /* Right Open Triangle */
    {631, 0xFFFF}, /* Down Open Triangle */
    {632, 0xFFFF}, /* Left Open Triangle */
    {633, 0xFFFF}, /* Much Less Than Sign */
    {634, 0xFFFF}, /* Much Greater Than Sign */
    {635, 0x2237}, /* Proportional To Symbol */
    {636, 0xFFFF}, /* Defined As Symbol */
    {637, 0xFFFF}, /* LC Greek Digamma */
    {638, 0x210F}, /* Plank's Constant/2 pi */
    {639, 0x2112}, /* Laplace Transform Sym */
    {640, 0xFFFF}, /* Power Set Symbol */
    {641, 0x2118}, /* Weierstraussian Symbol */
    {642, 0x2211}, /* Summation Symbol, Sigma */  /* u */
    {643, 0xFFFF}, /* Left Double Bracket */
    {644, 0xFFFF}, /* Middle Dbl Bracket */
    {645, 0xFFFF}, /* Right Double Bracket */
    {646, 0xFFFF}, /* Lft Top Round Corner */
    {647, 0xFFFF}, /* Lft Bottom Round Corner */
    {648, 0xFFFF}, /* Ext Lrg Union/Product */
    {649, 0xFFFF}, /* Bottom Lrg Union */
    {650, 0xFFFF}, /* Top Large Intersection */
    {651, 0xFFFF}, /* Top Lft Dbl Bracket */
    {652, 0xFFFF}, /* Bottom Lft Dbl Bracket */
    {653, 0xFFFF}, /* Large Open Box */
    {654, 0x25CA}, /* Open Diamond */
    {655, 0xFFFF}, /* Rt Top Round Corner */
    {656, 0xFFFF}, /* Rt Bottom Round Corner */
    {657, 0xFFFF}, /* Bottom Lrg Bott Product */
    {658, 0x220F}, /* Top Large Top Product */
    {659, 0xFFFF}, /* Top Rt Dbl Bracket */
    {660, 0xFFFF}, /* Bottom Rt Dbl Bracket */
    {661, 0xFFFF}, /* Large Solid Box */
    {662, 0xFFFF}, /* Solid Diamond */
    {663, 0x220B}, /* Such That */
    {664, 0x2217}, /* Math Asterisk Sign */
    {665, 0xFFFF}, /* Horizontal Arrow Ext */
    {666, 0xFFFF}, /* Dbl Horizontal Arrow Ext */
    {667, 0xFFFF}, /* Complement of #617 */
    {668, 0xFFFF}, /* Right Angle Symbol */
    {1000, 0xFFFF}, /* Superior Numeral 0 */
    {1001, 0xFFFF}, /* Superior Numeral 4 */
    {1002, 0xFFFF}, /* Superior Numeral 5 */
    {1003, 0xFFFF}, /* Superior Numeral 6 */
    {1004, 0xFFFF}, /* Superior Numeral 7 */
    {1005, 0xFFFF}, /* Superior Numeral 8 */
    {1006, 0xFFFF}, /* Superior Numeral 9 */
    {1017, 0x201C}, /* Double Open Quote (6) */ /* u */
    {1018, 0x201D}, /* Double Close Quote (9) */  /* u */
    {1019, 0x201E}, /* Dbl Baseline Quote (9) */  /* u */
    {1020, 0x2003}, /* Em Space */
    {1021, 0x2002}, /* En Space */
    {1023, 0x2009}, /* Thin Space */
    {1028, 0x2026}, /* Ellipsis */ /* u */
    {1030, 0x02DB}, /* Uppercase Ogonek */
    {1031, 0x017E}, /* Lowercase Z Hacek */ /* u */
    {1034, 0xFFFF}, /* Signature Mark, Sup */
    {1036, 0x211E}, /* Prescription Sign */
    {1040, 0xF001}, /* Lowercase FI Ligature */  /* u */
    {1041, 0xF002}, /* Lowercase FL Ligature */  /* u */
    {1042, 0xFFFF}, /* Lowercase FF Ligature */ 
    {1043, 0xFFFF}, /* Lowercase FFI Ligature */
    {1044, 0xFFFF}, /* Lowercase ffl Ligature */
    {1045, 0x02DD}, /* Uppercase Dbl Acute */
    {1047, 0x0133}, /* Lowercase IJ Ligature */  /* u */
    {1060, 0x2105}, /* Care Of Symbol */  /* u */
    {1061, 0x011E}, /* Uppercase G Breve */ /* u */
    {1062, 0x011F}, /* Lowercase G Breve */ /* u */
    {1063, 0x015E}, /* Uppercase S Cedilla */ /* u */
    {1064, 0x015F}, /* Lowercase S Cedilla */ /* u */
    {1065, 0x0130}, /* Uppercase I Overdot */ /* u */
    {1067, 0x201A}, /* Sngl Baseline Quote (9) */ /* u */
    {1068, 0x2030}, /* Per Mill Sign */  /* u */
    {1069, 0x20AC}, /* Euro Sign */
    {1084, 0x02C9}, /* Lowercase Macron */  /* u */
    {1085, 0xFFFF}, /* Uppercase Macron */
    {1086, 0x02D8}, /* Lowercase Breve */  /* u */
    {1087, 0xFFFF}, /* Uppercase Breve */
    {1088, 0x02D9}, /* Lowercase Overdot */ /* u */
    {1089, 0xFFFF}, /* Uppercase Overdot */
    {1090, 0x0153}, /* Lowercase OE Ligature */  /* u */
    {1091, 0x0152}, /* Uppercase OE Ligature */  /* u */
    {1092, 0x2039}, /* Sngl lft point Ang Quote */ /* u */
    {1093, 0x203A}, /* Sngl rt point Ang Quote */ /* u */
    {1094, 0x25A1}, /* Medium Open Box */
    {1095, 0x0141}, /* Uppercase L-Stroke */  /* u */
    {1096, 0x0142}, /* Lowercase L-Stroke */  /* u */
    {1097, 0x02DD}, /* Lowercase Dbl Acute */ /* u */
    {1098, 0x02DB}, /* Lowercase Ogonek */ /* u */
    {1099, 0x21B5}, /* Carriage Return Symbol */
    {1100, 0xFFFF}, /* Registered, Full, Serif */
    {1101, 0x00A9}, /* Copyright, Full, Serif */
    {1102, 0xFFFF}, /* Trademark, Full, Serif */
    {1103, 0xFFFF}, /* Registered, Full, Sans */
    {1104, 0xFFFF}, /* Copyright, Full, Sans */
    {1105, 0xFFFF}, /* Trademark, Full, Sans */
    {1106, 0x017D}, /* Uppercase Z Hacek */ /* u */
    {1107, 0x0132}, /* Uppercase IJ Ligature */  /* u */
    {1108, 0x25AB}, /* Small Open Box */
    {1109, 0x25E6}, /* Small Open Bullet */
    {1110, 0x25CB}, /* Medium Open Bullet */
    {1111, 0x25CF}, /* Large Solid Bullet */
    {1112, 0xFFFF}, /* Visible Carriage Return */
    {1113, 0xFFFF}, /* Visible Tab */
    {1114, 0xFFFF}, /* Visible Space */
    {1115, 0xFFFF}, /* Visible Soft Hyphen */
    {1116, 0xFFFF}, /* Visible End-of-File */
    {2010, 0x2713}, /* */
    {2011, 0x2727}, /* */
    {2014, 0x2722}, /* */
    {2027, 0x2717}, /* */
    {2030, 0x00A7}, /* */
    {2032, 0x272C}, /* */
    {2044, 0x271D}, /* */
    {2046, 0x2734}, /* */
    {2047, 0x272B}, /* */
    {2048, 0x25B2}, /* */
    {2050, 0x2020}, /* */
    {2052, 0x272A}, /* */
    {2058, 0x2730}, /* */
    {2059, 0x25EF}, /* */
    {2060, 0x272D}, /* */
    {2064, 0x2606}, /* */
    {2072, 0x2749}, /* */
    {2076, 0x271E}, /* */
    {2077, 0x2718}, /* */
    {2078, 0x272E}, /* */
    {2080, 0x2533}, /* */
    {2083, 0x2605}, /* */
    {2088, 0x2729}, /* */
    {2090, 0x2737}, /* */
    {2091, 0x25A0}, /* */
    {2092, 0x272F}, /* */
    {2096, 0x2702}, /* */
    {2116, 0x274D}, /* */
    {2120, 0x2703}, /* */
    {2121, 0x2712}, /* */
    {2210, 0x2714}, /* */
    {2230, 0x2720}, /* */
    {2232, 0x274B}, /* */
    {2240, 0x2738}, /* */
    {2248, 0x25BC}, /* */
    {2249, 0x25D6}, /* */
    {2250, 0x274F}, /* */
    {2252, 0x2732}, /* */
    {2256, 0x2731}, /* */
    {2258, 0x2743}, /* */
    {2259, 0x2610}, /* */
    {2260, 0x2747}, /* */
    {2263, 0x00A9}, /* */
    {2264, 0x273B}, /* */
    {2265, 0x27B4}, /* */
    {2272, 0x2739}, /* */
    {2279, 0x273B}, /* */
    {2280, 0x25BD}, /* */
    {2282, 0x2751}, /* */
    {2283, 0x2733}, /* */
    {2286, 0x2715}, /* */
    {2287, 0x27B5}, /* */
    {2288, 0x2741}, /* */
    {2290, 0x273E}, /* */
    {2292, 0x2748}, /* */
    {2293, 0x00AE}, /* */
    {2295, 0x261E}, /* */
    {2307, 0x270F}, /* */
    {2308, 0x2752}, /* */
    {2312, 0x261B}, /* */
    {2315, 0x2706}, /* */
    {2324, 0x2711}, /* */
    {2325, 0x2750}, /* */
    {2327, 0x27B6}, /* */
    {2411, 0x2721}, /* */
    {2432, 0x2746}, /* */
    {2436, 0x007B}, /* */
    {2444, 0x2719}, /* */
    {2448, 0x2726}, /* */
    {2451, 0x273F}, /* */
    {2452, 0x273A}, /* */
    {2454, 0x275A}, /* */
    {2460, 0x2740}, /* */
    {2464, 0x2744}, /* */
    {2468, 0x0079}, /* */
    {2472, 0x2723}, /* */
    {2476, 0x211F}, /* */
    {2478, 0x2724}, /* */
    {2479, 0x2735}, /* */
    {2480, 0x2758}, /* */
    {2483, 0x273C}, /* */
    {2486, 0x2759}, /* */
    {2488, 0x271B}, /* */
    {2490, 0x2742}, /* */
    {2492, 0x2745}, /* */
    {2493, 0x0040}, /* */
    {2497, 0x2704}, /* */
    {2498, 0x260E}, /* */
    {2499, 0x270E}, /* */
    {2500, 0x2761}, /* */
    {2502, 0x2725}, /* */
    {2509, 0x2710}, /* */
    {2513, 0x0023}, /* */
    {2514, 0x260F}, /* */
    {2515, 0x270D}, /* */
    {3812, 0xF000},		/* UGL, not Unicode */
    {65535, 0x0110}, /* Uppercase D-Stroke */ /* u */ /* undefined in MSL */
    {65535, 0x20A3}, /* French Franc */ /* u */
    {65535, 0x220F}, /* Product Symbol, Large Pi */ /* u */
};
#define pl_map_m2u_size countof(pl_map_m2u)

/* Map a MSL glyph code to a Unicode character code. */
/* Note that the symbol set is required, because some characters map */
/* differently in different symbol sets. */
uint
pl_map_MSL_to_Unicode(uint msl, uint symbol_set)
{	/* We should use binary search, but we don't.  This is VERY SLOW. */
	int i;
	for ( i = 0; i < pl_map_m2u_size; ++i )
	  if ( pl_map_m2u[i].key == msl )
	    return pl_map_m2u[i].value;
	return 0xffff;
}

/* Map a Unicode character code to a MSL glyph code similarly. */
uint
pl_map_Unicode_to_MSL(uint unicode, uint symbol_set)
{	/* Search the entire MSL to Unicode map.  This is VERY SLOW. */
	int i;
	for ( i = 0; i < pl_map_m2u_size; ++i )
	  if ( pl_map_m2u[i].value == unicode )
	    return pl_map_m2u[i].key;
	return 0xffff;
}
