/* plplatf.h   Platform-related utils */

#ifndef plplatf_INCLUDED
#   define plplatf_INCLUDED

/* ------------- Platform de/init --------- */
void
pl_platform_init(P1(FILE *debug_out));

void
pl_platform_dnit(P1(int exit_status));


/*----- The following is declared here, but must be implemented by client ----*/
/* Terminate execution */
void pl_exit(P1(int exit_status));

#endif     /* plplatf_INCLUDED */

