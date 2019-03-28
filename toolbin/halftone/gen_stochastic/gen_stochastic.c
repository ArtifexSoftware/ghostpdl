/* Copyright (C) 2001-2019 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  1305 Grant Avenue - Suite 200, Novato,
   CA 94945, U.S.A., +1(415)492-9861, for further information.
*/

/*
 *	Program to generate a stochastic threshold array that has good edge
 *	blending and high frequency spatial distribution.
 *
 *  usage:  gen_stochastic [ options ] SIZEWxSIZEH outfile [ -g ghostscript args ...]
 *
 *	    SIZEWxSIZEH are the width and height of the threshold array separated
 *	    by a lower case 'x'. If the threshold array is square, then only the
 *	    first number is required and the 'x' should not be present. Maximum
 *	    value is 512 (MAX_ARRAY_WIDTH, MAX_ARRAY_HEIGHT compile time option).
 *
 *	    'outfile' is the name of the threshold array file.
 *
 *	options are any combination of the following:
 *
 *	-m#	set the minimum dot size/shape pattern. This is an index
 *		to a specific size/shape table as follows:
 *
 *		0:	1:	2:	3:	4:	5:
 *		x	xx	x	xx	x	xx
 *				x	x	 x	xx
 *
 *		6:	7:	8:	9:	10:	11:	12:	13:
 *		xxx	xxx	xxx	xxx	xxx	xxx	xxx	xxx
 *		   	x  	xx 	xxx	x  	xx 	xxx	xxx
 *		   	   	   	   	x  	x 	x 	xx
 *
 *		14:	15:	16:	17:	18:	11:	13:	19:
 *		x  	xx 	xx 	xx 	xxx	xxx	xxx	xxx
 *		x   	x  	xx 	xx 	xx 	xx	xxx	xxx
 *		x   	x   	x   	xx	xx	x  	xx	xxx
 *
 *		Note that the duplicated indices for duplicated patterns are
 *		for clarity. Also, some patterns are intentionally omitted e.g.,
 *			x	xx	xxx
 *				x	xx
 *			x	xx	xxx
 *
 *	-p#.##  power for exponential bias of random choice. Default 2.0
 *
 *	-q	Quiet mode (default verbose).
 *
 *	-rWxH	allows for horizontal / vertical resolution, e.g. -r2x1
 *		values are used for aspect ratio -- actual values arbitrary
 *
 *	-s#	Initial seed for random number generation. Useful to generate
 *		decorrelated threshold arrays to be used with different colors.
 *
 *	-t#	sets the choice value threshold in 0.1% units (default 10 = 1%)
 *
 *	-v      verbose mode. Details to stdout about choices. Default OFF
 *
 *	If the build #defined USE_GSDLL, then a graphical display will be shown
 *	as the choices are made. Three sections of the page show the pseudo-color
 *	map of densities, the expanded view of the pixels, and the 1:1 pixel view
 *	of 3 tiles wide by 3 tiles tall, allowing interactive examination of the
 *	choices. Ghostscript is used to generate this display and any Ghostscript
 *	parameters may follow the '-g' trailing option. This option is last so
 *	that the number of gs args is variable.
 *
 * 	-g 	followed by ghostscript args are only used if built with the
 *		USE_GSDLL option and must follow the SIZE and outfile parameters.
 *		All parameters are then passed to gs to control the display.
 *
 *	when paused between values, <ret> for next value, 'g' or <esc> for
 *	continuous or a character to repeat c-0x20 times (! = 1, " = 2, ...)
 *	'q' for exit.
 *
*/


/*
 *	Outline.
 *	  1. Clear the array.
 *	  2. Generate array of positions ordered by increasing density.
 *	     The density is determined as a result of the "ValFunction"
 *	     function. This function determines the weighting of pixels.
 *	  3. Choose the next array element using random variable index
 *	     into the "Val".
 *
 *	Rev A.	Rather than re-compute the entire array "Val" from scratch
 *		at every level, since densities only increase due to the
 *		previous point, simply add the incremental value due to
 *		the latest point to every "Val".
 *
 *		NOTE that if more involved "Val" functions are used to
 *		try to detect "lines" in the array and increase the value
 *		for points that would form lines, then it will probably
 *		be necessary to recalculate values for the entire array.
 *		(but maybe not even then -- just keep it in mind)
 *
 *	Rev B.	Add support for differing horizontal and vertical resolutions
 *		and make the 'threshold' for including values in the 'choice'
 *		set a parameter. Also support threshold arrays of differing
 *		width and height.
 *
 *	Rev C.	Add support for 'minimum dot' (-m option)
 *
 *	Rev D.	Add support for multi-bit threshold arrays (-n#)
*/

#define	MAX_ARRAY_WIDTH		512
#define	MAX_ARRAY_HEIGHT	512

#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#ifdef __WIN32__
#   include <conio.h>
#endif /* __WIN32__ */

#define BIG_FLOAT	999999999.0

/******** GLOBALS ********/
int	array_width, array_height, resolution[2];
int	ThresholdArray[MAX_ARRAY_WIDTH][MAX_ARRAY_HEIGHT];
int	quiet = 0;

