#ifndef __STRBUF_H__
#define __STRBUF_H__

#include <sys/types.h>
#include <string.h>

#ifndef HAVE_STRLCPY
extern size_t strlcat(char *dest, const char *src, size_t count);
extern size_t strlcpy(char *dest, const char *src, size_t size);
#endif

#endif
