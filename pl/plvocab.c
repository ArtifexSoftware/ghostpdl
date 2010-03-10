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

/* derived from: http://pcl.to/msl/mastermsl.txt */
static const pl_glyph_mapping_t pl_map_m2u[] = {
    {0, 0x0000},
    {1, 0x0021}, /* exclamation mark */ /* u */
    {2, 0x0022}, /* Neutral Double Quote */ /* u */
    {3, 0x0023}, /* Number Sign */ /* u */
    {4, 0x0024}, /* dollar sign */ /* u */
    {5, 0x0025}, /* Per Cent Sign */ /* u */
    {6, 0x0026}, /* ampersand */ /* u */
    {7, 0xF4CF},
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
    {65, 0xEFA7},
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
    {98, 0xF4C7},
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
    {210, 0x25D8}, /* Large Solid Sq/Open Dot */
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
    {228, 0x221F}, /* Tick Up at Left */
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
    { 282, 0xEFA3 },
    {283, 0x0393}, /* Uppercase Greek Gamma */  /* u */
    {284, 0x03C0}, /* Lowercase Greek Pi */  /* u */
    {285, 0x03A3}, /* Uppercase Greek Sigma */
    {286, 0x03C3}, /* Lowercase Greek Sigma */  /* u */
    {287, 0x03C4}, /* Lowercase Greek Tau */  /* u */
    {288, 0x03A6}, /* Uppercase Greek Phi */  /* u */
    {289, 0x0398}, /* Uppercase Greek Theta */  /* u */
    {290, 0x03A9}, /* Uppercase Omega, Ohms */ /* u */
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
    {305, 0x25AA}, /* Small Solid Box */
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
    {336, 0xEFA2},
    {338, 0x0166}, /* Uppercase T-Stroke */  /* u */
    {339, 0x0167}, /* Lowercase T-Stroke */  /* u */
    {340, 0x014A}, /* Uppercase Eng */  /* u */
    {341, 0x014B}, /* Lowercase Eng */  /* u */
    {342, 0x0111}, /* Lowercase D-Stroke */ /* u */
    {371, 0xEFA4},
    {372, 0xEFAA},
    {373, 0xF4C8},
    {400, 0x0102}, /* Uppercase A Breve */ /* u */
    {401, 0x0103}, /* Lowercase A Breve */ /* u */
    {402, 0x0100}, /* Uppercase A Macron */ /* u */
    {403, 0x0101}, /* Lowercase A Macron */ /* u */
    {404, 0x0104}, /* Uppercase A Ogonek */ /* u */
    {405, 0x0105}, /* Lowercase A Ogonek */ /* u */
    {406, 0x0106}, /* Uppercase C Acute */ /* u */
    {407, 0x0107}, /* Lowercase C Acute */ /* u */
    {408, 0xF4C4},
    {409, 0xF4CE},
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
    {500, 0xEFBF}, /* Radical Segment */
    {501, 0x221D}, /* Proportional To Symbol */
    {502, 0x212F}, /* Lowercase Italic E */
    {503, 0xEFEC}, /* Alternate LC Epsilon */
    {504, 0x2234}, /* Therefore */
    {505, 0x0393}, /* Uppercase Greek Gamma */
    {506, 0x0394}, /* Uppercase Greek Delta */ /* u */
    {507, 0x0398}, /* Uppercase Greek Theta */
    {508, 0x039B}, /* Uppercase Greek Lambda */
    {509, 0x039E}, /* Uppercase Greek Xi */
    {510, 0x03A0}, /* Uppercase Greek Pi */
    {511, 0x03A3}, /* Uppercase Greek Sigma */
    {512, 0x03A5}, /* Uppercase Greek Upsilon */
    {513, 0x03A6}, /* Uppercase Greek Phi */
    {514, 0x03A8}, /* Uppercase Greek Psi */
    {515, 0x03A9}, /* Uppercase Greek Omega */
    {516, 0x2207}, /* Nabla Symbol */
    {517, 0x2202}, /* Partial Diff Symbol */  /* u */
    {518, 0x03C2}, /* Terminal Symbol */
    {519, 0x2260}, /* Not Equal To Symbol */  /* u */
    {520, 0xEFEB}, /* Underline, Composite */
    {521, 0x2235}, /* Because */
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
    {551, 0x21D1}, /* Up Arrow Dbl Stroke */
    {552, 0x21D2}, /* Right Arrow Dbl Stroke */
    {553, 0x21D3}, /* Down Arrow Dbl Stroke */
    {554, 0x21D0}, /* Left Arrow Dbl Stroke */
    {555, 0x21D5}, /* Up/Dn Arrow Dbl Stroke */
    {556, 0x21D4}, /* Lft/Rt Arrow Dbl Stroke */
    {557, 0x21C4}, /* Right Over Left Arrow */
    {558, 0x21C6}, /* Left Over Right Arrow */ 
    {559, 0xEFE9}, /* Vector Symbol */
    {560, 0xEFEA}, /* Overline, Composite */
    {561, 0x2200}, /* For All Symbol */
    {562, 0x2203}, /* There Exists Symbol */
    {563, 0x22A4}, /* Top Symbol */
    {564, 0x22A5}, /* Bottom Symbol */
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
    {583, 0x2218}, /* Funct. Composition Sym */
    {584, 0x20DD}, /* Large Open Circle */
    {585, 0x22A3}, /* Assertion Symbol */
    {586, 0x22A2}, /* Backwards Assertion Symbol */
    {587, 0x222B}, /* Integral Symbol */  /* u */
    {588, 0x222E}, /* Curvilinear Integral Sym */
    {589, 0x2220}, /* Angle Symbol */
    {590, 0x2205}, /* Empty Set Symbol */
    {591, 0x2135}, /* Hebrew Aleph */
    {592, 0x2136}, /* Hebrew Beth */
    {593, 0x2137}, /* Hebrew Gimmel */
    {594, 0x212D}, /* Fraktur C */
    {595, 0x2111}, /* Fraktur I */
    {596, 0x211C}, /* Fraktur R */
    {597, 0x2128}, /* Fraktur Z */
    {598, 0x250C}, /* Top Left Bracket */
    {599, 0x2514}, /* Bottom Left Bracket */
    {600, 0xEFE3}, /* Top Left Brace */
    {601, 0xEFE2}, /* Middle Left Brace */
    {602, 0xEFE1}, /* Bottom Left Brace */
    {603, 0xEFD4}, /* Middle Curv. Integral */
    {604, 0xEFD3}, /* Top Left Summation */
    {605, 0x2225}, /* Dbl Verical Line, Comp */
    {606, 0xEFD2}, /* Bottom Left Summation */
    {607, 0xEFD1}, /* Bottom Diag. Summation */
    {608, 0x2510}, /* Top Right Bracket */
    {609, 0x2518}, /* Bottom Right Bracket */
    {610, 0xEFE0}, /* Top Right Brace */
    {611, 0xEFDF}, /* Middle Right Brace */
    {612, 0xEFDE}, /* Bottom Right Brace */
    {613, 0xEFDD}, /* Thick Vert Line, Comp */
    {614, 0x2223}, /* Thin Vert Line, Comp */
    {615, 0xEFDC}, /* Bottom Radical, Vert */
    {616, 0xEFD0}, /* Top Right Summation */
    {617, 0xEFCF}, /* Middle Summation */
    {618, 0xEFCE}, /* Bottom Right Summation */
    {619, 0xEFCD}, /* Top Diagonal Summation */
    {620, 0x2213}, /* Minus Over Plus Sign */
    {621, 0x2329}, /* Left Angle Bracket */
    {622, 0x232A}, /* Right Angle Bracket */
    {623, 0xEFFF}, /* Mask Symbol, Sup */
    {624, 0x2245}, /* Wavy/2 Straight Approx */
    {625, 0x2197}, /* 45 degree Arrow */
    {626, 0x2198}, /* -45 degree Arrow */
    {627, 0x2199}, /* -135 degree Arrow */
    {628, 0x2196}, /* 135 degree Arrow */
    {629, 0x25B5}, /* Up Open Triangle */
    {630, 0x25B9}, /* Right Open Triangle */
    {631, 0x25BF}, /* Down Open Triangle */
    {632, 0x25C3}, /* Left Open Triangle */
    {633, 0x226A}, /* Much Less Than Sign */
    {634, 0x226B}, /* Much Greater Than Sign */
    {635, 0x2237}, /* Proportional To Symbol */
    {636, 0xEFCA}, /* Defined As Symbol */
    {637, 0xEFD5}, /* LC Greek Digamma */
    {638, 0x210F}, /* Plank's Constant/2 pi */
    {639, 0x2112}, /* Laplace Transform Sym */
    {640, 0xEFFE}, /* Power Set Symbol */
    {641, 0x2118}, /* Weierstraussian Symbol */
    {642, 0x2211}, /* Summation Symbol, Sigma */  /* u */
    {643, 0x301A}, /* Left Double Bracket */
    {644, 0xEFC9}, /* Middle Dbl Bracket */
    {645, 0x301B}, /* Right Double Bracket */
    {646, 0x256D}, /* Lft Top Round Corner */
    {647, 0x2570}, /* Lft Bottom Round Corner */
    {648, 0xEFC8}, /* Ext Lrg Union/Product */
    {649, 0xEFC7}, /* Bottom Lrg Union */
    {650, 0xEFC6}, /* Top Large Intersection */
    {651, 0xEFC5}, /* Top Lft Dbl Bracket */
    {652, 0xEFC4}, /* Bottom Lft Dbl Bracket */
    {653, 0xEFFC}, /* Large Open Box */
    {654, 0x25C7}, /* Open Diamond */
    {655, 0x256E}, /* Rt Top Round Corner */
    {656, 0x256F}, /* Rt Bottom Round Corner */
    {657, 0xEFC3}, /* Bottom Lrg Bott Product */
    {658, 0xEFC2}, /* Top Large Top Product */
    {659, 0xEFC1}, /* Top Rt Dbl Bracket */
    {660, 0xEFC0}, /* Bottom Rt Dbl Bracket */
    {661, 0xEFFB}, /* Large Solid Box */
    {662, 0x25C6}, /* Solid Diamond */
    {663, 0x220D}, /* Such That */
    {664, 0x2217}, /* Math Asterisk Sign */
    {665, 0xEFE8}, /* Horizontal Arrow Ext */
    {666, 0xEFCB}, /* Dbl Horizontal Arrow Ext */
    {667, 0xEFCC}, /* Complement of #617 */
    {668, 0x221F}, /* Right Angle Symbol */
    {669, 0x220F}, /* N-ary Product */
    {683, 0x203E}, /* Triangular Bullet */
    {684, 0x25CA}, /* Lozenge */
    {685, 0xF4CB},
    {1000, 0x2070}, /* Superior Numeral 0 */
    {1001, 0x2074}, /* Superior Numeral 4 */
    {1002, 0x2075}, /* Superior Numeral 5 */
    {1003, 0x2076}, /* Superior Numeral 6 */
    {1004, 0x2077}, /* Superior Numeral 7 */
    {1005, 0x2078}, /* Superior Numeral 8 */
    {1006, 0x2079}, /* Superior Numeral 9 */
    {1009, 0xF4C1},
    {1010, 0xF4C2},
    {1011, 0xEFA5},
    {1012, 0xEFAE},
    {1013, 0xEFA6},
    {1015, 0xEFAF},
    {1017, 0x201C}, /* Double Open Quote (6) */ /* u */
    {1018, 0x201D}, /* Double Close Quote (9) */  /* u */
    {1019, 0x201E}, /* Dbl Baseline Quote (9) */  /* u */
    {1020, 0x2003}, /* Em Space */
    {1021, 0x2002}, /* En Space */
    {1023, 0x2009}, /* Thin Space */
    {1028, 0x2026}, /* Ellipsis */ /* u */
    {1030, 0x02DB}, /* Uppercase Ogonek */
    {1031, 0x017E}, /* Lowercase Z Hacek */ /* u */
    {1034, 0x2120}, /* Signature Mark, Sup */
    {1036, 0x211E}, /* Prescription Sign */
    {1040, 0xF001}, /* Lowercase FI Ligature */  /* u */
    {1041, 0xF002}, /* Lowercase FL Ligature */  /* u */
    {1042, 0xFB00}, /* Lowercase FF Ligature */ 
    {1043, 0xFB03}, /* Lowercase FFI Ligature */
    {1044, 0xFB04}, /* Lowercase ffl Ligature */
    {1045, 0x02DD}, /* Uppercase Dbl Acute */
    {1047, 0x0133}, /* Lowercase IJ Ligature */  /* u */
    {1060, 0x2105}, /* Care Of Symbol */  /* u */
    {1061, 0x011E}, /* Uppercase G Breve */ /* u */
    {1062, 0x011F}, /* Lowercase G Breve */ /* u */
    {1063, 0x015E}, /* Uppercase S Cedilla */ /* u */
    {1064, 0x015F}, /* Lowercase S Cedilla */ /* u */
    {1065, 0x0130}, /* Uppercase I Overdot */ /* u */
    {1066, 0x20A3}, /* French Franc */ /* u */
    {1067, 0x201A}, /* Sngl Baseline Quote (9) */ /* u */
    {1068, 0x2030}, /* Per Mill Sign */  /* u */
    {1069, 0x20AC}, /* Euro Sign */
    {1084, 0x02C9}, /* Lowercase Macron */  /* u */
    {1085, 0xEFEF}, /* Uppercase Macron */
    {1086, 0x02D8}, /* Lowercase Breve */  /* u */
    {1087, 0xEFEE}, /* Uppercase Breve */
    {1088, 0x02D9}, /* Lowercase Overdot */ /* u */
    {1089, 0xEFED}, /* Uppercase Overdot */
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
    {1100, 0x00AE}, /* Registered, Full, Serif */
    {1101, 0x00A9}, /* Copyright, Full, Serif */
    {1102, 0x2122}, /* Trademark, Full, Serif */
    {1103, 0x00AE}, /* Registered, Full, Sans */
    {1104, 0x00A9}, /* Copyright, Full, Sans */
    {1105, 0x2122}, /* Trademark, Full, Sans */
    {1106, 0x017D}, /* Uppercase Z Hacek */ /* u */
    {1107, 0x0132}, /* Uppercase IJ Ligature */  /* u */
    {1108, 0x25AB}, /* Small Open Box */
    {1109, 0x25E6}, /* Small Open Bullet */
    {1110, 0x25CB}, /* Medium Open Bullet */
    {1111, 0x25CF}, /* Large Solid Bullet */
    {1112, 0x2182}, /* Visible Carriage Return */
    {1113, 0x21E5}, /* Visible Tab */
    {1114, 0x2423}, /* Visible Space */
    {1115, 0x00AD}, /* Visible Soft Hyphen */
    {1116, 0xFFFF}, /* Visible End-of-File */
    {1409, 0x02D9}, /* Dot Above */
    {1450, 0xEFA8},
    {1451, 0xEFA9},
    {1452, 0xF4CA},
    {1453, 0xF4CC},
    {1454, 0xEFAC},
    {2000, 0x278A}, /* Dingbat, negative circled 1 */
    {2001, 0x27A1}, /* Black rightwards arrow */
    {2002, 0x278C}, /* Dingbat, negative circled 3 */
    {2003, 0x278D}, /* Dingbat, negative circled 4 */
    {2004, 0x278E}, /* Dingbat, negative circled 5 */
    {2005, 0x2790}, /* Dingbat, negative circled 7 */
    {2007, 0x2792}, /* Dingbat, negative circled 9 */
    {2008, 0x2793}, /* Dingbat, negative circled 10 */
    {2009, 0x2791}, /* Dingbat, negative circled 8 */
    {2010, 0x2713}, /* Checkmark */
    {2011, 0x2727}, /* White 4-pointed star */
    {2013, 0x2708}, /* Airplane */
    {2014, 0x2722}, /* Four teardrop-spoked asterisk */
    {2015, 0x2789}, /* Dingbat, circled 10 */
    {2016, 0x2780}, /* Dingbat, circled 1 */
    {2017, 0x2781}, /* Dingbat, circled 2 */
    {2018, 0x2782}, /* Dingbat, circled 3 */
    {2019, 0x2783}, /* Dingbat, circled 4 */
    {2020, 0x2784}, /* Dingbat, circled 5 */
    {2021, 0x2785}, /* Dingbat, circled 6 */
    {2022, 0x2786}, /* Dingbat, circled 7 */
    {2023, 0x2787}, /* Dingbat, circled 8 */
    {2024, 0x2788}, /* Dingbat, circled 9 */
    {2027, 0x2717}, /* Ballot X */
    {2030, 0x00A7}, /* Section Sign */
    {2031, 0x278B}, /* Dingbat, negative circled 2 */
    {2032, 0x272C}, /* Black center white star */
    {2037, 0x279C}, /* Heavy round-tipped rightwards arrow */
    {2041, 0x27A7},
    {2044, 0x271D},
    {2046, 0x2736},
    {2047, 0x272B},
    {2048, 0x25B2},
    {2050, 0x2020},
    {2052, 0x272A},
    {2053, 0x279B},
    {2055, 0x27A5},
    {2058, 0x2730},
    {2059, 0x25EF},
    {2060, 0x272D},
    {2061, 0x278F},
    {2064, 0x2606},
    {2070, 0x279D},
    {2071, 0x279E},
    {2072, 0x2749},
    {2076, 0x271E},
    {2077, 0x2718},
    {2078, 0x272E},
    {2080, 0x2533},
    {2083, 0x2605},
    {2087, 0x27A6},
    {2088, 0x2729},
    {2089, 0x2756},
    {2090, 0x2737},
    {2091, 0x25A0},
    {2092, 0x272F},
    {2096, 0x2702},
    {2100, 0x275B},
    {2104, 0x27BD},
    {2105, 0x27B8},
    {2108, 0x2709},
    {2109, 0x2701},
    {2113, 0x275C},
    {2116, 0x274D},
    {2120, 0x2703},
    {2121, 0x2712},
    {2200, 0x2776},
    {2202, 0x2778},
    {2203, 0x2779},
    {2204, 0x277A},
    {2205, 0x277C},
    {2207, 0x277E},
    {2208, 0x277F},
    {2209, 0x277D},
    {2210, 0x2714},
    {2212, 0x27BE},
    {2213, 0x2765},
    {2215, 0x2469},
    {2216, 0x2460},
    {2217, 0x2461},
    {2218, 0x2462},
    {2219, 0x2463},
    {2220, 0x2464},
    {2221, 0x2465},
    {2222, 0x2466},
    {2223, 0x2467},
    {2224, 0x2468},
    {2227, 0xEFAB},
    {2228, 0xEFA1},
    {2229, 0xF4C0},
    {2230, 0x2720},
    {2231, 0x2777},
    {2232, 0x274B},
    {2233, 0x2798},
    {2234, 0x27B3},
    {2235, 0x27A4},
    {2240, 0x2738},
    {2242, 0x27BC},
    {2245, 0x27B7},
    {2248, 0x25BC},
    {2249, 0x25D6},
    {2250, 0x2751},
    {2252, 0x2732},
    {2253, 0x27BB},
    {2254, 0x2716},
    {2256, 0x2731},
    {2258, 0x2743},
    {2260, 0x2747},
    {2261, 0x277B},
    {2262, 0x27A8},
    {2263, 0x00A9},
    {2264, 0x274A},
    {2265, 0x27B4},
    {2267, 0x27A2},
    {2272, 0x2739},
    {2278, 0x273D},
    {2279, 0x273B},
    {2280, 0x25BD},
    {2282, 0x274F},
    {2283, 0x2733},
    {2285, 0x27BA},
    {2286, 0x2715},
    {2287, 0x27B5},
    {2288, 0x2741},
    {2290, 0x273E},
    {2292, 0x2748},
    {2293, 0x00AE},
    {2295, 0x261E},
    {2300, 0x2767},
    {2305, 0x2799},
    {2306, 0x2794},
    {2307, 0x270F},
    {2308, 0x2752},
    {2309, 0x27B9},
    {2310, 0x279A},
    {2312, 0x261B},
    {2313, 0x270C},
    {2315, 0x2706},
    {2317, 0x2766},
    {2324, 0x2711},
    {2325, 0x2750},
    {2326, 0x27A3},
    {2327, 0x27B6},
    {2410, 0x2763},
    {2411, 0x2721},
    {2412, 0x27B2},
    {2414, 0x2764},
    {2428, 0x2762},
    {2432, 0x2746},
    {2441, 0x27A9},
    {2442, 0x27AB},
    {2443, 0x27AD},
    {2444, 0x2719},
    {2447, 0x2734},
    {2448, 0x2726},
    {2451, 0x273F},
    {2452, 0x273A},
    {2454, 0x275A},
    {2456, 0x271C},
    {2458, 0x2707},
    {2460, 0x2740},
    {2464, 0x2744},
    {2472, 0x2723},
    {2475, 0x27AF},
    {2476, 0x271F},
    {2478, 0x2724},
    {2479, 0x2735},
    {2480, 0x2758},
    {2483, 0x273C},
    {2484, 0x2733},
    {2486, 0x2759},
    {2488, 0x271B},
    {2490, 0x2742},
    {2492, 0x2745},
    {2493, 0x0040},
    {2497, 0x2704},
    {2498, 0x260E},
    {2499, 0x270E},
    {2500, 0x2761},
    {2501, 0x275D},
    {2502, 0x2725},
    {2504, 0x27A0},
    {2507, 0x27AC},
    {2508, 0x27AE},
    {2509, 0x2710},
    {2513, 0x0023},
    {2514, 0x260F},
    {2515, 0x270D},
    {2517, 0x275E},
    {2520, 0x279F},
    {2523, 0x27AA},
    {2524, 0x27B1},
    {2612, 0x0391},
    {2613, 0x0392},
    {2616, 0x0395},
    {2617, 0x0396},
    {2618, 0x0397},
    {2620, 0x0399},
    {2621, 0x039A},
    {2623, 0x039C},
    {2624, 0x039D},
    {2626, 0x039F},
    {2628, 0x03A1},
    {2630, 0x03A4},
    {2633, 0x03A7},
    {2680, 0xEFAD},
    {2681, 0xF4C3},
    {2682, 0xF4C5},
    {2726, 0x2726},
    {3487, 0xEFA0},
    {3488, 0xF4C9},
    {3489, 0xF4CD},
    {3490, 0xF4C6},
    {3812, 0xF000}
};
#define pl_map_m2u_size countof(pl_map_m2u)

