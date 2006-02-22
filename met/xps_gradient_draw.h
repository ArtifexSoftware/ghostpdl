
enum { XPS_PAD, XPS_REPEAT, XPS_REFLECT };

int
xps_draw_linear_gradient(gs_memory_t *mem, gs_state *pgs,
	double *pt0, double *pt1,
	int spread, double *stops, int nstops);

int
xps_draw_radial_gradient(gs_memory_t *mem, gs_state *pgs,
	double *pt0, double *pt1, double xrad, double yrad,
	int spread, double *stops, int nstops);

