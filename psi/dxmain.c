/* Copyright (C) 2001-2025 Artifex Software, Inc.
   All Rights Reserved.

   This software is provided AS-IS with no warranty, either express or
   implied.

   This software is distributed under license and may not be copied,
   modified or distributed except as expressly authorized under the terms
   of the license contained in the file LICENSE in this distribution.

   Refer to licensing information at http://www.artifex.com or contact
   Artifex Software, Inc.,  39 Mesa Street, Suite 108A, San Francisco,
   CA 94129, USA, for further information.
*/


/* dxmain.c */
/*
 * Ghostscript frontend which provides a graphical window
 * using Gtk+.  Load time linking to libgs.so
 * Compile using
 *    gcc `gtk-config --cflags` -o gs dxmain.c -lgs `gtk-config --libs`
 *
 * The ghostscript library needs to be compiled with
 *  gcc -fPIC -g -c -Wall file.c
 *  gcc -shared -Wl,-soname,libgs.so.6 -o libgs.so.6.60 file.o -lc
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <locale.h>
#include <gtk/gtk.h>
#define __PROTOTYPES__
#include "ierrors.h"
#include "iapi.h"
#include "gdevdsp.h"
#include "locale_.h"

const char start_string[] = "systemdict /start get exec\n";

static gboolean read_stdin_handler(GIOChannel *channel, GIOCondition condition,
        gpointer data);
static int gsdll_stdin(void *instance, char *buf, int len);
static int gsdll_stdout(void *instance, const char *str, int len);
static int gsdll_stdout(void *instance, const char *str, int len);
static int display_open(void *handle, void *device);
static int display_preclose(void *handle, void *device);
static int display_close(void *handle, void *device);
static int display_presize(void *handle, void *device, int width, int height,
        int raster, unsigned int format);
static int display_size(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage);
static int display_sync(void *handle, void *device);
static int display_page(void *handle, void *device, int copies, int flush);
static int display_update(void *handle, void *device, int x, int y,
        int w, int h);

#ifndef min
#define min(a,b) ((a) < (b) ? (a) : (b))
#endif

/*********************************************************************/
/* stdio functions */

struct stdin_buf {
   char *buf;
   int len;	/* length of buffer */
   int count;	/* number of characters returned */
};

/* handler for reading non-blocking stdin */
static gboolean
read_stdin_handler(GIOChannel *channel, GIOCondition condition, gpointer data)
{
    struct stdin_buf *input = (struct stdin_buf *)data;
    GError *error = NULL;
    gsize c;

    if (condition & (G_IO_PRI)) {
        g_print("input exception");
        input->count = 0;	/* EOF */
    }
    else if (condition & (G_IO_IN)) {
        g_io_channel_read_chars(channel, input->buf, input->len, &c, &error);
        input->count = (int)c;
        if (error) {
            g_print("%s\n", error->message);
            g_error_free(error);
        }
    }
    else {
        g_print("input condition unknown");
        input->count = 0;	/* EOF */
    }
    return TRUE;
}

/* callback for reading stdin */
static int
gsdll_stdin(void *instance, char *buf, int len)
{
    struct stdin_buf input;
    gint input_tag;
    GIOChannel *channel;
    GError *error = NULL;

    input.len = len;
    input.buf = buf;
    input.count = -1;

    channel = g_io_channel_unix_new(fileno(stdin));
    g_io_channel_set_encoding(channel, NULL, &error);
    g_io_channel_set_buffered(channel, FALSE);
    if (error) {
        g_print("%s\n", error->message);
        g_error_free(error);
    }
    input_tag = g_io_add_watch(channel,
        (G_IO_IN | G_IO_PRI | G_IO_ERR | G_IO_HUP),
        read_stdin_handler, &input);
    while (input.count < 0)
        gtk_main_iteration_do(TRUE);
    g_source_remove(input_tag);
    g_io_channel_unref(channel);

    return input.count;
}

static int
gsdll_stdout(void *instance, const char *str, int len)
{
    gtk_main_iteration_do(FALSE);
    fwrite(str, 1, len, stdout);
    fflush(stdout);
    return len;
}

static int
gsdll_stderr(void *instance, const char *str, int len)
{
    gtk_main_iteration_do(FALSE);
    fwrite(str, 1, len, stderr);
    fflush(stderr);
    return len;
}


/*********************************************************************/
/* dll display device */

typedef struct IMAGE_DEVICEN_S IMAGE_DEVICEN;
struct IMAGE_DEVICEN_S {
    int used;		/* non-zero if in use */
    int visible;	/* show on window */
    char name[64];
    int cyan;
    int magenta;
    int yellow;
    int black;
    int menu;		/* non-zero if menu item added to system menu */
};
#define IMAGE_DEVICEN_MAX 8