FILE *fp;

double	Val[MAX_ARRAY_WIDTH * MAX_ARRAY_HEIGHT];

double	MinVal, MaxVal, ValRange;

typedef struct Order_t {
    int X;
    int Y;
} Order_s;

Order_s Order[MAX_ARRAY_WIDTH * MAX_ARRAY_HEIGHT];

/* Forward references */
int	do_dot(int choice_X, int choice_Y, int level, int last);
int	CompareOrder(const void *, const void *);
double	ValFunction(int thisX, int thisY, int refX, int refY, double rx_sq, double ry_sq);

#ifdef USE_GSDLL
int init_gs_display(int argc, char **argv);
int update_gs_image(void);
int update_gs_dot(int choice_X, int choice_Y, int last);
void close_gs_display(void);
#endif

/* Definition of the minimum dot patterns */
static struct min_dot_edge {
    int	num_rows;
    int	left[3];
    int	right[3];
} min_dot_edges[20] = {
    /*		0:	1:	2:	3:	4:	5:
     *		x	xx	x	xx	x	xx
     *				x	x	 x	xx
     */
    { 1, { 0, 0, 0 }, { 0, 0, 0 } },	/* 0:		x	*/

    { 1, { 0, 0, 0 }, { 1, 0, 0 } },	/* 1:		xx	*/

    { 2, { 0, 0, 0 }, { 0, 0, 0 } },	/* 2:		x	*/
                                            /*		x	*/

    { 2, { 0, 0, 0 }, { 1, 0, 0 } },	/* 3:		xx	*/
                                            /*		x	*/

    { 2, { 0, 1, 0 }, { 0, 1, 0 } },	/* 4:		x	*/
                                            /*		 x	*/

    { 2, { 0, 0, 0 }, { 1, 1, 0 } },	/* 5:		xx	*/
                                            /*		xx	*/

    /*		6:	7:	8:	9:	10:	11:	12:
     *		xxx	xxx	xxx	xxx	xxx	xxx	xxx
     *		   	x  	xx 	xxx	x  	xx 	xxx
     *		   	   	   	   	x  	x 	x
     */
    { 1, { 0, 0, 0 }, { 2, 0, 0 } },	/* 6:		xxx	*/


    { 2, { 0, 0, 0 }, { 2, 0, 0 } },	/* 7:		xxx	*/
                                            /*		x 	*/

    { 2, { 0, 0, 0 }, { 2, 1, 0 } },	/* 8:		xxx	*/
                                            /*		xx	*/

    { 2, { 0, 0, 0 }, { 2, 2, 0 } },	/* 9:		xxx	*/
                                            /*		xxx	*/

    { 3, { 0, 0, 0 }, { 2, 0, 0 } },	/* 10:		xxx	*/
                                            /*		x	*/
                                            /*		x	*/

    { 3, { 0, 0, 0 }, { 2, 1, 0 } },	/* 11:		xxx	*/
                                            /*		x	*/
                                            /*		x	*/

    { 3, { 0, 0, 0 }, { 2, 2, 0 } },	/* 12:		xxx	*/
                                            /*		xxx	*/
                                            /*		x	*/

    /*		13:	14:	15:	16:	17:	18:	19:
     *		xxx	x  	xx 	xx 	xx 	xxx	xxx
     *		xxx	x   	x  	xx 	xx 	xx 	xxx
     *		xx	x   	x   	x   	xx  	xx	xxx
     */

    { 3, { 0, 0, 0 }, { 2, 2, 1 } },	/* 13:		xxx	*/
                                            /*		xxx	*/
                                            /*		xx	*/

    { 3, { 0, 0, 0 }, { 0, 0, 0 } },	/* 14:		x	*/
                                            /*		x	*/
                                            /*		x	*/

    { 3, { 0, 0, 0 }, { 1, 0, 0 } },	/* 15:		xx	*/
                                            /*		x	*/
                                            /*		x	*/

    { 3, { 0, 0, 0 }, { 1, 1, 0 } },	/* 16:		xx	*/
                                            /*		xx	*/
                                            /*		x	*/

    { 3, { 0, 0, 0 }, { 1, 1, 1 } },	/* 17:		xx	*/
                                            /*		xx	*/
                                            /*		xx	*/

    { 3, { 0, 0, 0 }, { 2, 1, 1 } },	/* 18:		xxx	*/
                                            /*		xx	*/
                                            /*		xx	*/

    { 3, { 0, 0, 0 }, { 2, 2, 1 } }		/* 19:		xxx	*/
                                            /*		xxx	*/
                                            /*		xxx	*/
};


/**************************************************************************/

