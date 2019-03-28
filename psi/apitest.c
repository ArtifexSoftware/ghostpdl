#include <pthread.h>
#include "ierrors.h"
#include "iapi.h"
#include <gp.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define NUM_THREADS 10

static int my_argc;
static char **my_argv;
static int my_argv_file;

typedef struct my_stdio_s
{
    FILE *stdout;
    FILE *stderr;
} my_stdio;

/* stdio functions */
static int GSDLLCALL
my_stdin(void *instance, char *buf, int len)
{
    return 0; /* We don't support stdin */
}

static int GSDLLCALL
my_stdout(void *instance, const char *str, int len)
{
    my_stdio *stdio = (my_stdio *)instance;

    fwrite(str, 1, len, stdio->stdout);
    fflush(stdio->stdout);
    return len;
}

static int GSDLLCALL
my_stderr(void *instance, const char *str, int len)
{
    my_stdio *stdio = (my_stdio *)instance;

    fwrite(str, 1, len, stdio->stderr);
    fflush(stdio->stderr);
    return len;
}

static void *gs_main(void *arg)
{
    int threadnum = (int)(void *)arg;
    int code, code1;
    char text[256];
    void *minst = NULL;
    char **gsargv;
    int gsargc;
    int i, pos, len;
    my_stdio stdio;

    snprintf(text, sizeof(text), "stdout.%d", threadnum);
    stdio.stdout = gp_fopen(text, "w");
    snprintf(text, sizeof(text), "stderr.%d", threadnum);
    stdio.stderr = gp_fopen(text, "w");

    gsargv = malloc(sizeof(*gsargv)*my_argc);
    if (!gsargv)
    {
        fprintf(stdio.stderr, "Failed to allocate arg space in thread %d\n", threadnum);
        return (void *)-1;
    }

    for (i=0; i < my_argc; i++)
        gsargv[i] = my_argv[i];
    gsargv[my_argv_file] = text;
    gsargc = my_argc;
    
    strncpy(text, my_argv[my_argv_file], sizeof(text));
    text[sizeof(text)-1]=0;
    pos = strlen(text);
    len = sizeof(text)+1-pos;
    snprintf(text+pos, len, "%d", threadnum);

    code = gsapi_new_instance(&minst, &stdio);
    if (code < 0)
    {
        fprintf(stdio.stderr, "gsapi_new_instance failure in thread %d\n", threadnum);
        free(gsargv);
        return (void *)-1;
    }

    gsapi_set_stdio(minst, my_stdin, my_stdout, my_stderr);

    code = gsapi_init_with_args(minst, gsargc, gsargv);
    code1 = gsapi_exit(minst);
    if ((code == 0) || (code == gs_error_Quit))
        code = code1;

    gsapi_delete_instance(minst);

    free(gsargv);

    if ((code == 0) || (code == gs_error_Quit))
        return (void *)0;

    return (void *)1;
}

int main(int argc, char *argv[])
{
    int i;
    pthread_t thread[NUM_THREADS];

    my_argc = argc;
    my_argv = argv;
    
    for (i=0; i < argc; i++)
        if (!strcmp(argv[i], "-o"))
            break;
    
    if (i >= argc-1)
    {
        fprintf(stderr, "Expected a -o argument to rewrite!\n");
        exit(EXIT_FAILURE);
    }
    my_argv_file = i+1;

    for (i=0; i < NUM_THREADS; i++)
    {
        if (pthread_create(&thread[i], NULL, gs_main, (void *)i) != 0)
        {
            fprintf(stderr, "Thread %d creation failed\n", i);
            exit(EXIT_FAILURE);
        }
    }

    for (i=0; i < NUM_THREADS; i++)
    {
        if (pthread_join(thread[i], NULL) != 0)
        {
            fprintf(stderr, "Thread %d join failed\n", i);
            exit(EXIT_FAILURE);
        }
    }
    return EXIT_SUCCESS;
}