typedef struct IMAGE_S IMAGE;
struct IMAGE_S {
    void *handle;
    void *device;
    GtkWidget *window;
    GtkWidget *vbox;
    GtkWidget *cmyk_bar;
    GtkWidget *separation[IMAGE_DEVICEN_MAX];
    GtkWidget *show_as_gray;
    GtkWidget *scroll;
    GtkWidget *darea;
    guchar *buf;
    gint width;
    gint height;
    gint rowstride;
    unsigned int format;
    int devicen_gray;	/* true if a single separation should be shown gray */
    IMAGE_DEVICEN devicen[IMAGE_DEVICEN_MAX];
    guchar *rgbbuf;	/* used when we need to convert raster format */
    IMAGE *next;
};

IMAGE *first_image;
static IMAGE *image_find(void *handle, void *device);
static void window_destroy(GtkWidget *w, gpointer data);
static void window_create(IMAGE *img);
static void window_resize(IMAGE *img);
#if !GTK_CHECK_VERSION(3, 0, 0)
static gboolean window_draw(GtkWidget *widget, GdkEventExpose *event, gpointer user_data);
#else
static gboolean window_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data);
#endif

static IMAGE *
image_find(void *handle, void *device)
{
    IMAGE *img;
    for (img = first_image; img!=0; img=img->next) {
        if ((img->handle == handle) && (img->device == device))
            return img;
    }
    return NULL;
}

static gboolean
#if !GTK_CHECK_VERSION(3, 0, 0)
window_draw(GtkWidget *widget, GdkEventExpose *event, gpointer user_data)
#else
window_draw(GtkWidget *widget, cairo_t *cr, gpointer user_data)
#endif
{
    IMAGE *img = (IMAGE *)user_data;
    if (img && img->window && img->buf) {
        GdkPixbuf *pixbuf = NULL;
        int color = img->format & DISPLAY_COLORS_MASK;
        int depth = img->format & DISPLAY_DEPTH_MASK;
#if GTK_CHECK_VERSION(3, 0, 0)
        guint width, height;
#endif

#if !GTK_CHECK_VERSION(3, 0, 0)
        cairo_t *cr = gdk_cairo_create(gtk_widget_get_window(widget));
        gdk_cairo_region(cr, event->region);
        cairo_clip(cr);
#endif
#if GTK_CHECK_VERSION(3, 0, 0)
        width = gtk_widget_get_allocated_width (widget);
        height = gtk_widget_get_allocated_height (widget);
        gtk_render_background(gtk_widget_get_style_context(widget), cr, 0, 0, width, height);
#else
        gdk_cairo_set_source_color(cr, &gtk_widget_get_style(widget)->bg[GTK_STATE_NORMAL]);
#endif
        cairo_paint(cr);
            switch (color) {
                case DISPLAY_COLORS_NATIVE:
                    if (depth == DISPLAY_DEPTH_8 && img->rgbbuf)
                        pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                            GDK_COLORSPACE_RGB, FALSE, 8,
                            img->width, img->height, img->width*3,
                            NULL, NULL);
                    else if ((depth == DISPLAY_DEPTH_16) && img->rgbbuf)
                        pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                            GDK_COLORSPACE_RGB, FALSE, 8,
                            img->width, img->height, img->width*3,
                            NULL, NULL);
                    break;
                case DISPLAY_COLORS_GRAY:
                    if (depth == DISPLAY_DEPTH_8 && img->rgbbuf)
                        pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                            GDK_COLORSPACE_RGB, FALSE, 8,
                            img->width, img->height, img->width*3,
                            NULL, NULL);
                    break;
                case DISPLAY_COLORS_RGB:
                    if (depth == DISPLAY_DEPTH_8) {
                        if (img->rgbbuf)
                            pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                                GDK_COLORSPACE_RGB, FALSE, 8,
                                img->width, img->height, img->width*3,
                                NULL, NULL);
                        else
                            pixbuf = gdk_pixbuf_new_from_data(img->buf,
                                GDK_COLORSPACE_RGB, FALSE, 8,
                                img->width, img->height, img->rowstride,
                                NULL, NULL);
                    }
                    break;
                case DISPLAY_COLORS_CMYK:
                    if (((depth == DISPLAY_DEPTH_1) ||
                        (depth == DISPLAY_DEPTH_8)) && img->rgbbuf)
                        pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                            GDK_COLORSPACE_RGB, FALSE, 8,
                            img->width, img->height, img->width*3,
                            NULL, NULL);
                    break;
                case DISPLAY_COLORS_SEPARATION:
                    if ((depth == DISPLAY_DEPTH_8) && img->rgbbuf)
                        pixbuf = gdk_pixbuf_new_from_data(img->rgbbuf,
                            GDK_COLORSPACE_RGB, FALSE, 8,
                            img->width, img->height, img->width*3,
                            NULL, NULL);
                    break;
            }
            if (pixbuf) gdk_cairo_set_source_pixbuf(cr, pixbuf, 0, 0);
            cairo_paint(cr);