int
main(int argc, char *argv[])
{
    /*	Initialize master threshold array	*/
    int		i, j, k, m, level, level_up = 1;
    int		X, Y, choice_range, choice, choice_X, choice_Y;
    int		SortRange;
    int		min_dot_pattern = 0, do_min_dot;
    double	value, val_thresh = 0.01;	/* default -t10 */
    double	rx_sq = 1.0, ry_sq = 1.0;
    double	rand_scaled, bias_power = 2.0;
    float	x;

    int	gsarg_start;
    int code = 0, at_arg;

    resolution[0] = resolution[1] = 1;

    /* process the optional arguments */
    for (at_arg=1; at_arg<argc; at_arg++) {
        if (argv[at_arg][0] != '-') {
            break;				/* next is SIZE */
        } else if (argv[at_arg][1] == 'm') {
            j = sscanf(&argv[at_arg][2], "%d", &m);
            if (j < 1)
                goto usage_exit;
            min_dot_pattern = m;
        } else if (argv[at_arg][1] == 'p') {
            j = sscanf(&argv[at_arg][2], "%f", &x);
            if (j < 1)
                goto usage_exit;
            bias_power = x;
        } else if (argv[at_arg][1] == 'q') {
                    quiet = 1;
        } else if (argv[at_arg][1] == 'r') {
            /* resolution wwwxhhh */
            j = sscanf(&argv[at_arg][2], "%dx%d", &k, &m);
            if (j < 1)
                goto usage_exit;
            resolution[0] = k;
            if (j > 1)
                resolution[1] = m;
            rx_sq = resolution[0] * resolution[0];
            ry_sq = resolution[1] * resolution[1];
        } else if (argv[at_arg][1] = 's') {
            /* iseed value */
            j = sscanf(&argv[at_arg][2], "%d", &k);
            if (j != 0) {
                srand(k);
            }
        } else if (argv[at_arg][1] == 't') {
            /* threshold percent */
            j = sscanf(&argv[at_arg][2], "%d", &k);
            if (j < 1)
                goto usage_exit;
            val_thresh = (double)k / 1000.0;
        }
    }

    /* Initialize array_width and height from the next command line arg */
    /* format SSS (width and height equal or WWWxHHH for array */
    if (at_arg == argc)
        goto usage_exit;

    j = sscanf(argv[at_arg++], "%dx%d", &k, &m);
    if (j < 1)
        goto usage_exit;

    array_width = k;
    array_height = k;
    if (j > 1)
        array_height = m;

    if (array_width * array_height > MAX_ARRAY_WIDTH * MAX_ARRAY_HEIGHT) {
        printf("Array size is too large, max width = %d, max height = %d\n",
                MAX_ARRAY_WIDTH, MAX_ARRAY_HEIGHT);
        return 1;
    }

    /* And finally open the output file from the next required parameter */
    if (at_arg == argc)
        goto usage_exit;

    if ((fp = fopen(argv[at_arg++],"w")) == NULL)
        goto usage_exit;

    /* The -g is actually optional, but don't pass it to gs */
    gsarg_start = at_arg;
    if (at_arg < argc && argv[at_arg][0] == '-' && argv[at_arg][1] == 'g') {
        gsarg_start++;
    }
#ifdef USE_GSDLL
    if ((code=init_gs_display(argc - gsarg_start, argv+gsarg_start)) < 0)
        return 1;
#else
    if (gsarg_start < argc)
        printf("\nThis build did not specify -dUSE_GSDLL. -g args ignored and there will be no display.\n");
#endif

    /* Write out the header line for the threshold array */
    /* This should be compatible with 'thresh_remap.c' */
    fprintf(fp,"# W=%d H=%d\n", array_width, array_height);

    /* Initialize the ThresholdArray to -1 (an invalid value) for unfilled dots. */
    /* Initialize the Order array */
    for (Y=0; Y < array_height; Y++) {
        for (X=0; X < array_width; X++) {
            Order[ Y*array_width + X ].X = X;
            Order[ Y*array_width + X ].Y = Y;
            Val[ Y*array_width + X ] = 0.0;
            ThresholdArray[X][Y] = -1;
        }
    }
    /* Create an ordered list of values */
    SortRange = (array_width*array_height);
    MinVal = 0.0;
    MaxVal = 0.0;
    ValRange = 1.0;

    for (level = 0; level < (array_width * array_height); level += level_up) {

        /* We focus the processing on the first "SortRange" number of */
        /* elements to speed up the processing. The SortRange starts  */
        /* at the full array size, then is reduced to a smaller value */

        /* Create an ordered list of values */
        qsort((void *)Order, SortRange, sizeof(Order_s), CompareOrder);
        SortRange = array_width * array_height - level;

#ifdef USE_GSDLL
        /* display an array of values in pseudo color */
        if ((code=update_gs_image()) < 0)
            return 1;
#endif
        if (! quiet) {
            printf("MinVal = %f, MinX = %d, MinY = %d\n", MinVal, Order[0].X, Order[0].Y);
        }

        /* Print some statistics on the ordered array */
        choice_range = 0;
        for (i=0; i < (array_width * array_height) - level; i++) {
            value = Val[ (Order[i].Y * array_width) + Order[i].X ];
            value = (value-MinVal) / ValRange;
            if (value > val_thresh)
                break;
            choice_range++;
        }
        if (! quiet)
            printf("Number of points less than %5.3f = %d\n", val_thresh, choice_range);

        /* Now select the next pixel using a random number */

        /* Limit the choice to the 1/10 of the total number of points	*/
        /* or those points less than "val_thresh"	*/
        /* whichever is smaller	*/
        if (choice_range > array_width*array_height/10)
            choice_range = array_width*array_height/10;

        /* Choose from among the 'acceptable' points */
        rand_scaled = (double)rand() / (double)RAND_MAX;
        choice = (int)((double)choice_range * pow(rand_scaled, bias_power));
        choice_X = Order[choice].X;
        choice_Y = Order[choice].Y;

        /* if minimum dot size is set, modify the choice depending on the */
        /* neighboring dots. If the edge of the expanded dot is adajcent  */
        /* to a dot aleady 'on', then increase the size of that dot instead */
        do_min_dot = min_dot_pattern;
        level_up = 1;                   /* set for the default, single dot case */
        if (min_dot_pattern != 0) {
            int row, dot, cX, cY;
            int row_direction, dot_direction;
            int userow;

            /* Scan the area covered by this dot, including above and below by	*/
            /* one row, and to the left and to the right by one dot. If one 	*/
            /* marked dot is found, choose a single dot adjacent to the marked  */
            /* dot.                                                             */
            for (row=-1; row <= min_dot_edges[min_dot_pattern].num_rows; row++) {
                /* for the left and right edges, we use a row within the num_rows range */
                userow = row < 0 ? 0 :	/* top row of the min_dot_pattern	*/
                             row < min_dot_edges[min_dot_pattern].num_rows ?
                                 row:	     /* current row is within numrows	*/
                                 row - 1;    /* last row of min_dot_pattern	*/
                cY = (choice_Y + row + array_height) % array_height;
                for (dot=min_dot_edges[min_dot_pattern].left[userow] - 1;
                        dot <= min_dot_edges[min_dot_pattern].right[userow] + 1; dot++) {
                    cX = (choice_X + dot + array_height) % array_width;
                    if (ThresholdArray[cX][cY] != -1)
                        goto find_neighbor;
                }
            }
            goto do_dot;		/* we have room for a minimum dot, do it */

find_neighbor:
            /* Found an adjacent dot that is already used, select an unused	*/
            /* single dot contiguous to the dot that is used		*/
            do_min_dot = 0;			/* select a single dot	*/
            if (!quiet)
                printf("min_dot at [%d, %d] suppressed due to neighbor dot at: [%d, %d]\n",
                choice_X, choice_Y, cX, cY);
            /* Choose a white dot adjacent to this dot, closest to our initial */
            /* choice position.                                                */
            if (row < min_dot_edges[min_dot_pattern].num_rows >> 1)
                row_direction = 1;		/* go down from the marked dot found */
            else
                row_direction = -1;		/* go above the marked dot */
            if (dot < min_dot_edges[min_dot_pattern].right[userow] >> 1)
                dot_direction = 1;		/* move right */
            else
                dot_direction = -1;		/* move left */
            if (!quiet)
                printf("searching for unmarked dot %s and to the %s\n",
                   row_direction < 0 ? "above" : "below",
                   dot_direction < 0 ? "left" : "right");
            if ((choice_X & 1) == 0) {
                /* even columns are column major */
                for (; (row >= -1) && (row <= min_dot_edges[min_dot_pattern].num_rows);
                     row += row_direction) {
                   userow = row < 0 ? 0 :	/* top row of the min_dot_pattern	*/
                               row < min_dot_edges[min_dot_pattern].num_rows ?
                                   row:	    /* current row is within numrows	*/
                                   row - 1;    /* last row of min_dot_pattern	*/
                   cY = (choice_Y + row + array_height) % array_height;
                   dot = dot_direction > 0 ? min_dot_edges[min_dot_pattern].left[userow] - 1 :
                                             min_dot_edges[min_dot_pattern].right[userow] + 1;
                   for (; (dot >= -1) && (dot <= min_dot_edges[min_dot_pattern].right[userow] + 1);
                           dot += dot_direction) {
                        cX = (choice_X + dot + array_height) % array_width;
                        if (!quiet)
                            printf("dot at %d, %d is %s\n", cX, cY, ThresholdArray[cX][cY] == -1 ?
                                   "unmarked" : "marked");
                        if (ThresholdArray[cX][cY] == -1) {
                            choice_X = cX;
                            choice_Y = cY;
                            goto do_dot;
                        }
                    }
                }
            } else {
                /* odd columns are row major */
                for (dot = dot_direction > 0 ? -1 : 3;
                         dot >= -1 && dot <= 3;
                         dot += dot_direction) {
                    /* actual dot constrained below */
                    for (row = row_direction > 0 ? -1 : min_dot_edges[min_dot_pattern].num_rows;
                           (row <= min_dot_edges[min_dot_pattern].num_rows);
                           row += row_direction) {
                        userow = row < 0 ? 0 :	/* top row of the min_dot_pattern	*/
                                     row < min_dot_edges[min_dot_pattern].num_rows ?
                                        row:	    /* current row is within numrows	*/
                                        row - 1;    /* last row of min_dot_pattern	*/
                        cY = (choice_Y + row + array_height) % array_height;
                        if (dot > min_dot_edges[min_dot_pattern].right[userow] + 1)
                                break;      /* don't need this dot row */
                        cX = (choice_X + dot + array_height) % array_width;
                        if (!quiet)
                             printf("dot at %d, %d is %s\n", cX, cY, ThresholdArray[cX][cY] == -1 ?
                                    "unmarked" : "marked");
                        if (ThresholdArray[cX][cY] == -1) {
                            choice_X = cX;
                            choice_Y = cY;
                            goto do_dot;
                        }
                    }
                }
            }
            printf("what now?\n");
        }	/* end min_dot_pattern != 0 */
do_dot:
        if (!quiet)
            printf("choice: %d, choice_range: %d, do_min_dot: %d\n", choice,
                        choice_range, do_min_dot);	/* if do_min_dot is 0 and min_dot_pattern is not */
                                                        /* that means we are doing a single adjacent dot */
        if (!quiet)
            printf("Threshold Level %4d is depth %d, val = %5.3f at (%4d, %4d)\n",
                        level, choice, Val[ (choice_Y * array_width) + choice_X ], choice_X, choice_Y);
        if (do_min_dot != 0) {
            int row, dot, cX, cY;

            /* First, loop through marking the dots, then loop adjusting array density values */
            for (row=0; row < min_dot_edges[min_dot_pattern].num_rows; row++) {
                cY = (choice_Y + row) % array_height;
                for (dot=min_dot_edges[min_dot_pattern].left[row];
                        dot <= min_dot_edges[min_dot_pattern].right[row]; dot++) {
                    cX = (choice_X + dot) % array_width;
                    if ((row >= 0) || (dot >= 0))
                        ThresholdArray[cX][cY] = level;                }
            }
            for (row=0; row < min_dot_edges[min_dot_pattern].num_rows; row++) {
                cY = (choice_Y + row) % array_height;
                for (dot=min_dot_edges[min_dot_pattern].left[row];
                        dot <= min_dot_edges[min_dot_pattern].right[row]; dot++) {
                    cX = (choice_X + dot) % array_width;
                    if ((row > 0) || (dot > 0)) {
                        /* The 'choice' dot will be done outside this block as the 'last' */
                        do_dot(cX, cY, level, 0);
                        level_up++;
                    }
                }
            }
        }
        do_dot(choice_X, choice_Y, level, 1);	/* last dot in group */
    }	/* end for level... */

       /* print out final threshold array */
    if (! quiet) {
        for (Y=0; Y < array_height; Y++) {
            for (X=0; X < array_width; X++) {
                printf(" %6d", ThresholdArray[X][Y]);
                if ((X & 15) == 15)
                    printf("\n");
            }	/* end for X -- rows */
            if ((X & 15) != 0)
                    printf("\n");
        }   /* end for Y -- columns */
    }
    code = 0;				/* normal return */
    fclose(fp);
#ifdef USE_GSDLL
    close_gs_display();
#endif

    return code;
/* print out usage and exit */
usage_exit:
    printf("\nUsage:\tgen_stochastic [-m#] [-p#.##] [-q] [-rWxH] [-s#] [-t#] SIZEWxSIZEH outfile [-g ghostscript_args ... ]\n");
    printf("\n\t-m#\tset the minimum dot size/shape pattern. This is an index to a specific \n");
    printf("\t\tsize/shape table as follows (default 0):\n");
    printf("\n");
    printf("\t\t\t0:\t1:\t2:\t3:\t4:\t5:\n");
    printf("\t\t\tx\txx\tx\txx\tx\txx\n");
    printf("\t\t\t\t\tx\tx\t x\txx\n");
    printf("\n");
    printf("\t\t\t6:\t7:\t8:\t9:\t10:\t11:\t12:\t13:\n");
    printf("\t\t\txxx\txxx\txxx\txxx\txxx\txxx\txxx\txxx\n");
    printf("\t\t\t   \tx  \txx \txxx\tx  \txx \txxx\txxx\n");
    printf("\t\t   \t   \t   \t   \tx  \tx \tx \txx\n");
    printf("\n");
    printf("\t\t\t14:\t15:\t16:\t17:\t18:\t19:\n");
    printf("\t\t\tx  \txx \txx \txx \txxx\txxx\n");
    printf("\t\t\tx   \tx  \txx \txx \txx \txxx\n");
    printf("\t\t\tx   \tx   \tx   \txx  \txx\txxx\n");
    printf("\n\t-p#.##\texponenttial bias of random choice -- higher values are less random.\n");
    printf("\n\t-q\tquiet mode -- only error messages.\n");
    printf("\n\t-rWxH\tallows for horizontal / vertical resolution, e.g. -r2x1\n");
    printf("\t\tvalues are used for aspect ratio -- actual values arbitrary\n");
    printf("\n\t-s#\tInitial seed for random number generation. Useful to generate");
    printf("\n\t\tdecorrelated threshold arrays to be used with different colors.");
    printf("\n\t-t#\tsets the choice value threshold in 0.1%% units (default 10 = 1%%)\n");
    printf("\n\t-g\t(must be last) Following arguments are passed to Ghostscript if \n");
    printf("\t\tcompiled wuth USE_GS_DISPLAY=1.\n");
    printf("\n");
    return 1;
}   /* end main */

