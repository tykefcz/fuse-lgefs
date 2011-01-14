#ifdef DEBUG
#define debug(a)	(debug_printf a)
void debug_printf(char *fmt, ...);
#else
#define	debug(a)	
#endif