#if !GTK_CHECK_VERSION(3, 0, 0)
            cairo_destroy(cr);
#endif
            if (pixbuf) g_object_unref(pixbuf);
    }
    return TRUE;
}

static void window_destroy(GtkWidget *w, gpointer data)
{
    IMAGE *img = (IMAGE *)data;
    img->window = NULL;
    img->scroll = NULL;
    img->darea = NULL;
}

static void window_create(IMAGE *img)
{
    /* Create a gtk window */
    img->window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(img->window), "gs");
#if !GTK_CHECK_VERSION(3, 0, 0)
    img->vbox = gtk_vbox_new(FALSE, 0);
#else
    img->vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_box_set_homogeneous(GTK_BOX (img->vbox), FALSE);
#endif
    gtk_container_add(GTK_CONTAINER(img->window), img->vbox);
    gtk_widget_show(img->vbox);

    img->darea = gtk_drawing_area_new();
    gtk_widget_show(img->darea);
    img->scroll = gtk_scrolled_window_new(NULL, NULL);
    gtk_widget_show(img->scroll);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(img->scroll),
        GTK_POLICY_ALWAYS, GTK_POLICY_ALWAYS);

    gtk_container_add(GTK_CONTAINER(img->scroll), img->darea);

    gtk_box_pack_start(GTK_BOX(img->vbox), img->scroll, TRUE, TRUE, 0);
#if !GTK_CHECK_VERSION(3, 0, 0)
    g_signal_connect(G_OBJECT (img->darea), "expose-event",
#else
    g_signal_connect(G_OBJECT (img->darea), "draw",
#endif
                        G_CALLBACK (window_draw), img);
    g_signal_connect(G_OBJECT (img->window), "destroy",
                        G_CALLBACK (window_destroy), img);
    g_signal_connect(G_OBJECT (img->window), "delete-event",
                        G_CALLBACK (gtk_widget_hide_on_delete), NULL);
    /* do not show img->window until we know the image size */
}

static void window_resize(IMAGE *img)
{
    gboolean visible;
    gtk_widget_set_size_request(GTK_WIDGET (img->darea),
        img->width, img->height);

#if !GTK_CHECK_VERSION(3, 0, 0)
    visible = (GTK_WIDGET_FLAGS(img->window) & GTK_VISIBLE);
#else
    visible = gtk_widget_get_visible(img->window);
#endif

    if (!visible) {
        /* We haven't yet shown the window, so set a default size
         * which is smaller than the desktop to allow room for
         * desktop toolbars, and if possible a little larger than
         * the image to allow room for the scroll bars.
         * We don't know the width of the scroll bars, so just guess. */
#if !GTK_CHECK_VERSION(3, 0, 0)
        gtk_window_set_default_size(GTK_WINDOW(img->window),
            min(gdk_screen_width()-96, img->width+24),
            min(gdk_screen_height()-96, img->height+24));
#else
        guint width, height;
        width = gtk_widget_get_allocated_width (img->window) - 96;
        height = gtk_widget_get_allocated_height (img->window) - 96;
        gtk_window_set_default_size(GTK_WINDOW(img->window),
            min(width, img->width+24),
            min(height, img->height+24));
#endif
    }
}

static void window_separation(IMAGE *img, int sep)
{
    img->devicen[sep].visible = !img->devicen[sep].visible;
    display_sync(img->handle, img->device);
}

static void signal_sep0(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 0);
}

static void signal_sep1(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 1);
}

static void signal_sep2(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 2);
}

static void signal_sep3(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 3);
}

static void signal_sep4(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 4);
}

static void signal_sep5(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 5);
}

static void signal_sep6(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 6);
}

static void signal_sep7(GtkWidget *w, gpointer data)
{
    window_separation((IMAGE *)data, 7);
}

GCallback signal_separation[IMAGE_DEVICEN_MAX] = {
    (GCallback)signal_sep0,
    (GCallback)signal_sep1,
    (GCallback)signal_sep2,
    (GCallback)signal_sep3,
    (GCallback)signal_sep4,
    (GCallback)signal_sep5,
    (GCallback)signal_sep6,
    (GCallback)signal_sep7
};

static GtkWidget *
window_add_button(IMAGE *img, const char *label, GCallback fn)
{
    GtkWidget *w;
    w = gtk_check_button_new_with_label(label);
    gtk_box_pack_start(GTK_BOX(img->cmyk_bar), w, FALSE, FALSE, 5);
    gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(w), TRUE);
    g_signal_connect(G_OBJECT(w), "clicked", fn, img);
    gtk_widget_show(w);
    return w;
}

static void signal_show_as_gray(GtkWidget *w, gpointer data)
{
    IMAGE *img= (IMAGE *)data;
    img->devicen_gray= !img->devicen_gray;
    display_sync(img->handle, img->device);
}

