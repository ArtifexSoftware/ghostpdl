typedef struct _RinkjDevice RinkjDevice;
typedef struct _RinkjDeviceParams RinkjDeviceParams;

struct _RinkjDeviceParams {
  int width;
  int height;
  int n_planes;
  char *plane_names;
};

struct _RinkjDevice {
  int (*set) (RinkjDevice *self, const char *config);
  int (*init) (RinkjDevice *self, const RinkjDeviceParams *params);
  int (*write) (RinkjDevice *self, const char **data);
  int init_happened;
};

/* Deprecated */
int
rinkj_device_set (RinkjDevice *self, const char *config);

int
rinkj_device_set_param (RinkjDevice *self, const char *key,
			const char *value, int value_size);

/* Convenience functions */
int
rinkj_device_set_param_string (RinkjDevice *self, const char *key,
			       const char *value);
int
rinkj_device_set_param_int (RinkjDevice *self, const char *key, int value);

int
rinkj_device_init (RinkjDevice *self, const RinkjDeviceParams *params);

int
rinkj_device_write (RinkjDevice *self, const char **data);