/* inverse of previous map */
static const pl_glyph_mapping_t pl_map_u2m[] = {
    {0x0020,       0}, /* space         */
    {0x0021,       1}, /* exclam        */
    {0x0022,       2}, /* quotedbl      */
    {0x0023,       3}, /* numbersign    */
    {0x0024,       4}, /* dollar        */
    {0x0025,       5}, /* percent       */
    {0x0026,       6}, /* ampersand     */
    {0x0027,     329}, /* quotesingle or 8 */
    {0x0028,       9}, /* parenleft     */
    {0x0029,      10}, /* parenright    */
    {0x002A,      11}, /* asterisk      */
    {0x002B,      12}, /* plus          */
    {0x002C,      13}, /* comma         */
    {0x002D,      14}, /* hyphen        */
    {0x002E,      15}, /* period        */
    {0x002F,      16}, /* slash         */
    {0x0030,      17}, /* zero          */
    {0x0031,      18}, /* one           */
    {0x0032,      19}, /* two           */
    {0x0033,      20}, /* three         */
    {0x0034,      21}, /* four          */
    {0x0035,      22}, /* five          */
    {0x0036,      23}, /* six           */
    {0x0037,      24}, /* seven         */
    {0x0038,      25}, /* eight         */
    {0x0039,      26}, /* nine          */
    {0x003A,      27}, /* colon         */
    {0x003B,      28}, /* semicolon     */
    {0x003C,      29}, /* less          */
    {0x003D,      30}, /* equal         */
    {0x003E,      31}, /* greater       */
    {0x003F,      32}, /* question      */
    {0x0040,      33}, /* at            */
    {0x0041,      34}, /* A             */
    {0x0042,      35}, /* B             */
    {0x0043,      36}, /* C             */
    {0x0044,      37}, /* D             */
    {0x0045,      38}, /* E             */
    {0x0046,      39}, /* F             */
    {0x0047,      40}, /* G             */
    {0x0048,      41}, /* H             */
    {0x0049,      42}, /* I             */
    {0x004A,      43}, /* J             */
    {0x004B,      44}, /* K             */
    {0x004C,      45}, /* L             */
    {0x004D,      46}, /* M             */
    {0x004E,      47}, /* N             */
    {0x004F,      48}, /* O             */
    {0x0050,      49}, /* P             */
    {0x0051,      50}, /* Q             */
    {0x0052,      51}, /* R             */
    {0x0053,      52}, /* S             */
    {0x0054,      53}, /* T             */
    {0x0055,      54}, /* U             */
    {0x0056,      55}, /* V             */
    {0x0057,      56}, /* W             */
    {0x0058,      57}, /* X             */
    {0x0059,      58}, /* Y             */
    {0x005A,      59}, /* Z             */
    {0x005B,      60}, /* bracketleft   */
    {0x005C,      61}, /* backslash     */
    {0x005D,      62}, /* bracketright  */
    {0x005E,      63}, /* asciicircum   */
    {0x005F,      64}, /* underscore    */
    {0x0060,     107}, /* grave         */
    {0x0061,      67}, /* a             */
    {0x0062,      68}, /* b             */
    {0x0063,      69}, /* c             */
    {0x0064,      70}, /* d             */
    {0x0065,      71}, /* e             */
    {0x0066,      72}, /* f             */
    {0x0067,      73}, /* g             */
    {0x0068,      74}, /* h             */
    {0x0069,      75}, /* i             */
    {0x006A,      76}, /* j             */
    {0x006B,      77}, /* k             */
    {0x006C,      78}, /* l             */
    {0x006D,      79}, /* m             */
    {0x006E,      80}, /* n             */
    {0x006F,      81}, /* o             */
    {0x0070,      82}, /* p             */
    {0x0071,      83}, /* q             */
    {0x0072,      84}, /* r             */
    {0x0073,      85}, /* s             */
    {0x0074,      86}, /* t             */
    {0x0075,      87}, /* u             */
    {0x0076,      88}, /* v             */
    {0x0077,      89}, /* w             */
    {0x0078,      90}, /* x             */
    {0x0079,      91}, /* y             */
    {0x007A,      92}, /* z             */
    {0x007B,      93}, /* braceleft     */
    {0x007C,      94}, /* bar           */
    {0x007D,      95}, /* braceright    */
    {0x007E,      96}, /* asciitilde    */
    {0x00A0,       0}, /* No Break Space*/
    {0x00A1,     121}, /* exclamdown    */
    {0x00A2,     128}, /* cent          */
    {0x00A3,     124}, /* sterling      */
    {0x00A4,     123}, /* currency      */
    {0x00A5,     125}, /* yen           */
    {0x00A6,     192}, /* brokenbar     */
    {0x00A7,     126}, /* section       */
    {0x00A8,     109}, /* dieresis      */
    {0x00A9,     193}, /* copyright...  (various) */
    {0x00AA,     186}, /*               */
    {0x00AB,     188}, /* guillemotleft */
    {0x00AC,     194}, /* logicalnot    */
    {0x00AD,     195}, /* hyphen        */
    {0x00AE,     196}, /* registered (also 1103 & 1100) */
    {0x00AF,     113}, /* overline, also matches 1084 lc macron */
    {0x00B0,     116}, /* degree        */
    {0x00B1,     191}, /* plusminus     */
    {0x00B2,     197}, /* twosuperior   */
    {0x00B3,     198}, /* threesuperior */
    {0x00B4,     106}, /* acute         */
    {0x00B5,     180}, /* mu & 533 mu3  */
    {0x00B6,     181}, /* paragraph     */
    {0x00B7,     179}, /* periodcentered & 302 middot */
    {0x00B8,     199}, /* cedilla & 330 */
    {0x00B9,     200}, /* onesuperior   */
    {0x00BA,     187}, /*               */
    {0x00BB,     190}, /* guillemotright */
    {0x00BC,     184}, /* onequarter    */
    {0x00BD,     185}, /* onehalf       */
    {0x00BE,     182}, /* threequarters */
    {0x00BF,     122}, /* questiondown  */
    {0x00C0,      99}, /* Agrave        */
    {0x00C1,     161}, /* Aacute        */
    {0x00C2,     100}, /* Acircumflex   */
    {0x00C3,     162}, /* Atilde        */
    {0x00C4,     153}, /* Adieresis     */
    {0x00C5,     145}, /* Aring         */
    {0x00C6,     148}, /* AE            */
    {0x00C7,     117}, /* Ccedilla      */
    {0x00C8,     101}, /* Egrave        */
    {0x00C9,     157}, /* Eacute        */
    {0x00CA,     102}, /* Ecircumflex   */
    {0x00CB,     103}, /* Edieresis     */
    {0x00CC,     167}, /* Igrave        */
    {0x00CD,     166}, /* Iacute        */
    {0x00CE,     104}, /* Icircumflex   */
    {0x00CF,     105}, /* Idieresis     */
    {0x00D0,     164}, /* Eth see also 0x0110 */
    {0x00D1,     119}, /* Ntilde        */
    {0x00D2,     169}, /* Ograve        */
    {0x00D3,     168}, /* Oacute        */
    {0x00D4,     160}, /* Ocircumflex   */
    {0x00D5,     170}, /* Otilde        */
    {0x00D6,     155}, /* Odieresis     */
    {0x00D7,     201}, /* multiply      */
    {0x00D8,     147}, /* Oslash        */
    {0x00D9,     111}, /* Ugrave        */
    {0x00DA,     174}, /* Uacute        */
    {0x00DB,     112}, /* Ucircumflex   */
    {0x00DC,     156}, /* Udieresis     */
    {0x00DD,     114}, /* Yacute        */
    {0x00DE,     177}, /* Thorn         */
    {0x00DF,     159}, /* germandbls    */
    {0x00E0,     137}, /* agrave        */
    {0x00E1,     133}, /* aacute        */
    {0x00E2,     129}, /* acircumflex   */
    {0x00E3,     163}, /* atilde        */
    {0x00E4,     141}, /* adieresis     */
    {0x00E5,     149}, /* aring         */
    {0x00E6,     152}, /* ae            */
    {0x00E7,     118}, /* ccedilla      */
    {0x00E8,     138}, /* egrave        */
    {0x00E9,     134}, /* eacute        */
    {0x00EA,     130}, /* ecircumflex   */
    {0x00EB,     142}, /* edieresis     */
    {0x00EC,     154}, /* igrave        */
    {0x00ED,     150}, /* iacute        */
    {0x00EE,     146}, /* icircumflex   */
    {0x00EF,     158}, /* idieresis     */
    {0x00F0,     165}, /* eth           */
    {0x00F1,     120}, /* ntilde        */
    {0x00F2,     139}, /* ograve        */
    {0x00F3,     135}, /* oacute        */
    {0x00F4,     131}, /* ocircumflex   */
    {0x00F5,     171}, /* otilde        */
    {0x00F6,     143}, /* odieresis     */
    {0x00F7,     202}, /* divide        */
    {0x00F8,     151}, /* oslash        */
    {0x00F9,     140}, /* ugrave        */
    {0x00FA,     136}, /* uacute        */
    {0x00FB,     132}, /* ucircumflex   */
    {0x00FC,     144}, /* udieresis     */
    {0x00FD,     115}, /* yacute        */
    {0x00FE,     178}, /* thorn         */
    {0x00FF,     176}, /* ydieresis     */
    {0x0100,     402}, /* Amacron (NEW) */
    {0x0101,     403}, /* amacron (NEW) */
    {0x0102,     400}, /* Abreve        */
    {0x0103,     401}, /* abreve        */
    {0x0104,     404}, /* Aogonek       */
    {0x0105,     405}, /* aogonek       */
    {0x0106,     406}, /* Cacute        */
    {0x0107,     407}, /* cacute        */
    {0x010C,     410}, /* Ccaron        */
    {0x010D,     411}, /* ccaron        */
    {0x010E,     414}, /* Dcaron        */
    {0x010F,     415}, /* dcaron        */
    {0x0110,     164}, /* Eth see also 0x00D0 */
    {0x0111,     342}, /* dmacron       */
    {0x0112,     420}, /* Emacron (NEW) */
    {0x0113,     421}, /* emacron (NEW) */
    {0x0116,     418}, /* Edot (NEW)    */
    {0x0117,     419}, /* edot (NEW)    */
    {0x0118,     422}, /* Eogonek       */
    {0x0119,     423}, /* eogonek       */
    {0x011A,     416}, /* Ecaron        */
    {0x011B,     417}, /* ecaron        */
    {0x011E,    1061}, /* Gbreve        */
    {0x011F,    1062}, /* gbreve        */
    {0x0122,     428}, /* Gcommaaccent (NEW) */
    {0x0123,     429}, /* gcommaaccent (NEW) */
    {0x0128,     486}, /* Itilde        */
    {0x0129,     487}, /* itilde        */
    {0x012A,     434}, /* Imacron (NEW) */
    {0x012B,     435}, /* imacron (NEW) */
    {0x012E,     432}, /* Iogonek (NEW) */
    {0x012F,     433}, /* iogonek (NEW) */
    {0x0130,    1065}, /* Idot          */
    {0x0131,     328}, /* dotlessi      */
    {0x0132,    1107}, /* IJ            */
    {0x0133,    1047}, /* ij            */
    {0x0136,     438}, /* Kcomma (NEW)  */
    {0x0137,     439}, /* kcomma (NEW)  */
    {0x0138,     335}, /* kra (NEW)     */
    {0x0139,     440}, /* Lacute        */
    {0x013A,     441}, /* lacute        */
    {0x013B,     444}, /* Lcomma (NEW)  */
    {0x013C,     445}, /* lcomma (NEW)  */
    {0x013D,     442}, /* Lcaron        */
    {0x013E,     443}, /* lcaron        */
    {0x013F,     306}, /* Ldot          */
    {0x0140,     307}, /* ldot          */
    {0x0141,    1095}, /* Lslash        */
    {0x0142,    1096}, /* lslash        */
    {0x0143,     446}, /* Nacute        */
    {0x0144,     447}, /* nacute        */
    {0x0145,     450}, /* Ncomma (NEW)  */
    {0x0146,     451}, /* ncomma (NEW)  */      
    {0x0147,     448}, /* Ncaron        */
    {0x0148,     449}, /* ncaron        */
    {0x0149,     309}, /* apostrophe_n  */
    {0x014A,     340}, /* Eng (NEW)     */
    {0x014B,     341}, /* eng (NEW)     */
    {0x014C,     454}, /* Omacron (NEW) */
    {0x014D,     455}, /* omacron (NEW) */
    {0x0150,     452}, /* Odblacute     */
    {0x0151,     453}, /* odblacute     */
    {0x0152,    1091}, /* OE            */
    {0x0153,    1090}, /* oe            */
    {0x0154,     456}, /* Racute        */
    {0x0155,     457}, /* racute        */
    {0x0156,     460}, /* Rcomma (NEW)  */
    {0x0157,     461}, /* rcomma (NEW)  */
    {0x0158,     458}, /* Rcaron        */
    {0x0159,     459}, /* rcaron        */
    {0x015A,     462}, /* Sacute        */
    {0x015B,     463}, /* sacute        */
    {0x015E,    1063}, /* Scedilla      */
    {0x015F,    1064}, /* scedilla      */
    {0x0160,     172}, /* Scaron        */
    {0x0161,     173}, /* scaron        */
    {0x0162,     468}, /* Tcedilla      */
    {0x0163,     469}, /* tcedilla      */
    {0x0164,     466}, /* Tcaron        */
    {0x0165,     467}, /* tcaron        */
    {0x0166,     338}, /* Tbar (NEW)    */
    {0x0167,     339}, /* tbar (NEW)    */
    {0x0168,     470}, /* Utilde (NEW)  */
    {0x0169,     471}, /* utilde (NEW)  */
    {0x016A,     478}, /* Umacron (NEW) */
    {0x016B,     479}, /* umacron (NEW) */
    {0x016E,     476}, /* Uring         */
    {0x016F,     477}, /* uring         */
    {0x0170,     474}, /* Udblacute     */
    {0x0171,     475}, /* udblacute     */
    {0x0172,     480}, /* Uogonek (NEW) */
    {0x0173,     481}, /* uogonek (NEW) */
    {0x0178,     175}, /* Ydieresis     */
    {0x0179,     482}, /* Zacute        */
    {0x017A,     483}, /* zacute        */
    {0x017B,     484}, /* Zoverdot      */
    {0x017C,     485}, /* zoverdot      */
    {0x017D,    1106}, /* Zcaron        */
    {0x017E,    1031}, /* zcaron        */
    {0x0192,     127}, /* florin        */
    {0x02C6,     108}, /* circumflex    */
    {0x02C7,     315}, /* caron         */
    {0x02C9,    1084}, /* macron        */
    {0x02D8,    1086}, /* breve         */
    {0x02D9,    1088}, /* dotaccent     */
    {0x02D9,    1409}, /* dotaccent     */
    {0x02DA,     316}, /* ring          */
    {0x02DB,    1098}, /* ogonek & 1425, 1412, 1030 */
    {0x02DC,     110}, /* tilde         */
    {0x02DD,    1097}, /* hungarumlaut & was 1407 (NEW)*/
    {0x0391,    2612}, /* Alpha         */  /* Prefer Math, use Greek if nec. */
    {0x0392,    2613}, /* Beta          */
    {0x0393,     283}, /* Gamma or 505, or Greek */
    {0x0394,     506}, /* Delta         */
    {0x0395,    2616}, /* Epsilon       */
    {0x0396,    2617}, /* Zeta          */
    {0x0397,    2618}, /* Eta           */
    {0x0398,     507}, /* Theta or 507 or Greek */
    {0x0399,    2620}, /* Iota          */
    {0x039A,    2621}, /* Kappa         */
    {0x039B,     508}, /* Lambda        */
    {0x039C,    2623}, /* Mu            */
    {0x039D,    2624}, /* Nu            */
    {0x039E,     509}, /* Xi            */
    {0x039F,    2626}, /* Omicron       */
    {0x03A0,     510}, /* Pi            */
    {0x03A1,    2628}, /* Rho           */
    {0x03A3,     511}, /* Sigma         */
    {0x03A4,    2630}, /* Tau           */
    {0x03A5,     512}, /* Upsilon       */
    {0x03A6,     288}, /* Phi or 513 or Greek */
    {0x03A7,    2633}, /* Chi           */
    {0x03A8,     514}, /* Psi           */
    {0x03A9,     290}, /* Omega or 515  */
    {0x03B1,     281}, /* alpha or 522  */
    {0x03B2,     523}, /* beta          */
    {0x03B3,     524}, /* gamma         */
    {0x03B4,     291}, /* delta or 525  */
    {0x03B5,     294}, /* epsilon or 503 or 526 or Greek */
    {0x03B6,     527}, /* zeta          */
    {0x03B7,     528}, /* eta           */
    {0x03B8,     529}, /* theta         */
    {0x03B9,     530}, /* iota          */
    {0x03BA,     531}, /* kappa         */
    {0x03BB,     532}, /* lambda        */
    {0x03BC,     533}, /* mu            */
    {0x03BD,     534}, /* nu            */
    {0x03BE,     535}, /* xi (NEW) fix  */
    {0x03BF,     536}, /* omicron       */
    {0x03C0,     284}, /* pi or 537     */
    {0x03C1,     538}, /* rho           */
    {0x03C2,     518}, /* sigma1 - term */
    {0x03C3,     286}, /* sigma or 539  */
    {0x03C4,     287}, /* tau or 540    */
    {0x03C5,     541}, /* upsilon       */
    {0x03C6,     293}, /* phi or 542    */
    {0x03C7,     543}, /* chi           */
    {0x03C8,     544}, /* psi           */
    {0x03C9,     545}, /* omega         */
    {0x03D1,     546}, /* theta1        */
    {0x03D2,     512}, /* Upsilon1      */
    {0x03D5,     547}, /* phi1          */
    {0x03D6,     548}, /* omega1        */
    {0x2002,    1021}, /* en space (NEW) */
    {0x2003,    1020}, /* em space (NEW) */
    {0x2009,    1023}, /* thin space (NEW) */
    {0x2013,     326}, /* endash        */
    {0x2014,     325}, /* emdash        */
    {0x2017,     314}, /* dbl underline */
    {0x2018,      66}, /* quoteleft     */
    {0x2019,       8}, /* quoteright    */
    {0x201A,    1067}, /* quotesinglbase */
    {0x201C,    1017}, /* quotedblleft  */
    {0x201D,    1018}, /* quotedblright */
    {0x201E,    1019}, /* quotedblbase  */
    {0x2020,     312}, /* dagger        */
    {0x2021,     327}, /* daggerdbl     */
    {0x2022,     331}, /* bullet        */
    {0x2026,    1028}, /* ellipsis      */
    {0x2030,    1068}, /* perthousand   */
    {0x2032,     310}, /* minute        */
    {0x2033,     311}, /* second        */
    {0x2039,    1092}, /* guilsinglleft */
    {0x203A,    1093}, /* guilsinglright */
    {0x203C,     221}, /* dblexclam     */
    {0x203E,     683}, /* radicalex     */
    {0x2044,     324}, /* slash (NEW) */
    {0x2070,    1000}, /* superior0 (NEW)    */
    {0x2074,    1001}, /* superior4 (NEW)    */
    {0x2075,    1002}, /* superior5 (NEW)    */
    {0x2076,    1003}, /* superior6 (NEW)    */
    {0x2077,    1004}, /* superior7 (NEW)    */
    {0x2078,    1005}, /* superior8 (NEW)    */
    {0x2079,    1006}, /* superior9 (NEW)    */
    {0x207F,     332}, /* nsuperior (NEW)    */
    {0x20A3,    1066}, /* franc - NOT FOUND IN MSLs FOR LJ4 YET */
    {0x20A7,     232}, /* peseta        */
    {0x20AC,    1069}, /* EUR symbol */
    {0x20DD,     584}, /* combiningcircle (NEW) */
    {0x2105,    1060}, /* careof        */
    {0x210F,     638}, /* planckconstant (NEW)*/
    {0x2111,     595}, /* Ifraktur      */
    {0x2112,     639}, /* scriptL (NEW) */
    {0x2113,     308}, /* script l      */
    {0x2118,     641}, /* scriptP (NEW) */
    {0x211C,     596}, /* Rfraktur      */
    {0x211E,    1036}, /* prescription  (NEW) */
    {0x2120,    1034}, /* servicemark  (NEW) */
    {0x2122,     313}, /* trademark or 1102 or 1105 */
    {0x2126,     290}, /* Omega         */
    {0x2128,     597}, /* blackletterZ (NEW) */
    {0x212D,     594}, /* blackletterC (NEW) */
    {0x212F,     502}, /* scripte (NEW) */
    {0x2135,     591}, /* aleph         */
    {0x2136,     592}, /* betsymbol (NEW) */
    {0x2137,     593}, /* gimelsymbol (NEW) */
    {0x2182,    1112}, /* arrowleft     */
    {0x2190,     227}, /* arrowleft     */
    {0x2191,     224}, /* arrowup       */
    /*    {0x2192,    2042}, Why was this here ?*/ /* a161   <- indicates a dingbat, PostScript num? */
    {0x2192,     226}, /* arrowright    */
    {0x2193,     225}, /* arrowdown     */
    {0x2194,     229}, /* arrowboth     */
    /* {0x2194,    2043}, */ /* a163          */
    {0x2195,     220}, /* up_down_arrow */
    /* {0x2195,    2075}, */ /* a164          */
    {0x2196,     628}, /* nw arrow  (NEW) */
    {0x2197,     625}, /* ne arrow  (NEW) */
    {0x2198,     626}, /* se arrow  (NEW) */
    {0x2199,     627}, /* ww arrow  (NEW) */
    {0x21A8,     223}, /* updownunderarrow */
    {0x21B5,    1099}, /* carriagereturn */
    {0x21C4,     557}, /* rightoverleftarrows (NEW) */
    {0x21C6,     558}, /* leftoverrightarrows (NEW) */
    {0x21D0,     554}, /* arrowdblleft  */
    {0x21D1,     551}, /* arrowdblup    */
    {0x21D2,     552}, /* arrowdblright */
    {0x21D3,     553}, /* arrowdbldown  */
    {0x21D4,     556}, /* arrowdblboth  */
    {0x21D5,     555}, /* arrowdblbvertboth (NEW) */
    {0x21E5,    1113}, /* arrowdblbvertboth (NEW) */
    {0x2200,     561}, /* universal     */
    {0x2202,     517}, /* partialdiff   */
    {0x2203,     562}, /* existential   */
    {0x2205,     590}, /* emptyset      */
    {0x2206,     506}, /* Delta or 98   */
    {0x2207,     516}, /* gradient      */
    {0x2208,     566}, /* element       */
    {0x2209,     568}, /* notelement    */
    {0x220B,     567}, /* suchthat      */
    {0x220D,     663}, /* small member (NEW) */
    /*    {0x220F,     658}, */ /* producttop    */
    {0x220F,     669}, /* producttop  (NEW)  */
    /*{0x2211,     285}, */ /* Sigma or 642  */
    {0x2211,     642}, /* Sigma or 642 (NEW) */
    {0x2212,     183}, /* minus         */
    {0x2213,     620}, /* minusoverplus (NEW) */
    {0x2215,     324}, /* fraction      */
    {0x2217,     664}, /* asteriskmath  */
    /*   {0x2218,     641}, */ /* weierstrass   */
    {0x2218,     583}, /* ringoperator (NEW) */
    {0x2219,     302}, /* middot or 1427*/
    {0x221A,     303}, /* radical       */
    {0x221D,     501}, /* proportional  */
    {0x221E,     292}, /* infinity      */
    /* {0x221F,     228}, */ /* Tick up left  */
    {0x221F,     668}, /* rightangle (NEW)  */
    {0x2220,     589}, /* angle         */
    {0x2223,     614}, /* divides (NEW) */
    {0x2225,     605}, /* parallelto (NEW) */
    {0x2227,     580}, /* logicaland    */
    {0x2228,     581}, /* logicalor     */
    {0x2229,     295}, /* intersection  */
    {0x222A,     565}, /* union         */
    {0x222B,     587}, /* integral      */
    {0x222E,     588}, /* contourintegral (NEW) */
    {0x2234,     504}, /* therefore     */
    {0x2235,     521}, /* because (NEW) */
    {0x2237,     635}, /* proportion (NEW) */
 /* 0x223C, UNKNOWN,    similar       */
    {0x2243,     549}, /* asymptoticallyequalto (NEW) */
    {0x2245,     624}, /* congruent     */
    {0x2248,     301}, /* approxequal   */
    {0x2260,     519}, /* notequal      */
    {0x2261,     296}, /* equivalence   */
    {0x2262,     550}, /* notidenticalto (NEW) */
    {0x2264,     298}, /* lessequal     */
    {0x2265,     297}, /* greaterequal  */
    {0x226A,     633}, /* muchlessthan (NEW) */
    {0x226B,     634}, /* muchgreaterthan (NEW) */
    {0x2282,     569}, /* propersubset  */
    {0x2283,     570}, /* propersuperset */
    {0x2284,     571}, /* notsubset     */
    {0x2285,     572}, /* notasupersetof (NEW) */
    {0x2286,     573}, /* reflexsubset  */
    {0x2287,     574}, /* reflexsuperset */
    {0x2295,     575}, /* circleplus    */
    {0x2296,     578}, /* circleminus (NEW) */
    {0x2297,     577}, /* circlemultiply */
    {0x2298,     579}, /* circledivide (NEW) */
    {0x2299,     576}, /* circledot (NEW) */
    {0x22A2,     586}, /* righttack (NEW) */
    {0x22A3,     585}, /* lefttack (NEW) */
    {0x22A4,     563}, /* downtack (NEW) */
    {0x22A5,     564}, /* perpendicular */
    {0x22BB,     582}, /* XOR (NEW)     */
    {0x22C5,     302}, /* dotmath       */
    {0x2302,     333}, /* home_plate    */
    {0x2310,     233}, /* tick down left*/
    {0x2320,     299}, /* integraltp    */
    {0x2321,     300}, /* integralbt    */
    {0x2329,     621}, /* angleleft     */
    {0x232A,     622}, /* angleright    */
    {0x2423,    1114}, /* a120          */
    {0x2460,    2216}, /* a120          */
    {0x2461,    2217}, /* a121          */
    {0x2462,    2218}, /* a122          */
    {0x2463,    2219}, /* a123          */
    {0x2464,    2220}, /* a124          */
    {0x2465,    2221}, /* a125          */
    {0x2466,    2222}, /* a126          */
    {0x2467,    2223}, /* a127          */
    {0x2468,    2224}, /* a128          */
    {0x2469,    2215}, /* a129          */
    {0x2500,     253}, /* line-draw semi-graphics follow */
    {0x2502,     236}, /* names omitted */
    {0x250C,     275}, /*     :         */
    {0x2510,     248}, /*     :         */
    {0x2514,     249}, /*     :         */
    {0x2518,     274}, /*     :         */
    {0x251C,     252}, /*     :         */
    {0x2524,     237}, /*     :         */
    {0x252C,     251}, /*     :         */
    {0x2534,     250}, /*     :         */
    {0x253C,     254}, /*     :         */
    {0x2550,     262}, /*     :         */
    {0x2551,     243}, /*     :         */
    {0x2552,     270}, /*     :         */
    {0x2553,     271}, /*     :         */
    {0x2554,     258}, /*     :         */
    {0x2555,     241}, /*     :         */
    {0x2556,     240}, /*     :         */
    {0x2557,     244}, /*     :         */
    {0x2558,     269}, /*     :         */
    {0x2559,     268}, /*     :         */
    {0x255A,     257}, /*     :         */
    {0x255B,     247}, /*     :         */
    {0x255C,     246}, /*     :         */
    {0x255D,     245}, /*     :         */
    {0x255E,     255}, /*     :         */
    {0x255F,     256}, /*     :         */
    {0x2560,     261}, /*     :         */
    {0x2561,     238}, /*     :         */
    {0x2562,     239}, /*     :         */
    {0x2563,     242}, /*     :         */
    {0x2564,     266}, /*     :         */
    {0x2565,     267}, /*     :         */
    {0x2566,     260}, /*     :         */
    {0x2567,     264}, /*     :         */
    {0x2568,     265}, /*     :         */
    {0x2569,     259}, /*     :         */
    {0x256A,     273}, /*     :         */
    {0x256B,     272}, /*     :         */
    {0x256C,     263}, /*     :         */
    {0x256D,     646}, /* arcupperleft (NEW) */
    {0x256E,     655}, /* arcupperright (NEW) */
    {0x256F,     656}, /* arclowerright (NEW) */
    {0x2570,     647}, /* arclowerleft (NEW) */
    {0x2580,     280}, /* top_half_cell */
    {0x2584,     277}, /* bottom_half_cell */
    {0x2588,     276}, /* solid_cell    */
    {0x258C,     278}, /* left_half_cell  */
    {0x2590,     279}, /* right_half_cell */
    {0x2591,     234}, /* quarter_fill  */
    {0x2592,      97}, /* half_fill     */
    {0x2593,     235}, /* threeq_fill   */
    {0x25A0,     189}, /* medium box     */
    {0x25A1,    1094}, /* opensquare    */
    {0x25AA,     305}, /* small box     */
    {0x25AB,    1108}, /* enboxopen     */
    {0x25AC,     222}, /* candybar      */
    {0x25B2,     230}, /* up_triangle   */
    {0x25B5,     629}, /* upopentriange (NEW) */
    {0x25B9,     630}, /* rightopentriangle (NEW) */
    {0x25BA,     218}, /* right_triangle*/
    {0x25BC,     231}, /* down_triangle */
    {0x25BF,     631}, /* downopentriangle (NEW) */
    {0x25C3,     632}, /* leftopentriange (NEW) */
    {0x25C4,     219}, /* left_triangle */
    /*{0x25C6,    2491},*/ /* a78           */
    {0x25C6,     662}, /* blackdiamond (NEW) */
    {0x25C7,     654}, /* opendiamond (NEW) */
    {0x25CA,     684}, /* lozenge (NEW) fixed */
    {0x25CB,    1110}, /* circle or 584 or 1110 */
    {0x25CF,     209}, /* ball          */
    {0x25D8,     210}, /* reverse_ball  */
    {0x25D9,     212}, /* reverse_circle*/
    {0x25E6,    1109}, /* encircleopen  */
    {0x2605,    2083}, /* a35           */
    {0x260E,    2498}, /* a4            */
    {0x261B,    2312}, /* a11           */
    {0x261E,    2295}, /* a12           */
    {0x263A,     203}, /* smiley_face   */
    {0x263B,     204}, /* shiney_face   */
    {0x263C,     217}, /* sphincter     */
    {0x2640,     214}, /* female ordinal*/
    {0x2642,     213}, /* male ordinal  */
    {0x2660,     208}, /* spade         */
    {0x2663,     207}, /* club          */
    {0x2665,     205}, /* heart         */
    {0x2666,     206}, /* diamond       */
    {0x266A,     215}, /* eightnote     */
    {0x266B,     216}, /* dblsixteenthnote */
    {0x2701,    2109}, /* a1 Scissors   */
    {0x2702,    2096}, /* a2            */
    {0x2703,    2120}, /* a202          */
    {0x2704,    2497}, /* a3            */
    {0x2706,    2315}, /* a5            */
    {0x2707,    2458}, /* a119          */
    {0x2708,    2013}, /* a118          */
    {0x2709,    2108}, /* a117          */
    {0x270C,    2313}, /* a13           */
    {0x270D,    2515}, /* a14           */
    {0x270E,    2499}, /* a15           */
    {0x270F,    2307}, /* a16           */
    {0x2710,    2509}, /* a105          */
    {0x2711,    2324}, /* a17           */
    {0x2712,    2121}, /* a18           */
    {0x2713,    2010}, /* a19           */
    {0x2714,    2210}, /* a20           */
    {0x2715,    2286}, /* a21           */
    {0x2716,    2254}, /* a22           */
    {0x2717,    2027}, /* a23           */
    {0x2718,    2077}, /* a24           */
    {0x2719,    2444}, /* a25           */
 /* 0x271A, UNKNOWN, a26 Probably some sort of plus sign */
    {0x271B,    2488}, /* a27           */
    {0x271C,    2456}, /* a28           */
    {0x271D,    2044}, /* a6            */
    {0x271E,    2076}, /* a7            */
    {0x271F,    2476}, /* a8            */
    {0x2720,    2230}, /* a9            */
    {0x2721,    2411}, /* a10           */
    {0x2722,    2014}, /* a29           */
    {0x2723,    2472}, /* a30           */
    {0x2724,    2478}, /* a31           */
    {0x2725,    2502}, /* a32           */
    {0x2726,    2726}, /* a33           */
    {0x2727,    2011}, /* a34           */
    {0x2729,    2088}, /* a36           */
    {0x272A,    2052}, /* a37           */
    {0x272B,    2047}, /* a38           */
    {0x272C,    2032}, /* a39           */
    {0x272D,    2060}, /* a40           */
    {0x272E,    2078}, /* a41           */
    {0x272F,    2092}, /* a42           */
    {0x2730,    2058}, /* a43           */
    {0x2731,    2256}, /* a44           */
    {0x2732,    2252}, /* a45           */
    {0x2733,    2484}, /* a46           */
    {0x2734,    2447}, /* a47           */
    {0x2735,    2479}, /* a48           */
    {0x2736,    2046}, /* a49           */
    {0x2737,    2090}, /* a50           */
    {0x2738,    2240}, /* a51           */
    {0x2739,    2272}, /* a52           */
    {0x273A,    2452}, /* a53           */
    {0x273B,    2279}, /* a54           */
    {0x273C,    2483}, /* a55           */
    {0x273D,    2278}, /* a56           */
    {0x273E,    2290}, /* a57           */
    {0x273F,    2451}, /* a58           */
    {0x2740,    2460}, /* a59           */
    {0x2741,    2288}, /* a60           */
    {0x2742,    2490}, /* a61           */
    {0x2743,    2258}, /* a62           */
    {0x2744,    2464}, /* a63           */
    {0x2745,    2492}, /* a64           */
    {0x2746,    2432}, /* a65           */
    {0x2747,    2260}, /* a66           */
    {0x2748,    2292}, /* a67           */
    {0x2749,    2072}, /* a68           */
    {0x274A,    2264}, /* a69           */
    {0x274B,    2232}, /* a70           */
    {0x274D,    2116}, /* a72           */
    {0x274F,    2282}, /* a74           */
    {0x2750,    2325}, /* a203          */
    {0x2751,    2250}, /* a75           */
    {0x2752,    2308}, /* a204          */
    {0x2756,    2089}, /* a79           */
    {0x2758,    2480}, /* a82           */
    {0x2759,    2486}, /* a83           */
    {0x275A,    2454}, /* a84           */
    {0x275B,    2100}, /* a97           */
    {0x275C,    2113}, /* a98           */
    {0x275D,    2501}, /* a99           */
    {0x275E,    2517}, /* a100          */
    {0x2761,    2500}, /* a101          */
    {0x2762,    2428}, /* a102          */
    {0x2763,    2410}, /* a103          */
    {0x2764,    2414}, /* a104          */
    {0x2765,    2213}, /* a106          */
    {0x2766,    2317}, /* a107          */
    {0x2767,    2300}, /* a108          */
    {0x2776,    2200}, /* a130          */
    {0x2777,    2231}, /* a131          */
    {0x2778,    2202}, /* a132          */
    {0x2779,    2203}, /* a133          */
    {0x277A,    2204}, /* a134          */
    {0x277B,    2261}, /* a135          */
    {0x277C,    2205}, /* a136          */
    {0x277D,    2209}, /* a137          */
    {0x277E,    2207}, /* a138          */
    {0x277F,    2208}, /* a139          */
    {0x2780,    2016}, /* a140          */
    {0x2781,    2017}, /* a141          */
    {0x2782,    2018}, /* a142          */
    {0x2783,    2019}, /* a143          */
    {0x2784,    2020}, /* a144          */
    {0x2785,    2021}, /* a145          */
    {0x2786,    2022}, /* a146          */
    {0x2787,    2023}, /* a147          */
    {0x2788,    2024}, /* a148          */
    {0x2789,    2015}, /* a149          */
    {0x278A,    2000}, /* a150          */
    {0x278B,    2031}, /* a151          */
    {0x278C,    2002}, /* a152          */
    {0x278D,    2003}, /* a153          */
    {0x278E,    2004}, /* a154          */
    {0x278F,    2061}, /* a155          */
    {0x2790,    2005}, /* a156          */
    {0x2791,    2009}, /* a157          */
    {0x2792,    2007}, /* a158          */
    {0x2793,    2008}, /* a159          */
    {0x2794,    2306}, /* a160          */
    {0x2798,    2233}, /* a196          */
    {0x2799,    2305}, /* a165          */
    {0x279A,    2310}, /* a192          */
    {0x279B,    2053}, /* a166          */
    {0x279C,    2037}, /* a167          */
    {0x279D,    2070}, /* a168          */
    {0x279E,    2071}, /* a169          */
    {0x279F,    2520}, /* a170          */
    {0x27A0,    2504}, /* a171          */
    {0x27A1,    2001}, /* a172          */
    {0x27A2,    2267}, /* a173          */
    {0x27A3,    2326}, /* a162          */
    {0x27A4,    2235}, /* a174          */
    {0x27A5,    2055}, /* a175          */
    {0x27A6,    2087}, /* a176          */
    {0x27A7,    2041}, /* a177          */
    {0x27A8,    2262}, /* a178          */
    {0x27A9,    2441}, /* a179          */
    {0x27AA,    2523}, /* a193          */
    {0x27AB,    2442}, /* a180          */
    {0x27AC,    2507}, /* a199          */
    {0x27AD,    2443}, /* a181          */
    {0x27AE,    2508}, /* a200          */
    {0x27AF,    2475}, /* a182          */
    {0x27B1,    2524}, /* a201          */
    {0x27B2,    2412}, /* a183          */
    {0x27B3,    2234}, /* a184          */
    {0x27B4,    2265}, /* a197          */
    {0x27B5,    2287}, /* a185          */
    {0x27B6,    2327}, /* a194          */
    {0x27B7,    2245}, /* a198          */
    {0x27B8,    2105}, /* a186          */
    {0x27B9,    2309}, /* a195          */
    {0x27BA,    2285}, /* a187          */
    {0x27BB,    2253}, /* a188          */
    {0x27BC,    2242}, /* a189          */
    {0x27BD,    2104}, /* a190          */
    {0x27BE,    2212}, /* a191          */


    {0x301A,     643}, /* (NEW) */
    {0x301B,     645}, /* (NEW) */

    {0xEFA0,    3487}, /* (NEW) */
    {0xEFA1,    2228}, /* (NEW) */
    {0xEFA2,     336}, /* (NEW) */
    {0xEFA3,     282}, /* (NEW) */
    {0xEFA4,     371}, /* (NEW) */
    {0xEFA5,    1011}, /* (NEW) */
    {0xEFA6,    1013}, /* (NEW) */
    {0xEFA7,      65}, /* (NEW) */
    {0xEFA8,    1450}, /* (NEW) */
    {0xEFA9,    1451}, /* (NEW) */
    {0xEFAA,     372}, /* (NEW) */
    {0xEFAB,    2227}, /* (NEW) */
    {0xEFAC,    1454}, /* (NEW) */
    {0xEFAD,    2680}, /* (NEW) */
    {0xEFAE,    1012}, /* (NEW) */
    {0xEFAF,    1015}, /* (NEW) */

    {0xEFBF,     500}, /* (NEW) */

    {0xEFC0,     660}, /* (NEW) */
    {0xEFC1,     659}, /* (NEW) */
    {0xEFC2,     658}, /* (NEW) */
    {0xEFC3,     657}, /* (NEW) */
    {0xEFC4,     652}, /* (NEW) */
    {0xEFC5,     651}, /* (NEW) */
    {0xEFC6,     650}, /* (NEW) */
    {0xEFC7,     649}, /* (NEW) */
    {0xEFC8,     648}, /* (NEW) */
    {0xEFC9,     644}, /* (NEW) */
    {0xEFCA,     636}, /* (NEW) */
    {0xEFCB,     666}, /* (NEW) */
    {0xEFCC,     667}, /* (NEW) */
    {0xEFCD,     619}, /* (NEW) */
    {0xEFCE,     618}, /* (NEW) */
    {0xEFCF,     617}, /* (NEW) */

    {0xEFD0,     616}, /* (NEW) */
    {0xEFD1,     607}, /* (NEW) */
    {0xEFD2,     606}, /* (NEW) */
    {0xEFD3,     604}, /* (NEW) */
    {0xEFD4,     603}, /* (NEW) */
    {0xEFD5,     637}, /* (NEW) */
    {0xEFD6,    1105}, /* (NEW) */
    {0xEFD7,    1104}, /* (NEW) */
    {0xEFD8,    1103}, /* (NEW) */
    {0xEFD9,    1102}, /* (NEW) */
    {0xEFDA,    1101}, /* (NEW) */
    {0xEFDB,    1100}, /* (NEW) */
    {0xEFDC,     615}, /* (NEW) */
    {0xEFDD,     613}, /* (NEW) */
    {0xEFDE,     612}, /* (NEW) */
    {0xEFDF,     611}, /* (NEW) */

    {0xEFE0,     610}, /* (NEW) */
    {0xEFE1,     602}, /* (NEW) */
    {0xEFE2,     601}, /* (NEW) */
    {0xEFE3,     600}, /* (NEW) */
    {0xEFE4,     609}, /* (NEW) */
    {0xEFE5,     608}, /* (NEW) */
    {0xEFE6,     599}, /* (NEW) */
    {0xEFE7,     598}, /* (NEW) */
    {0xEFE8,     665}, /* (NEW) */
    {0xEFE9,     559}, /* (NEW) */
    {0xEFEA,     560}, /* (NEW) */
   {0xEFEB,     520}, /* (NEW) */
    {0xEFEC,     503}, /* (NEW) */
    {0xEFED,    1089}, /* (NEW) */
    {0xEFEE,    1087}, /* (NEW) */
    {0xEFEF,    1085}, /* (NEW) */

    {0xEFF0,    1045}, /* (NEW) */
    {0xEFF1,    1030}, /* (NEW) */
    {0xEFF2,     330}, /* (NEW) */
    {0xEFF3,     323}, /* (NEW) */
    {0xEFF4,     322}, /* (NEW) */
    {0xEFF5,     321}, /* (NEW) */
    {0xEFF6,     320}, /* (NEW) */
    {0xEFF7,     319}, /* (NEW) */
    {0xEFF8,     318}, /* (NEW) */
    {0xEFF9,     317}, /* (NEW) */

    {0xEFFA,    1111}, /* (NEW) */
    {0xEFFB,     661}, /* (NEW) */
    {0xEFFC,     653}, /* (NEW) */
    {0xEFFD,     211}, /* (NEW) */


    {0xEFFE,     640}, /* (NEW) */
    {0xEFFF,     623}, /* (NEW) */

    {0xF000,    3812}, /* apple         */
    {0xF001,    1040}, /* fi            */
    {0xF002,    1041},  /* fl            */

    {0xF4C0,    2229}, /* (NEW) */
    {0xF4C1,    1009}, /* (NEW) */
    {0xF4C2,    1010}, /* (NEW) */
    {0xF4C3,    2681}, /* (NEW) */
    {0xF4C4,     408}, /* (NEW) */
    {0xF4C5,    2682}, /* (NEW) */
    {0xF4C6,    3490}, /* (NEW) */
    {0xF4C7,      98}, /* (NEW) */
    {0xF4C8,     373}, /* (NEW) */
    {0xF4C9,    3488}, /* (NEW) */
    {0xF4CA,    1452}, /* (NEW) */
    {0xF4CB,     685}, /* (NEW) */
    {0xF4CC,    1453}, /* (NEW) */
    {0xF4CD,    3489}, /* (NEW) */
    {0xF4CE,     409}, /* (NEW) */
    {0xF4CF,       7}, /* (NEW) */

    {0xFB00,    1042},  /* ff (NEW)            */
    {0xFB01,    1040},  /* fi (NEW)           */
    {0xFB02,    1041},  /* fl (NEW)           */
    {0xFB03,    1043},  /* ffi (NEW)           */
    {0xFB04,    1044}   /* ffl (NEW)           */
};

