#ifdef __cplusplus
extern "C" {
#endif

typedef struct _IjsClientCtx IjsClientCtx;

IjsClientCtx *
ijs_invoke_server (const char *server_cmd);

int 
ijs_exec_server (const char *server_cmd, int *pfd_to, int *pfd_from, 
    int *pchild_pid);

int
ijs_client_begin_cmd (IjsClientCtx *ctx, IjsCommand cmd);

int
ijs_client_send_int (IjsClientCtx *ctx, int val);

int
ijs_client_send_cmd (IjsClientCtx *ctx);

int
ijs_client_send_cmd_wait (IjsClientCtx *ctx);

int
ijs_client_send_data_wait (IjsClientCtx *ctx, IjsJobId job_id,
			   const char *buf, int size);

int
ijs_client_open (IjsClientCtx *ctx);

int
ijs_client_close (IjsClientCtx *ctx);

int
ijs_client_begin_job (IjsClientCtx *ctx, IjsJobId job_id);

int
ijs_client_end_job (IjsClientCtx *ctx, IjsJobId job_id);

int
ijs_client_list_params (IjsClientCtx *ctx, IjsJobId job_id,
		       char *value, int value_size);

int
ijs_client_enum_param (IjsClientCtx *ctx, IjsJobId job_id,
		       const char *key, char *value,
		       int value_size);

int
ijs_client_set_param (IjsClientCtx *ctx, IjsJobId job_id,
		      const char *key, const char *value,
		      int value_size);

int
ijs_client_get_param (IjsClientCtx *ctx, IjsJobId job_id,
		      const char *key, char *value,
		      int value_size);

int
ijs_client_begin_page (IjsClientCtx *ctx, IjsJobId job_id);

int
ijs_client_end_page (IjsClientCtx *ctx, IjsJobId job_id);

int
ijs_client_get_version (IjsClientCtx *ctx);

#ifdef __cplusplus
}
#endif
