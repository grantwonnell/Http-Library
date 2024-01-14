/* HTTP library in itself is 170 lines (I was proud of this) */
/* Code is from a bigger project so its why theres utilties for some library functions */
/* I tried to think through how to make the sockets non blocking or at least stop hangups but it seemed impossible because we return the response from the direct function */
/* I also tried figuring how to not statically define the header length but was too lazy */
/*
USAGE EXAMPLE

httpresponse_t *resp = HTTP(&(httpconfig_t){
    .host = inet_addr("1.1.1.1"),
    .port = htons(80),
    
    .path = "index.html",
    .data = NULL,
    .output = "/root/index.html",
    .method = "GET",
    .version = "1.1",

    .headers_len = 3,
    .headers = {
        {"Host", "216.18.189.81"},
        {"User-Agent", "Wget"},
        {"Connection", "close"},
    },
});

httpresponse_t *resp = HTTP(&(httpconfig_t){
    .host = inet_addr("1.1.1.1"),
    .port = htons(80),
    
    .path = "index.html",
    .data = "var1=hello&var2=world",
    .output = "/root/index.html",
    .method = "POST",
    .version = "1.1",

    .headers_len = 4,
    .headers = {
        {"Host", "216.18.189.81"},
        {"User-Agent", "Wget"},
        {"Connection", "close"},
        {"Conent-Type", "application/x-www-form-urlencoded"},
    },
});

*/

#include <fcntl.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>

#include <arpa/inet.h>

#include "http.h"

int UtilStrlenUntil(const char *str, char c);
char *UtilItoa(int value, int radix, char *string);
char **UtilArr(char *Rdbuf, const char *Delim, size_t *Index);
int FileWrite(const char *Path, const char *Rdbuf, size_t Len);

static void HttpInitHeader(char *Request, const char *Method, const char *Path, const char *Version) {
    strcpy(Request, Method);
    strcat(Request, " /");
    strcat(Request, Path);
    strcat(Request, " HTTP/");
    strcat(Request, Version);
    strcat(Request, "\r\n");
}

static void HttpAddHeaders(char *Request, httpconfig_t *config) {
    for(int i = 0; i < config->headers_len; i++) {
        strcat(Request, config->headers[i].name);
        strcat(Request, ": ");
        strcat(Request, config->headers[i].val);
        strcat(Request, "\r\n");
    }
}

static void AddPostLength(char *Request, httpconfig_t *config) {
    strcat(Request, "Content-Length: ");
    UtilItoa(strlen(config->data), 10, Request + strlen(Request));
    strcat(Request, "\r\n");
}

static void HttpParseHeaders(char *Response, httpresponse_t *resp) {
    char *Begin = strdup(Response);
    char *End = strstr(Begin, "\r\n\r\n");

    resp->headers = NULL;
    resp->headers_len = 0;

    if(End != NULL)
        *End = 0;

    resp->headers_end = strlen(Begin) + 4;

    size_t index = 0;
    char **args = UtilArr(Begin, "\r\n", &index);

    for(int i = 2; i < index; i++) {
        if(!strstr(args[i], ": "))
            continue;

        char *ptr;
        
        resp->headers = realloc(resp->headers, (resp->headers_len + 1) * sizeof(httpheader_t));

        resp->headers[resp->headers_len].name = strdup(strtok_r(args[i], ": ", &ptr));
        resp->headers[resp->headers_len++].val = strdup(strtok_r(NULL, ": ", &ptr));
    }

    free(Begin);
    free(args);
}

void HttpCleanupResp(httpresponse_t *resp) {
    for(int i = 0; i < resp->headers_len; i++) {
        free(resp->headers[i].name);
        free(resp->headers[i].val);
    }
    
    if(resp->headers != NULL)
        free(resp->headers);
    if(resp->response != NULL)
        free(resp->response);
    if(resp->code != NULL)
        free(resp->code);

    free(resp);

    resp = NULL;
}


char *HttpDowloadResponse(int rfd, size_t *bytes) {
	*bytes = 0;

	int ret;
	char rdbuf[2048] = {0};
	char *File = NULL;

	do {
		ret = read(rfd, rdbuf, sizeof(rdbuf));

		if(ret <= 0)
			break;

		*bytes += ret;

		if(File == NULL)
			File = calloc(1, *bytes + 1);
		else
			File = realloc(File, *bytes);

		strcat(File, rdbuf);
	} while(ret > 0);

	return File;
}