/* New device has been opened */
static int display_open(void *handle, void *device)
{

    IMAGE *img = (IMAGE *)malloc(sizeof(IMAGE));
    if (img == NULL)
        return -1;
    memset(img, 0, sizeof(IMAGE));

    /* add to list */
    if (first_image)
        img->next = first_image;
    first_image = img;

    /* remember device and handle */
    img->handle = handle;
    img->device = device;

    /* create window */
    window_create(img);

    gtk_main_iteration_do(FALSE);
    return 0;
}

static int display_preclose(void *handle, void *device)
{
    IMAGE *img = image_find(handle, device);
    if (img == NULL)
        return -1;

    gtk_main_iteration_do(FALSE);

    img->buf = NULL;
    img->width = 0;
    img->height = 0;
    img->rowstride = 0;
    img->format = 0;

    gtk_widget_destroy(img->window);
    img->window = NULL;
    img->scroll = NULL;
    img->darea = NULL;
    if (img->rgbbuf)
        free(img->rgbbuf);
    img->rgbbuf = NULL;

    gtk_main_iteration_do(FALSE);

    return 0;
}

static int display_close(void *handle, void *device)
{
    IMAGE *img = image_find(handle, device);
    if (img == NULL)
        return -1;

    /* remove from list */
    if (img == first_image) {
        first_image = img->next;
    }
    else {
        IMAGE *tmp;
        for (tmp = first_image; tmp!=0; tmp=tmp->next) {
            if (img == tmp->next)
                tmp->next = img->next;
        }
    }

    return 0;
}

static int display_presize(void *handle, void *device, int width, int height,
        int raster, unsigned int format)
{
    /* Assume everything is OK.
     * It would be better to return gs_error_rangecheck if we can't
     * support the format.
     */
    return 0;
}

static int display_size(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage)
{
    IMAGE *img = image_find(handle, device);
    int color;
    int depth;
    int i;
    gboolean visible;

    if (img == NULL)
        return -1;

    if (img->rgbbuf)
        free(img->rgbbuf);
    img->rgbbuf = NULL;

    img->width = width;
    img->height = height;
    img->rowstride = raster;
    img->buf = pimage;
    img->format = format;

    /* Reset separations */
    for (i=0; i<IMAGE_DEVICEN_MAX; i++) {
        img->devicen[i].used = 0;
        img->devicen[i].visible = 1;
        memset(img->devicen[i].name, 0, sizeof(img->devicen[i].name));
        img->devicen[i].cyan = 0;
        img->devicen[i].magenta = 0;
        img->devicen[i].yellow = 0;
        img->devicen[i].black = 0;
    }

    color = img->format & DISPLAY_COLORS_MASK;
    depth = img->format & DISPLAY_DEPTH_MASK;
    switch (color) {
        case DISPLAY_COLORS_NATIVE:
            if (depth == DISPLAY_DEPTH_8) {
                img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
                if (img->rgbbuf == NULL)
                    return -1;
                break;
            }
            else if (depth == DISPLAY_DEPTH_16) {
                /* need to convert to 24RGB */
                img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
                if (img->rgbbuf == NULL)
                    return -1;
            }
            else
                return gs_error_rangecheck;	/* not supported */
        case DISPLAY_COLORS_GRAY:
            if (depth == DISPLAY_DEPTH_8) {
                img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
                if (img->rgbbuf == NULL)
                    return -1;
                break;
            }
            else
                return -1;	/* not supported */
        case DISPLAY_COLORS_RGB:
            if (depth == DISPLAY_DEPTH_8) {
                if (((img->format & DISPLAY_ALPHA_MASK) == DISPLAY_ALPHA_NONE)
                    && ((img->format & DISPLAY_ENDIAN_MASK)
                        == DISPLAY_BIGENDIAN))
                    break;
                else {
                    /* need to convert to 24RGB */
                    img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
                    if (img->rgbbuf == NULL)
                        return -1;
                }
            }
            else
                return -1;	/* not supported */
            break;
        case DISPLAY_COLORS_CMYK:
            if ((depth == DISPLAY_DEPTH_1) || (depth == DISPLAY_DEPTH_8)) {
                /* need to convert to 24RGB */
                img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
                if (img->rgbbuf == NULL)
                    return -1;
                /* We already know about the CMYK components */
                img->devicen[0].used = 1;
                img->devicen[0].cyan = 65535;
                strncpy(img->devicen[0].name, "Cyan",
                    sizeof(img->devicen[0].name));
                img->devicen[1].used = 1;
                img->devicen[1].magenta = 65535;
                strncpy(img->devicen[1].name, "Magenta",
                    sizeof(img->devicen[1].name));
                img->devicen[2].used = 1;
                img->devicen[2].yellow = 65535;
                strncpy(img->devicen[2].name, "Yellow",
                    sizeof(img->devicen[2].name));
                img->devicen[3].used = 1;
                img->devicen[3].black = 65535;
                strncpy(img->devicen[3].name, "Black",
                    sizeof(img->devicen[3].name));
            }
            else
                return -1;	/* not supported */
            break;
        case DISPLAY_COLORS_SEPARATION:
            /* we can't display this natively */
            /* we will convert it just before displaying */
            if (depth != DISPLAY_DEPTH_8)
                return -1;	/* not supported */
            img->rgbbuf = (guchar *)malloc((size_t)width * height * 3);
            if (img->rgbbuf == NULL)
                return -1;
            break;
    }

    if ((color == DISPLAY_COLORS_CMYK) ||
        (color == DISPLAY_COLORS_SEPARATION)) {
        if (!GTK_IS_WIDGET(img->cmyk_bar)) {
            /* add bar to select separation */
#if !GTK_CHECK_VERSION(3, 0, 0)
            img->cmyk_bar = gtk_hbox_new(FALSE, 0);
#else
            img->cmyk_bar = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 0);
            gtk_box_set_homogeneous(GTK_BOX(img->cmyk_bar), FALSE);
#endif
            gtk_box_pack_start(GTK_BOX(img->vbox), img->cmyk_bar,
                FALSE, FALSE, 0);
            for (i=0; i<IMAGE_DEVICEN_MAX; i++) {
               img->separation[i] =
                window_add_button(img, img->devicen[i].name,
                   signal_separation[i]);
            }
            img->show_as_gray = gtk_check_button_new_with_label("Show as Gray");
            gtk_box_pack_end(GTK_BOX(img->cmyk_bar), img->show_as_gray,
                FALSE, FALSE, 5);
            gtk_toggle_button_set_active(GTK_TOGGLE_BUTTON(img->show_as_gray),
                FALSE);
            g_signal_connect(G_OBJECT(img->show_as_gray), "clicked",
                G_CALLBACK(signal_show_as_gray), img);
            gtk_widget_show(img->show_as_gray);
        }
        gtk_widget_show(img->cmyk_bar);
    }
    else {
        if (GTK_IS_WIDGET(img->cmyk_bar))
            gtk_widget_hide(img->cmyk_bar);
    }

    window_resize(img);

