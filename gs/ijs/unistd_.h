/* unistd_.h */
#ifdef _MSC_VER
#include <process.h>
#include <io.h>
#include <fcntl.h>
#define read(handle, buffer, count) _read(handle, buffer, count)
#define write(handle, buffer, count) _write(handle, buffer, count)
#else
#include <unistd.h>
#endif