double
ValFunction(int thisX, int thisY, int refX, int refY, double rx_sq, double ry_sq)
{
    int dx, dy;
    double distance;

    dx = abs(refX - thisX);
    if (dx > array_width/2)
        dx = array_width - dx;

    dy = abs(refY - thisY);
    if (dy > array_height/2)
        dy = array_height - dy;

    distance = ((double)(dx*dx)/rx_sq) + ((double)(dy*dy)/ry_sq);

#ifdef FUDGE_DIAG_ONAXIS
        /* Now decrease the distance (increasing the value returned for	*/
        /* on-axis and diagonal positions.				*/
    if ((dx == 0) || (dy == 0) || (dx == dy)  || ((dx+dy) < 10))
                distance *= 0.7;
#endif

    return(1.0 / distance);
}

int
CompareOrder(const void *vp, const void *vq)
{
    const Order_s *p = (const Order_s *)vp;
    const Order_s *q = (const Order_s *)vq;
    int retval = 0;

    if (Val[ p->Y*array_width + p->X ] < Val[ q->Y*array_width + q->X ])
       retval = -1;
    else if (Val[ p->Y*array_width + p->X ] > Val[ q->Y*array_width + q->X ])
       retval = 1;

    return retval;
}

int
do_dot(int choice_X, int choice_Y, int level, int last)
{
    int code = 0, X, Y;
    double value;

    ThresholdArray[choice_X][choice_Y] = level;
    value = Val[ choice_Y * array_width + choice_X ];
    value = (value-MinVal) / ValRange;
    fprintf(fp,"%d\t%d\n",choice_X,choice_Y);

#ifdef USE_GSDLL
    if ((code=update_gs_dot(choice_X, choice_Y, last)) < 0)
        return -1;
#endif

    Val[ choice_Y*array_width + choice_X ] =  BIG_FLOAT;	/* value for dot already painted */
    /* accumulate the value contribution of this new pixel */
    /* While we do, also recalculate the MinVal and MaxVal and ValRange */
    MinVal = BIG_FLOAT;
    MaxVal = 0.0;
    for (Y=0; Y < array_height; Y++) {
        for (X=0; X < array_width; X++) {
            if (ThresholdArray[X][Y] == -1) {
                double rx_sq = resolution[0] * resolution[0];
                double ry_sq = resolution[1] * resolution[1];
                double vtmp = Val[ Y*array_width + X ] +
                            ValFunction(X, Y, choice_X, choice_Y, rx_sq, ry_sq);

                 Val[ Y*array_width + X ] = vtmp;
                if (vtmp < MinVal)
                    MinVal = vtmp;
                if (vtmp > MaxVal)
                    MaxVal = vtmp;
            }
        }   /* end for X -- columns */
    }	/* end for Y -- rows */
    ValRange = MaxVal - MinVal;
    if (ValRange == 0.0)
        ValRange = 1.0;
    return code;
}