#define pl_map_u2m_size countof(pl_map_u2m)
/* Map a Unicode character code to a MSL glyph code similarly. */
uint pl_map_Unicode_to_MSL(uint uni, uint symbol_set)
{
    uint low = 0;
    uint high = pl_map_u2m_size - 1;
    uint mid;

    /* A symbol map entry of 0xFFFF indicates that no printable
     * symbol is associated with that character code.
     * In this case, the LJ4 prints a blank space.
     */

    if(uni == 0xffff)
        return 0x0020;

     if ((uni < pl_map_u2m[low].key) || (uni > pl_map_u2m[high].key))
        return 0xffff;

    /* Binary search, right out of K&R */
    while (low <= high)
    {
        mid = (low+high) / 2;
        if (uni < pl_map_u2m[mid].key)
            high = mid - 1;
        else if (uni > pl_map_u2m[mid].key)
            low = mid + 1;
        else
            return (uint)pl_map_u2m[mid].value;
    }
    return 0xffff;
}
/* Map a MSL glyph code to a Unicode character code. */
/* Note that the symbol set is required, because some characters map */
/* differently in different symbol sets. */
uint pl_map_MSL_to_Unicode(uint msl, uint symbol_set)
{
    uint low = 0;
    uint high = pl_map_m2u_size - 1;
    uint mid;

    /* Binary search, right out of K&R */
    while (low <= high)
    {
        mid = (low+high) / 2;
        if (msl < pl_map_m2u[mid].key)
            high = mid - 1;
        else if (msl > pl_map_m2u[mid].key)
            low = mid + 1;
        else
            return (uint)pl_map_m2u[mid].value;
    }
    return 0xffff;
}
