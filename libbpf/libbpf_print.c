#include "libbpf.h"
#include "libbpf_internal.h"

/* this function is needed to not include libbpf.c that needs a bunch of other dependencies too */
void libbpf_print(enum libbpf_print_level level, const char *format, ...)
{
	va_list args;
	va_start(args, format);
	vfprintf(stderr, format, args);
	va_end(args);
}