#ifdef USE_GSDLL

#ifdef __WIN32__
#  include <windows.h>
#include "gdevdsp.h"
#include "dwdll.h"
#include "dwimg.h"
#include "dwtrace.h"
#include <process.h>
#include <io.h>
#endif /* __WIN32__ */

#include <fcntl.h>
#include "errors.h"
#include "iapi.h"
#include "vdtrace.h"

void *instance = NULL;

#ifdef __WIN32__
GSDLL gsdll;
BOOL quitnow = FALSE;
HANDLE hthread;
DWORD thread_id;
HWND hwndtext = NULL;	/* for dwimg.c, but not used */
HWND hwndforeground;	/* our best guess for our console window handle */


int loop_ctr;
char start_string[] = "systemdict /start get exec\n";


/*********************************************************************/
/* stdio functions */

static int GSDLLCALL
gsdll_stdin(void *instance, char *buf, int len)
{
    return _read(fileno(stdin), buf, len);
}

static int GSDLLCALL
gsdll_stdout(void *instance, const char *str, int len)
{
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

static int GSDLLCALL
gsdll_stderr(void *instance, const char *str, int len)
{
    fwrite(str, 1, len, stderr);
    fflush(stderr);
    return len;
}

/*********************************************************************/
/* dll device */

/* We must run windows from another thread, since main thread */
/* is running Ghostscript and blocks on stdin. */

/* We notify second thread of events using PostThreadMessage()
 * with the following messages. Apparently Japanese Windows sends
 * WM_USER+1 with lParam == 0 and crashes. So we use WM_USER+101.
 * Fix from Akira Kakuto
 */
#define DISPLAY_OPEN WM_USER+101
#define DISPLAY_CLOSE WM_USER+102
#define DISPLAY_SIZE WM_USER+103
#define DISPLAY_SYNC WM_USER+104
#define DISPLAY_PAGE WM_USER+105

/* The second thread is the message loop */
static void winthread(void *arg)
{
    MSG msg;
    thread_id = GetCurrentThreadId();
    hthread = GetCurrentThread();

    while (!quitnow && GetMessage(&msg, (HWND)NULL, 0, 0)) {
        switch (msg.message) {
            case DISPLAY_OPEN:
                image_open((IMAGE *)msg.lParam);
                break;
            case DISPLAY_CLOSE:
                {
                    IMAGE *img = (IMAGE *)msg.lParam;
                    HANDLE hmutex = img->hmutex;
                    image_close(img);
                    CloseHandle(hmutex);
                }
                break;
            case DISPLAY_SIZE:
                image_updatesize((IMAGE *)msg.lParam);
                break;
            case DISPLAY_SYNC:
                image_sync((IMAGE *)msg.lParam);
                break;
            case DISPLAY_PAGE:
                image_page((IMAGE *)msg.lParam);
                break;
            default:
                TranslateMessage(&msg);
                DispatchMessage(&msg);
        }
    }
}


/* New device has been opened */
/* Tell user to use another device */
int display_open(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_open(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_new(handle, device);	/* create and add to list */
    img->hmutex = CreateMutex(NULL, FALSE, NULL);
    if (img)
        PostThreadMessage(thread_id, DISPLAY_OPEN, 0, (LPARAM)img);
    return 0;
}

int display_preclose(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_preclose(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img) {
        /* grab mutex to stop other thread using bitmap */
        WaitForSingleObject(img->hmutex, 120000);
    }
    return 0;
}

int display_close(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_close(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img) {
        /* This is a hack to pass focus from image window to console */
        if (GetForegroundWindow() == img->hwnd)
            SetForegroundWindow(hwndforeground);

        image_delete(img);	/* remove from list, but don't free */
        PostThreadMessage(thread_id, DISPLAY_CLOSE, 0, (LPARAM)img);
    }
    return 0;
}

int display_presize(void *handle, void *device, int width, int height,
        int raster, unsigned int format)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_presize(0x%x 0x%x, %d, %d, %d, %d, %ld)\n",
        handle, device, width, height, raster, format);
#endif
    img = image_find(handle, device);
    if (img) {
        /* grab mutex to stop other thread using bitmap */
        WaitForSingleObject(img->hmutex, 120000);
    }
    return 0;
}

