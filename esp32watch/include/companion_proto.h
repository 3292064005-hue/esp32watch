#ifndef COMPANION_PROTO_H
#define COMPANION_PROTO_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

size_t companion_proto_process_line(const char *request, char *out, size_t out_size);

#ifdef __cplusplus
}
#endif

#endif
