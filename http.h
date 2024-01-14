#pragma once

#include <stdint.h>

#define ALLOC_SIZE 1024
#define HEADER_SIZE 2048

typedef struct httpheader_t {
    char *name;
    char *val;
} httpheader_t;

typedef struct httpresponse_t {
    int ret;
    size_t bytes_read;

    char *body;
    char *code;
    char *response;
    httpheader_t *headers;

    int code_int;

    size_t headers_len;
    size_t headers_end;
} httpresponse_t;

typedef struct httpconfig_t {
    uint32_t host;
    uint16_t port;

    char *path;
    char *data;
    char *output;
    char *method;
    char *version;
    
    size_t headers_len;
    httpheader_t headers[64];
} httpconfig_t;

void HttpCleanupResp(httpresponse_t *resp);
httpresponse_t *HTTP(httpconfig_t *config);