int display_size(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_size(0x%x 0x%x, %d, %d, %d, %d, %ld, 0x%x)\n",
        handle, device, width, height, raster, format, pimage);
#endif
    img = image_find(handle, device);
    if (img) {
        image_size(img, width, height, raster, format, pimage);
        /* release mutex to allow other thread to use bitmap */
        ReleaseMutex(img->hmutex);
        PostThreadMessage(thread_id, DISPLAY_SIZE, 0, (LPARAM)img);
    }
    return 0;
}

int display_sync(void *handle, void *device)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_sync(0x%x, 0x%x)\n", handle, device);
#endif
    img = image_find(handle, device);
    if (img)
        PostThreadMessage(thread_id, DISPLAY_SYNC, 0, (LPARAM)img);
    return 0;
}

int display_page(void *handle, void *device, int copies, int flush)
{
    IMAGE *img;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_page(0x%x, 0x%x, copies=%d, flush=%d)\n",
        handle, device, copies, flush);
#endif
    img = image_find(handle, device);
    if (img)
        PostThreadMessage(thread_id, DISPLAY_PAGE, 0, (LPARAM)img);
    return 0;
}

int display_update(void *handle, void *device,
    int x, int y, int w, int h)
{
    /* Unneeded for polling - we are running Windows on another thread. */
    /* Eventually we will add code here which provides progressive
     * update of the display during rendering.
     */
    return 0;
}

