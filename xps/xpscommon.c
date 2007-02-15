#include "ghostxps.h"

void
xps_parse_render_transform(xps_context_t *ctx, char *transform, gs_matrix *matrix)
{
    float args[6];
    char *s = transform;
    int i;

    args[0] = 1.0; args[1] = 0.0;
    args[2] = 0.0; args[3] = 1.0;
    args[4] = 0.0; args[5] = 0.0;

    for (i = 0; i < 6 && *s; i++)
    {
	args[i] = atof(s);
	while (*s && *s != ',')
	    s++;
	if (*s == ',')
	    s++;
    }

    matrix->xx = args[0]; matrix->xy = args[1];
    matrix->yx = args[2]; matrix->yy = args[3];
    matrix->tx = args[4]; matrix->ty = args[5];

    dprintf7("parsed transform '%s' into [%g %g %g %g %g %g]\n",
	    transform,
	    args[0], args[1],
	    args[2], args[3],
	    args[4], args[5]);
}

void
xps_parse_matrix_transform(xps_context_t *ctx, xps_item_t *root, gs_matrix *matrix)
{
    char *transform;

    gs_make_identity(matrix);

    if (!strcmp(xps_tag(root), "MatrixTransform"))
    {
	transform = xps_att(root, "Matrix");
	if (transform)
	    xps_parse_render_transform(ctx, transform, matrix);
    }
}