#if !GTK_CHECK_VERSION(3, 0, 0)
    visible = (GTK_WIDGET_FLAGS(img->window) & GTK_VISIBLE);
#else
    visible = gtk_widget_get_visible(img->window);
#endif

    if (!visible) gtk_widget_show_all(img->window);

    gtk_main_iteration_do(FALSE);
    return 0;
}

static int display_sync(void *handle, void *device)
{
    IMAGE *img = image_find(handle, device);
    int color;
    int depth;
    int endian;
    int native555;
    int alpha;
    gboolean visible;

    if (img == NULL)
        return -1;

    color = img->format & DISPLAY_COLORS_MASK;
    depth = img->format & DISPLAY_DEPTH_MASK;
    endian = img->format & DISPLAY_ENDIAN_MASK;
    native555 = img->format & DISPLAY_555_MASK;
    alpha = img->format & DISPLAY_ALPHA_MASK;

    if ((color == DISPLAY_COLORS_CMYK) ||
        (color == DISPLAY_COLORS_SEPARATION)) {
        /* check if separations have changed */
        int i;
        const gchar *str;
        for (i=0; i<IMAGE_DEVICEN_MAX; i++) {
            str = gtk_label_get_text(
                GTK_LABEL(gtk_bin_get_child(GTK_BIN(img->separation[i]))));
            if (!img->devicen[i].used)
                gtk_widget_hide(img->separation[i]);
            else if (strcmp(img->devicen[i].name, str) != 0) {
                /* text has changed, update it */
                gtk_label_set_text(
                    GTK_LABEL(gtk_bin_get_child(GTK_BIN(img->separation[i]))),
                    img->devicen[i].name);
                gtk_widget_show(img->separation[i]);
            }
        }
    }

    /* some formats need to be converted for use by GdkRgb */
    switch (color) {
        case DISPLAY_COLORS_NATIVE:
            if (depth == DISPLAY_DEPTH_16) {
              if (endian == DISPLAY_LITTLEENDIAN) {
                if (native555 == DISPLAY_NATIVE_555) {
                    /* BGR555 */
                    int x, y;
                    unsigned short w;
                    unsigned char value;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            w = s[0] + (s[1] << 8);
                            value = (w >> 10) & 0x1f;	/* red */
                            *d++ = (value << 3) + (value >> 2);
                            value = (w >> 5) & 0x1f;	/* green */
                            *d++ = (value << 3) + (value >> 2);
                            value = w & 0x1f;		/* blue */
                            *d++ = (value << 3) + (value >> 2);
                            s += 2;
                        }
                    }
                }
                else {
                    /* BGR565 */
                    int x, y;
                    unsigned short w;
                    unsigned char value;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            w = s[0] + (s[1] << 8);
                            value = (w >> 11) & 0x1f;	/* red */
                            *d++ = (value << 3) + (value >> 2);
                            value = (w >> 5) & 0x3f;	/* green */
                            *d++ = (value << 2) + (value >> 4);
                            value = w & 0x1f;		/* blue */
                            *d++ = (value << 3) + (value >> 2);
                            s += 2;
                        }
                    }
                }
              }
              else {
                if (native555 == DISPLAY_NATIVE_555) {
                    /* RGB555 */
                    int x, y;
                    unsigned short w;
                    unsigned char value;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            w = s[1] + (s[0] << 8);
                            value = (w >> 10) & 0x1f;	/* red */
                            *d++ = (value << 3) + (value >> 2);
                            value = (w >> 5) & 0x1f;	/* green */
                            *d++ = (value << 3) + (value >> 2);
                            value = w & 0x1f;		/* blue */
                            *d++ = (value << 3) + (value >> 2);
                            s += 2;
                        }
                    }
                }
                else {
                    /* RGB565 */
                    int x, y;
                    unsigned short w;
                    unsigned char value;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            w = s[1] + (s[0] << 8);
                            value = (w >> 11) & 0x1f;	/* red */
                            *d++ = (value << 3) + (value >> 2);
                            value = (w >> 5) & 0x3f;	/* green */
                            *d++ = (value << 2) + (value >> 4);
                            value = w & 0x1f;		/* blue */
                            *d++ = (value << 3) + (value >> 2);
                            s += 2;
                        }
                    }
                }
              }
            }
            if (depth == DISPLAY_DEPTH_8) {
                /* palette of 96 colors */
                guchar color[96][3];
                int i;
                int one = 255 / 3;
                int x, y;
                unsigned char *s, *d;

                for (i=0; i<96; i++) {
                    /* 0->63 = 00RRGGBB, 64->95 = 010YYYYY */
                    if (i < 64) {
                        color[i][0] =
                            ((i & 0x30) >> 4) * one; /* r */
                        color[i][1] =
                            ((i & 0x0c) >> 2) * one; /* g */
                        color[i][2] =
                            (i & 0x03) * one; /* b */
                    }
                    else {
                        int val = i & 0x1f;
                        val = (val << 3) + (val >> 2);
                        color[i][0] = color[i][1] = color[i][2] = val;
                    }
                }

                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                            *d++ = color[*s][0];	/* r */
                            *d++ = color[*s][1];	/* g */
                            *d++ = color[*s][2];	/* b */
                            s++;
                    }
                }
            }
            break;
        case DISPLAY_COLORS_GRAY:
            if (depth == DISPLAY_DEPTH_8) {
                int x, y;
                unsigned char *s, *d;
                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                            *d++ = *s;	/* r */
                            *d++ = *s;	/* g */
                            *d++ = *s;	/* b */
                            s++;
                    }
                }
            }
            break;
        case DISPLAY_COLORS_RGB:
            if ( (depth == DISPLAY_DEPTH_8) &&
                 ((alpha == DISPLAY_ALPHA_FIRST) ||
                  (alpha == DISPLAY_UNUSED_FIRST)) &&
                 (endian == DISPLAY_BIGENDIAN) ) {
                /* Mac format */
                int x, y;
                unsigned char *s, *d;
                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                        s++;		/* x = filler */
                        *d++ = *s++;	/* r */
                        *d++ = *s++;	/* g */
                        *d++ = *s++;	/* b */
                    }
                }
            }
            else if ( (depth == DISPLAY_DEPTH_8) &&
                      (endian == DISPLAY_LITTLEENDIAN) ) {
                if ((alpha == DISPLAY_UNUSED_LAST) ||
                    (alpha == DISPLAY_ALPHA_LAST)) {
                    /* Windows format + alpha = BGRx */
                    int x, y;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            *d++ = s[2];	/* r */
                            *d++ = s[1];	/* g */
                            *d++ = s[0];	/* b */
                            s += 4;
                        }
                    }
                }
                else if ((alpha == DISPLAY_UNUSED_FIRST) ||
                    (alpha == DISPLAY_ALPHA_FIRST)) {
                    /* xBGR */
                    int x, y;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            *d++ = s[3];	/* r */
                            *d++ = s[2];	/* g */
                            *d++ = s[1];	/* b */
                            s += 4;
                        }
                    }
                }
                else {
                    /* Windows BGR24 */
                    int x, y;
                    unsigned char *s, *d;
                    for (y = 0; y<img->height; y++) {
                        s = img->buf + y * img->rowstride;
                        d = img->rgbbuf + y * img->width * 3;
                        for (x=0; x<img->width; x++) {
                            *d++ = s[2];	/* r */
                            *d++ = s[1];	/* g */
                            *d++ = s[0];	/* b */
                            s += 3;
                        }
                    }
                }
            }
            break;
        case DISPLAY_COLORS_CMYK:
            if (depth == DISPLAY_DEPTH_8) {
                /* Separations */
                int x, y;
                int cyan, magenta, yellow, black;
                unsigned char *s, *d;
                int vc = img->devicen[0].visible;
                int vm = img->devicen[1].visible;
                int vy = img->devicen[2].visible;
                int vk = img->devicen[3].visible;
                int vall = vc && vm && vy && vk;
                int show_gray = (vc + vm + vy + vk == 1) && img->devicen_gray;
                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                        cyan = *s++;
                        magenta = *s++;
                        yellow = *s++;
                        black = *s++;
                        if (!vall) {
                            if (!vc)
                                cyan = 0;
                            if (!vm)
                                magenta = 0;
                            if (!vy)
                                yellow = 0;
                            if (!vk)
                                black = 0;
                            if (show_gray) {
                                black += cyan + magenta + yellow;
                                cyan = magenta = yellow = 0;
                            }
                        }
                        *d++ = (255-cyan)    * (255-black) / 255; /* r */
                        *d++ = (255-magenta) * (255-black) / 255; /* g */
                        *d++ = (255-yellow)  * (255-black) / 255; /* b */
                    }
                }
            }
            else if (depth == DISPLAY_DEPTH_1) {
                /* Separations */
                int x, y;
                int cyan, magenta, yellow, black;
                unsigned char *s, *d;
                int vc = img->devicen[0].visible;
                int vm = img->devicen[1].visible;
                int vy = img->devicen[2].visible;
                int vk = img->devicen[3].visible;
                int vall = vc && vm && vy && vk;
                int show_gray = (vc + vm + vy + vk == 1) && img->devicen_gray;
                int value;
                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                        value = s[x/2];
                        if (x & 0)
                            value >>= 4;
                        cyan = ((value >> 3) & 1) * 255;
                        magenta = ((value >> 2) & 1) * 255;
                        yellow = ((value >> 1) & 1) * 255;
                        black = (value & 1) * 255;
                        if (!vall) {
                            if (!vc)
                                cyan = 0;
                            if (!vm)
                                magenta = 0;
                            if (!vy)
                                yellow = 0;
                            if (!vk)
                                black = 0;
                            if (show_gray) {
                                black += cyan + magenta + yellow;
                                cyan = magenta = yellow = 0;
                            }
                        }
                        *d++ = (255-cyan)    * (255-black) / 255; /* r */
                        *d++ = (255-magenta) * (255-black) / 255; /* g */
                        *d++ = (255-yellow)  * (255-black) / 255; /* b */
                    }
                }
            }
            break;
        case DISPLAY_COLORS_SEPARATION:
            if (depth == DISPLAY_DEPTH_8) {
                int j;
                int x, y;
                unsigned char *s, *d;
                int cyan, magenta, yellow, black;
                int num_comp = 0;
                int value;
                int num_visible = 0;
                int show_gray = 0;
                IMAGE_DEVICEN *devicen = img->devicen;
                for (j=0; j<IMAGE_DEVICEN_MAX; j++) {
                    if (img->devicen[j].used) {
                       num_comp = j+1;
                       if (img->devicen[j].visible)
                            num_visible++;
                    }
                }
                if ((num_visible == 1) && img->devicen_gray)
                    show_gray = 1;

                for (y = 0; y<img->height; y++) {
                    s = img->buf + y * img->rowstride;
                    d = img->rgbbuf + y * img->width * 3;
                    for (x=0; x<img->width; x++) {
                        cyan = magenta = yellow = black = 0;
                        if (show_gray) {
                            for (j=0; j<num_comp; j++) {
                                devicen = &img->devicen[j];
                                if (devicen->visible && devicen->used)
                                    black += s[j];
                            }
                        }
                        else {
                            for (j=0; j<num_comp; j++) {
                                devicen = &img->devicen[j];
                                if (devicen->visible && devicen->used) {
                                    value = s[j];
                                    cyan    += value*devicen->cyan   /65535;
                                    magenta += value*devicen->magenta/65535;
                                    yellow  += value*devicen->yellow /65535;
                                    black   += value*devicen->black  /65535;
                                }
                            }
                        }
                        if (cyan > 255)
                           cyan = 255;
                        if (magenta > 255)
                           magenta = 255;
                        if (yellow > 255)
                           yellow = 255;
                        if (black > 255)
                           black = 255;
                        *d++ = (255-cyan)    * (255-black) / 255; /* r */
                        *d++ = (255-magenta) * (255-black) / 255; /* g */
                        *d++ = (255-yellow)  * (255-black) / 255; /* b */
                        s += 8;
                    }
                }
            }
            break;
    }

    if (!GTK_IS_WIDGET(img->window)) {
        window_create(img);
        window_resize(img);
    }