/*
#define DISPLAY_DEBUG_USE_ALLOC
*/
#ifdef DISPLAY_DEBUG_USE_ALLOC
/* This code isn't used, but shows how to use this function */
void *display_memalloc(void *handle, void *device, unsigned long size)
{
    void *mem;
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_memalloc(0x%x 0x%x %d)\n",
        handle, device, size);
#endif
    mem = malloc(size);
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "  returning 0x%x\n", (int)mem);
#endif
    return mem;
}

int display_memfree(void *handle, void *device, void *mem)
{
#ifdef DISPLAY_DEBUG
    fprintf(stdout, "display_memfree(0x%x, 0x%x, 0x%x)\n",
        handle, device, mem);
#endif
    free(mem);
    return 0;
}
#endif



display_callback display = {
    sizeof(display_callback),
    DISPLAY_VERSION_MAJOR,
    DISPLAY_VERSION_MINOR,
    display_open,
    display_preclose,
    display_close,
    display_presize,
    display_size,
    display_sync,
    display_page,
    display_update,
#ifdef DISPLAY_DEBUG_USE_ALLOC
    display_memalloc,	/* memalloc */
    display_memfree	/* memfree */
#else
    NULL,	/* memalloc */
    NULL	/* memfree */
#endif
};
#endif /* __WIN32__ */

