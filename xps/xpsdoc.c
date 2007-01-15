#include "ghostxps.h"

int
xps_process_fpage(xps_context_t *ctx, xps_part_t *part)
{
    xps_parser_t *parser;
    parser = xps_new_parser(ctx, part->data, part->size);
    if (!parser)
	return gs_rethrow(-1, "could not create xml parser");

    xps_debug_parser(parser);

    return 0;
}

int
xps_process_part(xps_context_t *ctx, xps_part_t *part)
{
    if (part->complete)
    {
	dprintf1("complete part '%s'.\n", part->name);
	if (strstr(part->name, ".fpage"))
	{
	    dprintf("  a page!\n");
	    xps_process_fpage(ctx, part);
	}
    }
    else
    {
	dprintf1("incomplete piece '%s'.\n", part->name);
    }
}

