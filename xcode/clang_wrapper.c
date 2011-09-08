#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <libgen.h>

int main(int argc, char *argv[])
{
    int i, l;
    char output[1024], *o;
    char run[1024];
    char base[1024];

    strcpy(base, dirname(argv[0]));
    o = output;
    *o = 0;
    for (i=1; i < argc; i++)
    {
        char *arg = argv[i];
        FILE *f = fopen(arg, "rb");
        int l;
        *o++ = 32;
        *o = 0;
        if (f != NULL)
        {
            char cmd[1024];
            char *tmp = tmpnam(NULL);
            fclose(f);

            sprintf(cmd, "%s/resolve.sh %s > %s", base, arg, tmp);
            system(cmd);
            f = fopen(tmp, "rb");
            if (f == NULL) {
                strcpy(cmd, arg);
            } else {
                char *p = cmd;
                do {
                    int c = fgetc(f);
                    if ((c == EOF) || (c < 32))
                        break;
                    *p++ = c;
                } while (!feof(f));
                *p++ = 0;
                fclose(f);
                unlink(tmp);
            }
            l = strlen(cmd);
            strcpy(o, cmd);
            o += l;
        } else {
            char *space = strchr(arg, ' ');
            if (space)
            {
                char *equals = strchr(arg, '=');
                if (equals && equals < space)
                {
                    memcpy(o, arg, equals+1-arg);
                    o += equals+1-arg;
                    arg = equals+1;
                }
                *o++ = '\"';
            }
            l = strlen(arg);
            memcpy(o, arg, l);
            o += l;
            if (space)
                *o++ = '\"';
            *o = 0;
        }
        
    }
    sprintf(run, "clang --analyze %s", output);
    system(run);
    sprintf(run, "gcc %s", output);
    system(run);
    
    return 0;
}