int
init_gs_display(int argc, char **argv)
{
    char buf[256];
    char *gsargs[32];	/* enough */
    int atgsarg = 1;
    int code, exit_code, i, j, k;

    gsargs[0] = "";
    memset(buf, 0, sizeof(buf));

#ifdef __WIN32__
    if (!_isatty(fileno(stdin)))
        _setmode(fileno(stdin), _O_BINARY);
    _setmode(fileno(stdout), _O_BINARY);
    _setmode(fileno(stderr), _O_BINARY);

    hwndforeground = GetForegroundWindow();	/* assume this is ours */
#endif /* __WIN32__ */
    if (gsapi_new_instance((void *)&instance, NULL) < 0) {
        fprintf(stderr, "Can't create Ghostscript instance\n");
        return 1;
    }

#ifdef __WIN32__
    if (_beginthread(winthread, 65535, NULL) == -1) {
        fprintf(stderr, "GUI thread creation failed\n");
    }
    else {
        int n = 30;
        /* wait for thread to start */
        Sleep(0);
        while (n && (hthread == INVALID_HANDLE_VALUE)) {
            n--;
            Sleep(100);
        }
        while (n && (PostThreadMessage(thread_id, WM_USER, 0, 0) == 0)) {
            n--;
            Sleep(100);
        }
        if (n == 0)
            fprintf(stderr, "Can't post message to GUI thread\n");
    }
    gsapi_set_stdio(instance, gsdll_stdin, gsdll_stdout, gsdll_stderr);
    gsapi_set_display_callback(instance, &display);
    gsargs[atgsarg++] = "-dDisplayFormat=16#20804";
#endif /* __WIN32__ */

    for(i=0; i<argc; i++)
        gsargs[atgsarg++] = argv[i];
    if ((code = gsapi_init_with_args(instance, atgsarg, gsargs)) < 0) {
        printf("gsapi_init returned code = %d\n", code);
        return code;
    }
    if ((code = gsapi_run_string_begin(instance, 0, &exit_code)) < 0) {
        printf("gsapi_run_string_begin returned code = %d\n", code);
        gsapi_exit(instance);
        gsapi_delete_instance(instance);
        return -1;
    }
    sprintf(buf, "/NX %d def /NY %d def /RX %d def /RY %d def\n"
                " (genpat0.ps) run\n"
                "1 false .outputpage\n",
                array_width, array_height, resolution[0], resolution[1]);
    if ((code = gsapi_run_string_continue(instance, buf, strlen(buf), 0, &exit_code)) != gs_error_NeedInput) {
        printf(" Execution of 'genpat0.ps' returned code = %d\n", code);
        return -1;
    }
    loop_ctr = 1;		/* stop after the first */
    return 0;
}

int
update_gs_image()
{
    char	buf[MAX_ARRAY_WIDTH*2 + 256];
    char	*bufp;
    int	   	code, exit_code, i, j, k, Gray;

    sprintf(buf, "DoImage\n");
    bufp = buf + 8;
    for (i=0; i < array_width*array_height; i++) {
        if (Val[i] < BIG_FLOAT)
                /* Scale the value to be in the middle range of colors */
                /* leaving 0=BLACK and 255=white			     */
            Gray = 254 - (int)(0.5 + 253.0*
                    pow((Val[i]-MinVal)/(ValRange), 0.25));
        else
            Gray = 0;
        if ((i % array_width) == array_width - 1) {
            sprintf(bufp, "%02X\n", Gray);
            if ((code = gsapi_run_string_continue(instance, buf, strlen(buf), 0, &exit_code)) < gs_error_NeedInput) {
                printf(" during image data for DoImage returned code = %d\n", code);
                return -1;
            }
            bufp = buf;
        } else {
            sprintf(bufp, "%02X", Gray);
            bufp += 2;
        }
    }
    return 0;
}

int
update_gs_dot(int choice_X, int choice_Y, int last)
{
    char	buf[256];
    int	   	code, exit_code;

    if (loop_ctr <= 1) {
        sprintf(buf, "%d %d %d Dot\n", choice_X, choice_Y, 180);
        if ((code = gsapi_run_string_continue(instance, buf, strlen(buf), 0, &exit_code)) < gs_error_NeedInput) {
            printf(" Execution of Dot returned code = %d\n", code);
            return -1;
        }
    }
    if (last) {
        /* Sync the display before waiting for a character */
        sprintf(buf, "1 false .outputpage\n");
        if ((code = gsapi_run_string_continue(instance, buf, strlen(buf), 0, &exit_code)) < gs_error_NeedInput) {
            printf(" Execution of .outputpage returned code = %d\n", code);
            return -1;
        }


        if (--loop_ctr <= 0) {
#ifdef __WIN32__
            loop_ctr = getch();
#else
            loop_ctr = getchar();
#endif
            if (loop_ctr == 'q')
                exit(1);
            if (loop_ctr == 27 || loop_ctr == 'g')	/* Escape means run forever */
                loop_ctr = 9999999;
            else
                loop_ctr -= ' ';
            if (loop_ctr < 0)
                loop_ctr = 1;
        }

        /* Now change that dot to black (0) before proceeding */
        sprintf(buf, "%d %d %d Dot\n", choice_X, choice_Y, 0);
        if ((code = gsapi_run_string_continue(instance, buf, strlen(buf), 0, &exit_code)) < gs_error_NeedInput) {
            printf(" Execution of Dot returned code = %d\n", code);
            return -1;
        }
    }
    return 0;
}

void
close_gs_display()
{
    int exit_code;

    gsapi_run_string_end(instance, 0, &exit_code);
close_dll_exit:
    gsapi_exit(instance);
    gsapi_delete_instance(instance);
}

#endif /* USE_GSDLL */