#if !GTK_CHECK_VERSION(3, 0, 0)
    visible = (GTK_WIDGET_FLAGS(img->window) & GTK_VISIBLE);
#else
    visible = gtk_widget_get_visible(img->window);
#endif

    if (!visible) gtk_widget_show_all(img->window);

    gtk_widget_queue_draw(img->darea);
    gtk_main_iteration_do(FALSE);
    return 0;
}

static int display_page(void *handle, void *device, int copies, int flush)
{
    display_sync(handle, device);
    return 0;
}

static int display_update(void *handle, void *device,
    int x, int y, int w, int h)
{
    /* not implemented - eventually this will be used for progressive update */
    return 0;
}

static int
display_separation(void *handle, void *device,
    int comp_num, const char *name,
    unsigned short c, unsigned short m,
    unsigned short y, unsigned short k)
{
    IMAGE *img = image_find(handle, device);
    if (img == NULL)
        return -1;
    if ((comp_num < 0) || (comp_num > IMAGE_DEVICEN_MAX))
        return -1;
    img->devicen[comp_num].used = 1;
    strncpy(img->devicen[comp_num].name, name,
        sizeof(img->devicen[comp_num].name)-1);
    img->devicen[comp_num].cyan    = c;
    img->devicen[comp_num].magenta = m;
    img->devicen[comp_num].yellow  = y;
    img->devicen[comp_num].black   = k;
    return 0;
}