char *HttpParseFile(char *File) {
    return strstr(File, "\r\n\r\n") + 4;
}

httpresponse_t *HTTP(httpconfig_t *config) {
    httpresponse_t *resp = calloc(1, sizeof(httpresponse_t));

    int fd;
    if((fd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        resp->ret = -1;
        return resp;
    }
    
    struct sockaddr_in addr = {
        .sin_addr.s_addr = config->host,
        .sin_port = config->port,
        .sin_family = AF_INET,
    };

    if((resp->ret = connect(fd, (struct sockaddr *)&addr, sizeof(addr))) == -1)
        return resp;
    
    char *request = calloc(1, HEADER_SIZE);

    if(config->data != NULL)
        request = realloc(request, HEADER_SIZE + strlen(config->data));

    HttpInitHeader(request, config->method, config->path, config->version);
    HttpAddHeaders(request, config);

    if(!strcmp(config->method, "POST"))
        AddPostLength(request, config);

    strcat(request, "\r\n");

    if(config->data != NULL)
        strcat(request, config->data);

    if((resp->ret = send(fd, request, strlen(request), 0)) == -1) {
        free(request);
        return NULL;
    }

    resp->response = HttpDowloadResponse(fd, &resp->bytes_read);

    char *response_header = strstr(resp->response, " ") + 1;

    int header_len = UtilStrlenUntil(response_header, '\n') - 1;
    
    resp->code = calloc(1, header_len + 1);
    resp->code_int = atoi(strstr(resp->code, " ") + 1);

    memcpy(resp->code, response_header, header_len);

    if(resp->bytes_read - resp->headers_len == 0) {
        resp->body = NULL;
        free(request);
        return resp;
    }
    
    resp->body = HttpParseFile(resp->response);

    HttpParseHeaders(resp->response, resp);
    
    if(config->output != NULL)
        resp->ret = FileWrite(config->output, resp->body, resp->bytes_read - resp->headers_end);

    free(request);

    return resp;
}

int FileWrite(const char *Path, const char *Rdbuf, size_t Len) {
    int fd;
    if((fd = open(Path, O_CREAT | O_WRONLY | O_TRUNC, 0644)) == -1)
        return -1;

    int ret = write(fd, Rdbuf, Len);
    close(fd);
    
    return ret;
}

char **UtilArr(char *Rdbuf, const char *Delim, size_t *Index) {
	char *ptr;
	char *token;
    char **arr = NULL;

	token = strtok_r(Rdbuf, Delim, &ptr);

	while(token) {
        arr = realloc(arr, (*Index + 1) * sizeof(char *));
        arr[(*Index)++] = token;

		token = strtok_r(NULL, Delim, &ptr);
	}

	return arr;
}

int UtilStrlenUntil(const char *str, char c) {
    int t = 0;

    while(*str) {
        if(*str++ == c)
            break;
        else
            t++;
    }

    return t;
}

char *UtilItoa(int value, int radix, char *string)
{
    if (string == NULL)
        return NULL;

    if (value != 0)
    {
        char scratch[34];
        int neg;
        int offset;
        int c;
        unsigned int accum;

        offset = 32;
        scratch[33] = 0;

        if (radix == 10 && value < 0)
        {
            neg = 1;
            accum = -value;
        }
        else
        {
            neg = 0;
            accum = (unsigned int)value;
        }

        while (accum)
        {
            c = accum % radix;
            if (c < 10)
                c += '0';
            else
                c += 'A' - 10;

            scratch[offset] = c;
            accum /= radix;
            offset--;
        }
        
        if (neg)
            scratch[offset] = '-';
        else
            offset++;

        strcpy(string, &scratch[offset]);
    }
    else
    {
        string[0] = '0';
        string[1] = 0;
    }

    return string;
}

int main(void) {
    httpresponse_t *resp = HTTP(&(httpconfig_t){
        .host = INET_ADDR(216,18,189,81),
        .port = htons(80),
        
        .path = "index.html",
        .data = NULL,
        .output = "/root/index.html",
        .method = "GET",
        .version = "1.1",

        .headers_len = 3,
        .headers = {
            {"Host", "216.18.189.81"},
            {"User-Agent", "Wget"},
            {"Connection", "close"},
        },
    });
}

