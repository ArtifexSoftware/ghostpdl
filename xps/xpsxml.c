/* Recursive descent XML parsing API on top of Expat. */

xps_item_t *xps_next_item(xps_parser_t*);
void xps_go_down(xps_parser_t*);
void xps_go_up(xps_parser_t*);

xps_mark_t xps_mark(xps_parser_t*);
void xps_goto(xps_parser_t*, xps_mark_t);

char *xps_get_tag(xps_item_t*);
char *xps_get_att(xps_item_t*, char*);

