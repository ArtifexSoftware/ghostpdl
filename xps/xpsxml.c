/* Recursive descent XML parsing API on top of Expat. */

#include "ghostxps.h"

#include <expat.h>

#define XMLBUFLEN 4096

struct xps_item_s
{
    char *name;
    char **atts;
    xps_item_t *up;
    xps_item_t *down;
    xps_item_t *next;
};

struct xps_parser_s
{
    xps_context_t *ctx;
    xps_item_t *root;
    xps_item_t *head;
    const char *error;
    int nexted;
    int downed;
};


static void free_item(xps_context_t *ctx, xps_item_t *item)
{
    xps_item_t *next;
    while (item)
    {
	next = item->next;
	if (item->down)
	    free_item(ctx, item->down);
	xps_free(ctx, item);
	item = next;
    }
}

static void on_open_tag(void *zp, const char *name, const char **atts)
{
    xps_parser_t *parser = zp;
    xps_context_t *ctx = parser->ctx;
    xps_item_t *item;
    xps_item_t *tail;
    int namelen;
    int attslen;
    int textlen;
    char *p;
    int i;

    if (parser->error)
	return;

    /* count size to alloc */

    namelen = strlen(name) + 1;
    attslen = sizeof(char*);
    textlen = 0;
    for (i = 0; atts[i]; i++)
    {
	attslen += sizeof(char*);
	textlen += strlen(atts[i]) + 1;
    }

    item = xps_alloc(ctx, sizeof(xps_item_t) + attslen + namelen + textlen);
    if (!item)
    {
	parser->error = "out of memory";
    }

    /* copy strings to new memory */

    item->atts = (char**) (((char*)item) + sizeof(xps_item_t));
    item->name = ((char*)item) + sizeof(xps_item_t) + attslen;
    p = ((char*)item) + sizeof(xps_item_t) + attslen + namelen;

    strcpy(item->name, name);
    for (i = 0; atts[i]; i++)
    {
	item->atts[i] = p;
	strcpy(item->atts[i], atts[i]);
	p += strlen(p) + 1;
    }

    item->atts[i] = 0;

    /* link item into tree */

    item->up = parser->head;
    item->down = NULL;
    item->next = NULL;

    if (!parser->head)
    {
	parser->root = item;
	parser->head = item;
	return;
    }

    if (!parser->head->down)
    {
	parser->head->down = item;
	parser->head = item;
	return;
    }

    tail = parser->head->down;
    while (tail->next)
	tail = tail->next;
    tail->next = item;
    parser->head = item;
}

static void on_close_tag(void *zp, const char *name)
{
    xps_parser_t *parser = zp;

    if (parser->error)
	return;

    if (parser->head)
	parser->head = parser->head->up;
}

static inline int is_xml_space(int c)
{
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

static void on_text(void *zp, const char *buf, int len)
{
    xps_parser_t *parser = zp;
    xps_context_t *ctx = parser->ctx;
    int i;

    if (parser->error)
	return;

    for (i = 0; i < len; i++)
    {
	if (!is_xml_space(buf[i]))
	{
	    char *tmp = xps_alloc(ctx, len + 1);
	    const char *atts[] = {"", tmp, 0};
	    if (!tmp)
	    {
		parser->error = "out of memory";
		return;
	    }
	    memcpy(tmp, buf, len);
	    tmp[len] = 0;
	    on_open_tag(zp, "", atts);
	    on_close_tag(zp, "");
	    xps_free(ctx, tmp);
	    return;
	}
    }
}


xps_parser_t *
xps_new_parser(xps_context_t *ctx, char *buf, int len)
{
    xps_parser_t *parser;
    XML_Parser xp;
    int code;

    parser = xps_alloc(ctx, sizeof(xps_parser_t));
    if (!parser)
	return NULL;

    parser->ctx = ctx;
    parser->root = NULL;
    parser->head = NULL;
    parser->error = NULL;
    parser->downed = 0;
    parser->nexted = 0;

    /* xp = XML_ParserCreateNS(NULL, ns); */
    xp = XML_ParserCreate(NULL);
    if (!xp)
    {
	xps_free(ctx, parser);
	gs_throw(-1, "xml error: could not create expat parser");
	return NULL;
    }

    XML_SetUserData(xp, parser);
    XML_SetParamEntityParsing(xp, XML_PARAM_ENTITY_PARSING_NEVER);

    XML_SetStartElementHandler(xp, on_open_tag);
    XML_SetEndElementHandler(xp, on_close_tag);
    XML_SetCharacterDataHandler(xp, on_text);

    code = XML_Parse(xp, buf, len, 1);
    if (code == 0)
    {
	if (parser->root)
	    free_item(ctx, parser->root);
	xps_free(ctx, parser);
	XML_ParserFree(xp);
	gs_throw1(-1, "xml error: %s", XML_ErrorString(XML_GetErrorCode(xp)));
	return NULL;
    }

    parser->head = NULL;
    return parser;
}

void
xps_free_parser(xps_parser_t *parser)
{
    xps_context_t *ctx = parser->ctx;
    if (parser->root)
	free_item(ctx, parser->root);
    xps_free(ctx, parser);
}

static void indent(int n)
{
    while (n--)
	printf("  ");
}

void
xps_debug_item(xps_item_t *item, int level)
{
    int i;

    while (item)
    {
	indent(level);

	if (strlen(item->name) == 0)
	    printf("%s\n", item->atts[1]);
	else
	{
	    printf("<%s", item->name);

	    for (i = 0; item->atts[i]; i += 2)
		printf(" %s=\"%s\"", item->atts[i], item->atts[i+1]);

	    if (item->down)
	    {
		printf(">\n");
		xps_debug_item(item->down, level + 1);
		indent(level);
		printf("</%s>\n", item->name);
	    }
	    else
		printf(" />\n");
	}

	item = item->next;
    }
}

void
xps_debug_parser(xps_parser_t *parser)
{
    xps_debug_item(parser->root, 0);
}

xps_item_t *
xps_next_item(xps_parser_t *parser)
{
    if (parser->downed)
	return NULL;

    if (!parser->head)
    {
	parser->head = parser->root;
	return parser->head;
    }

    if (!parser->nexted)
    {
	parser->nexted = 1;
	return parser->head;
    }

    if (parser->head->next)
    {
	parser->head = parser->head->next;
	return parser->head;
    }

    return NULL;
}

void
xps_down(xps_parser_t *parser)
{
    if (!parser->downed && parser->head && parser->head->down)
	parser->head = parser->head->down;
    else
	parser->downed ++;
    parser->nexted = 0;
}

void
xps_up(xps_parser_t *parser)
{
    if (!parser->downed && parser->head && parser->head->up)
	parser->head = parser->head->up;
    else
	parser->downed --;
}

char *
xps_tag(xps_item_t *item)
{
    return item->name;
}

char *
xps_att(xps_item_t *item, char *att)
{
    int i;
    for (i = 0; item->atts[i]; i += 2)
	if (!strcmp(item->atts[i], att))
	    return item->atts[i + 1];
    return NULL;
}

xps_mark_t xps_mark(xps_parser_t *parser)
{
    xps_mark_t mark;
    mark.head = parser->head;
    mark.nexted = parser->nexted;
    mark.downed = parser->downed;
    return mark;
}

void xps_goto(xps_parser_t *parser, xps_mark_t mark)
{
    parser->head = mark.head;
    parser->nexted = mark.nexted;
    parser->downed = mark.downed;
}

