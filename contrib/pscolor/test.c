/* using GS DLL as a ps colorpage separator.
        Carsten Hammer Océ-Deutschland GmbH
        Hammer.Carsten@oce.de			*/

#ifdef _Windows
/* Compile with:
 * cl -D_Windows -Isrc -Febin\pscolor.exe test.c lex.yy.c bin\gsdll32.lib
 */
#include <windows.h>
#define GSDLLEXPORT __declspec(dllimport)
#endif

#include <stdio.h>
#include <malloc.h>

/* prevent gp.h redefining fopen */
#define fopen fopen

#include "stdpre.h"
#include "iapi.h"    /* Ghostscript interpreter public interface */
#include "gdevdsp.h"
#include "string_.h"
#include "ierrors.h"
#include "gscdefs.h"
#include "gstypes.h"
#include "iref.h"
#include "iminst.h"
#include "imain.h"
#include "gp.h"

#include "gsdll.h"   /* old DLL public interface */

unsigned char *myimage;
int breite,hoehe,seite=0,myraster;
FILE *color_fd,*black_fd,*temp_fd,*choose;
FILE *read_fd;

//#define DISPLAY_DEBUG

int yylex(char *buf,int *mylen);

/* stdio functions */
static int GSDLLCALL
gsdll_stdin(void *instance, char *buf, int len)
{
    int c;
    int count = 0;
    char *init=buf;
    int eof;
    int hlen=len;
    eof=yylex(buf,&hlen);
//    fprintf(stderr, "eof %d\n",eof);
    if(eof==1001){
    fprintf(color_fd,"%s",buf);
    fflush(color_fd);
    fprintf(black_fd,"%s",buf);
    fflush(black_fd);
    return (strlen(buf));
    } else if(eof==1000){
    fprintf(temp_fd,"%s",buf);
        fflush(temp_fd);
        fclose(temp_fd);
        read_fd=fopen("temp.ps","rb");
//    fprintf(stderr, "Seitenende %d %d %s\n",hlen,strlen(buf),buf);
        while( (c=fgetc(read_fd)) != EOF)
        {
                fputc(c,choose);
        }
 //   fprintf(stderr, "Seitenende ende %d %d %s\n",hlen,strlen(buf),buf);
        fflush(choose);
        fclose(read_fd);
        temp_fd=fopen("temp.ps","wb");
    return (strlen(buf));
    } else if(eof!=0){
//    fprintf(stderr, "Mittendrin %s\n",buf);
    fprintf(temp_fd,"%s",buf);
    fflush(temp_fd);
    return (strlen(buf));
    } else {
    fprintf(stderr, "Dateiende\n");
        read_fd=fopen("temp.ps","rb");
        while( (c=fgetc(read_fd)) != EOF)
        {
                fputc(c,choose);
        }
        fflush(choose);
        fclose(read_fd);
        fclose(temp_fd);
        temp_fd=fopen("temp.ps","wb");
    return 0;
        }
}
#if 0
static int GSDLLCALL
gsdll_stdin(void *instance, char *buf, int len)
{
    int ch;
    int count = 0;
        size_t back;
    char *init=buf;
    while (count < len) {
        ch = fgetc(stdin);
        if (ch == EOF){
#ifdef DISPLAY_DEBUG
    fprintf(stderr, "lese %s\n",buf);
#endif
        fwrite(init,1,count,temp_fd);
        fflush(temp_fd);
            return 0;
        }
        *buf++ = ch;
        count++;
        if (ch == '\n')
            break;
    }
#ifdef DISPLAY_DEBUG
    fprintf(stderr, "lese %s\n",buf);
#endif
        fwrite(init,1,count,temp_fd);
        fflush(temp_fd);
    return count;
}
#endif

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

/* New device has been opened */
/* This is the first event from this device. */
static int display_open(void *handle, void *device)
{
#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_open(0x%x, 0x%x)\n", handle, device);
    fprintf(stderr, buf);
#endif
    return 0;
}

/* Device is about to be closed. */
/* Device will not be closed until this function returns. */
static int display_preclose(void *handle, void *device)
{
#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_preclose(0x%x, 0x%x)\n", handle, device);
    fprintf(stderr, buf);
#endif
    /* do nothing - no thread synchonisation needed */
    return 0;
}

/* Device has been closed. */
/* This is the last event from this device. */
static int display_close(void *handle, void *device)
{
#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_close(0x%x, 0x%x)\n", handle, device);
    fprintf(stderr, buf);
#endif
    return 0;
}

