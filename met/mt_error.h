/*
 * Error handling
 */

typedef struct mt_error_s mt_error_t;


mt_error_t *mt_throw_imp(const char *func, const char *file, int line, mt_error_t *cause, char *fmt, ...);
void mt_print_error(mt_error_t *error); 
void mt_free_error(mt_error_t *error);

/* NB: __func__ protability ? */

#define mt_throw(...) mt_throw_imp(__func__, __FILE__, __LINE__, 0, __VA_ARGS__)
 
#define mt_rethrow(...) mt_throw_imp(__func__, __FILE__, __LINE__, __VA_ARGS__)

#define mt_okay ((mt_error_t*)0)