/* callback structure for "display" device */
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
    NULL,	/* memalloc */
    NULL,	/* memfree */
    display_separation
};

static int
write_stderr(const char *str)
{
    fwrite(str, 1, strlen(str), stderr);

    return fflush(stderr);
}


/* Note the space! It makes the string merging simpler */
#define OUR_DEFAULT_DEV_STR "display "

/*********************************************************************/

int main(int argc, char *argv[])
{
    int exit_status;
    int code = 1, code1;
    void *instance = NULL;
    int nargc;
    char **nargv;
    char dformat[64];
    int exit_code;
    gboolean use_gui;
    char *default_devs = NULL;
    char *our_default_devs = NULL;
    int len;
    char *curlocale;

    /* Gtk initialisation */
    curlocale = setlocale(LC_ALL, "");
    use_gui = gtk_init_check(&argc, &argv);

    /* insert display device parameters as first arguments */
    sprintf(dformat, "-dDisplayFormat=%d",
            DISPLAY_COLORS_RGB | DISPLAY_ALPHA_NONE | DISPLAY_DEPTH_8 |
            DISPLAY_BIGENDIAN | DISPLAY_TOPFIRST);
    nargc = argc + 1;
    nargv = (char **)malloc((size_t)nargc * sizeof(char *));
    if (nargv == NULL)
        return 1;

    nargv[0] = argv[0];
    nargv[1] = dformat;
    memcpy(&nargv[2], &argv[1], (argc-1) * sizeof(char *));

    /* run Ghostscript */
    if ((code = gsapi_new_instance(&instance, NULL)) == 0) {
        gsapi_set_stdio(instance, gsdll_stdin, gsdll_stdout, gsdll_stderr);

        if (curlocale == NULL || strstr(curlocale, "UTF-8") != NULL || strstr(curlocale, "utf8") != NULL)
            code = gsapi_set_arg_encoding(instance, 1); /* PS_ARG_ENCODING_UTF8 = 1 */
        else {
            code = gsapi_set_arg_encoding(instance, 0); /* PS_ARG_ENCODING_LOCAL = 0 */
        }

        if (use_gui) {
            gsapi_set_display_callback(instance, &display);

            code = gsapi_get_default_device_list(instance, &default_devs, &len);
            if (code >= 0) {
                our_default_devs = malloc((size_t)len + strlen(OUR_DEFAULT_DEV_STR) + 1);
                if (our_default_devs) {
                    memcpy(our_default_devs, OUR_DEFAULT_DEV_STR, strlen(OUR_DEFAULT_DEV_STR));
                    memcpy(our_default_devs + strlen(OUR_DEFAULT_DEV_STR), default_devs, len);
                    our_default_devs[len + strlen(OUR_DEFAULT_DEV_STR)] = '\0';
                    code = gsapi_set_default_device_list(instance, our_default_devs, strlen(default_devs));
                    free(our_default_devs);
                }
                else {
                    code = -1;
                }
            }
            if (code < 0) {
                write_stderr("Could not set default devices, continuing with existing defaults\n");
                code = 0;
            }
        }

        if (code == 0)
            code = gsapi_init_with_args(instance, nargc, nargv);

        if (code == 0)
            code = gsapi_run_string(instance, start_string, 0, &exit_code);
        code1 = gsapi_exit(instance);
        if (code == 0 || code == gs_error_Quit)
            code = code1;
        if (code == gs_error_Quit)
            code = 0;	/* user executed 'quit' */

        gsapi_delete_instance(instance);
    }

    exit_status = 0;
    switch (code) {
        case 0:
        case gs_error_Info:
        case gs_error_Quit:
            break;
        case gs_error_Fatal:
            exit_status = 1;
            break;
        default:
            exit_status = 255;
    }

    return exit_status;
}