/* Device is about to be resized. */
/* Resize will only occur if this function returns 0. */
static int display_presize(void *handle, void *device, int width, int height,
        int raster, unsigned int format)
{
#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_presize(0x%x, 0x%x, width=%d height=%d raster=%d\n\
  format=%d)\n",
       handle, device, width, height, raster, format);
    fprintf(stderr, buf);
#endif
    return 0;
}

/* Device has been resized. */
/* New pointer to raster returned in pimage */
static int display_size(void *handle, void *device, int width, int height,
        int raster, unsigned int format, unsigned char *pimage)
{

#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_size(0x%x, 0x%x, width=%d height=%d raster=%d\n\
  format=%d image=0x%x)\n",
       handle, device, width, height, raster, format, pimage);
    fprintf(stderr, buf);
#endif
        myimage=pimage;
        breite=width;
        hoehe=height;
        myraster=raster;
           return 0;
}

/* flushpage */
static int display_sync(void *handle, void *device)
{

#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_sync(0x%x, 0x%x)\n", handle, device);
    fprintf(stderr, buf);
#endif

    return 0;
}

/* showpage */
/* If you want to pause on showpage, then don't return immediately */
static int display_page(void *handle, void *device, int copies, int flush)
{
        int i,t,color=0;
#ifdef DISPLAY_DEBUG
    char buf[256];
    sprintf(buf, "display_page(0x%x, 0x%x, copies=%d flush=%d)\n",
        handle, device, copies, flush);
    fprintf(stderr, buf);
#endif
        seite++;
        for(i=hoehe-1;i>=0;i=i-1){
                for(t=breite-1;t>=0;t=t-1){
                        if((myimage[i*myraster+t*3+0]!=myimage[i*myraster+t*3+1])||(myimage[i*myraster+t*3+1]!=myimage[i*myraster+t*3+2])){
                                color=1;
                                fprintf(stderr,"Ungleich %d,%d %x,%x,%x\n",i,t,myimage[i*myraster+t*3+0],myimage[i*myraster+t*3+1],myimage[i*myraster+t*3+2]);
                                goto out;
                        }
                }
        }
out:
        if(color){
        fprintf(stderr,"[%d]Color\n",seite);
        choose=color_fd;
        } else {
        fprintf(stderr,"[%d]Grey\n",seite);
        choose=black_fd;
        }
/*
        read_fd=fopen("temp.ps","rb");
        while( (c=fgetc(read_fd)) != EOF)
        {
                fputc(c,choose);
        }
        fflush(choose);
        fclose(read_fd);
        fclose(temp_fd);
        temp_fd=fopen("temp.ps","wb");
*/
    return 0;
}

/* Poll the caller for cooperative multitasking. */
/* If this function is NULL, polling is not needed */
static int display_update(void *handle, void *device,
    int x, int y, int w, int h)
{
    return 0;
}

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
    NULL	/* memfree */
};

static gs_main_instance *minst;
const char start_string[] = "systemdict /start get exec\n";

int main(int argc, char *argv[])
{
    int code;
    const char * gsargv[12];
    char arg[64];
    int gsargc;
    gsargv[0] = "ps2colordetect";	/* actual value doesn't matter */
    gsargv[1] = "-dNOPAUSE";
    gsargv[2] = "-dBATCH";
    gsargv[3] = "-dSAFER";
//    gsargv[4] = "-sDEVICE=pdfwrite";
    gsargv[4] = "-sDEVICE=display";
    gsargv[5] = "-dDisplayHandle=0";
    sprintf(arg,"-dDisplayFormat=%d",DISPLAY_COLORS_RGB|DISPLAY_ALPHA_NONE|DISPLAY_DEPTH_8|DISPLAY_LITTLEENDIAN|DISPLAY_BOTTOMFIRST);
    gsargv[6] = arg;
    gsargv[7] = "-sOutputFile=out.pdf";
    gsargv[8] = "-c";
    gsargv[9] = ".setpdfwrite";
    gsargv[10] = "-f";
//    gsargv[11] = "input.ps";
    gsargv[11] = "-";
    gsargc=12;

    code = gsapi_new_instance(&minst, NULL);
    if (code < 0)
        return 1;
    gsapi_set_stdio(minst, gsdll_stdin, gsdll_stdout, gsdll_stderr);
    gsapi_set_display_callback(minst, &display);

        color_fd=fopen("color.ps","wb");
        black_fd=fopen("black.ps","wb");
        temp_fd=fopen("temp.ps","wb");
        choose=black_fd;

    code = gsapi_init_with_args(minst, gsargc, gsargv);
    gsapi_exit(minst);

    gsapi_delete_instance(minst);

        fclose(color_fd);
        fclose(black_fd);
        fclose(temp_fd);

    if ((code == 0) || (code == gs_error_Quit))
        return 0;
    return 1;
}
