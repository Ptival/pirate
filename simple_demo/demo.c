#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "primitives.h"
#include "tiny.h"
#ifdef HIGH
#include <regex.h>
#include <pthread.h>
#include <sys/stat.h>
#endif

/* GAPS channel configuration will be automatically linked */
#define LOW_NAME  "\033[1;32mLOW\033[0m"
#define HIGH_NAME "\033[1;31mHIGH\033[0m"

#define HIGH_TO_LOW_CH 0
#define LOW_TO_HIGH_CH 1

pirate_channel_desc_t pirate_channels[] = {
#ifdef HIGH
    PIRATE_CHANNEL_CONFIG(HIGH_TO_LOW_CH, O_WRONLY, HIGH_NAME"->"LOW_NAME),
    PIRATE_CHANNEL_CONFIG(LOW_TO_HIGH_CH, O_RDONLY, HIGH_NAME"<-"LOW_NAME),
#endif /* HIGH */
#ifdef LOW
    PIRATE_CHANNEL_CONFIG(HIGH_TO_LOW_CH, O_RDONLY, LOW_NAME"<-"HIGH_NAME),
    PIRATE_CHANNEL_CONFIG(LOW_TO_HIGH_CH, O_WRONLY, LOW_NAME"->"HIGH_NAME),
#endif /* LOW */
    PIRATE_END_CHANNEL_CONFIG
};

#define DATA_LEN            (32 << 10)         // 32 KB
typedef struct {
    char buf[DATA_LEN];
    int len;
} data_t;

#ifdef HIGH
#define NAME                HIGH_NAME
#endif /* HIGH */

#ifdef LOW
#define NAME                LOW_NAME
#endif /* LOW */


#ifdef HIGH
#define HTML_PATH           "./index.html"
static int load_web_content(data_t* data) {
    struct stat sbuf;
    if (stat(HTML_PATH, &sbuf) < 0) {
        fprintf(stderr, "ERROR: could not find file %s\n", HTML_PATH);
        return -1;
    }

    if (sbuf.st_size >= (long)sizeof(data->buf)) {
        fprintf(stderr, "ERROR: file %s exceeds size limits\n", HTML_PATH);
        return -1;
    }

    FILE* fp = fopen(HTML_PATH, "r");
    if (fp == NULL) {
        fprintf(stderr, "ERROR failed to open %s\n", HTML_PATH);
        return -1;
    }

    if (fread(data->buf, sbuf.st_size, 1, fp) != 1) {
        fprintf(stderr, "Failed to read %s file\n", HTML_PATH);
        return -1;
    }

    fclose(fp);

    data->buf[sbuf.st_size] = '\0';
    data->len = sbuf.st_size;
    
    return 0;
}
#endif

#ifdef LOW
static int load_web_content(data_t* data) {
    /* Low side requests data by writing zero */
    int len = 0;
    ssize_t num = pirate_write(LOW_TO_HIGH_CH, &len, sizeof(int));
    if (num != sizeof(int)) {
        fprintf(stderr, "Failed to send request\n");
        return -1;
    }
    printf("Sent read request to the %s side\n", HIGH_NAME);

    /* Read and validate response length */
    num = pirate_read(HIGH_TO_LOW_CH, &len, sizeof(len));
    if (num != sizeof(len)) {
        fprintf(stderr, "Failed to receive response length\n");
        return -1;
    }

    if (len >= DATA_LEN) {
        fprintf(stderr, "Response length %d is too large\n", len);
        return -1;
    }

    /* Read back the response */
    num = pirate_read(HIGH_TO_LOW_CH, data->buf, len);
    if (num != len) {
        fprintf(stderr, "Failed to read back the response\n");
        return -1;
    }

    /* Success */
    data->len = len;
    printf("Received %d bytes from the %s side\n", data->len, HIGH_NAME);
    return 0;
}
#endif


#ifdef HIGH
#define HTML_BOLD_REGEX     "<b>[0-9]*,</b>\\s*"
static void* gaps_thread(void *arg) {
    (void)arg;
    data_t high_data;
    data_t low_data;

    while (1) {
        /* Low side requests data by writing zero */
        int len = 0;
        ssize_t num = pirate_read(LOW_TO_HIGH_CH, &len, sizeof(len));
        if (num != sizeof(len)) {
            fprintf(stderr, "Failed to read request from the low side\n");
            exit(-1);
        }

        if (len != 0) {
            fprintf(stderr, "Invalid request from the low side %d\n", len);
            continue;
        }

        printf("Received data request from the %s side\n", LOW_NAME);

        /* Read in high data */
        if (load_web_content(&high_data) != 0) {
            fprintf(stderr, "Failed to load data\n");
            continue;
        }

        /* Create filtered data (remove bold text) */
        regmatch_t match;
        regex_t regex;
        int ret = regcomp(&regex, HTML_BOLD_REGEX, REG_EXTENDED);
        if (ret != 0) {
            fprintf(stderr, "Failed to compile regex\n");
            continue;
        }

        char* search = high_data.buf;
        char* wr = low_data.buf;
        while ((ret = regexec(&regex, search, 1, &match, 0)) == 0) {
            memcpy(wr, search, match.rm_so);
            wr += match.rm_so;
            search += match.rm_eo;
        }
        int tail_len = high_data.len - (search - high_data.buf);
        memcpy(wr, search, tail_len);
        wr[tail_len] = '\0';
        low_data.len = strlen(low_data.buf);

        /* Reply back. Data length is sent first */
        num = pirate_write(HIGH_TO_LOW_CH, &low_data.len, sizeof(low_data.len));
        if (num != sizeof(low_data.len)) {
            fprintf(stderr, "Failed to send response length\n");
            continue;
        }

        num = pirate_write(HIGH_TO_LOW_CH, &low_data.buf, low_data.len);
        if (num != low_data.len) {
            fprintf(stderr, "Failed to send response content\n");
            continue;
        }

        printf("Sent %d bytes to the %s side\n\n", low_data.len, LOW_NAME);
    }
    return NULL;
}

static int run_gaps()
{
    pthread_t tid;
    int ret = pthread_create(&tid, NULL, gaps_thread, NULL);
    if (ret != 0) {
        fprintf(stderr, "Failed to start the GAPS thread\n");
        return ret;
    }
    return 0;
}
#endif


static int web_server(int port) {
    server_t si;
    client_t ci;
    request_t ri;
    data_t data;

    /* Create, initialize, bind, listen on server socket */
    server_connect(&si, port);

    /*
     * Wait for a connection request, parse HTTP, serve high requested content,
     * close connection.
     */
    while (1) {
        /* accept client's connection and open fstream */
        client_connect(&si, &ci);

        /* process client request */
        client_request_info(&ci, &ri);

        /* tiny only supports the GET method */
        if (strcasecmp(ri.method, "GET")) {
            cerror(ci.stream, ri.method, "501", "Not Implemented",
                    "This method is not implemented");
            client_disconnect(&ci);
            continue;
        }

        /* Get data from the high side */
        if (load_web_content(&data) != 0) {
            cerror(ci.stream, ri.method, "501", "No data", "Data load fail");
            client_disconnect(&ci);
            continue;
        }

        /* Serve low content */
        int ret = serve_static_content(&ci, &ri, data.buf, data.len);
        if  (ret < 0) {
            client_disconnect(&ci);
            continue;
        }
        printf("%s served %d bytes of web content\n", NAME, data.len);

        client_disconnect(&ci);
    }

    return 0;
}


int main(int argc, char* argv[]) {
    /* Validate and parse command-line options */
    if (argc != 2) {
        fprintf(stderr, "usage: %s <port>\n", argv[0]);
        return -1;
    }

    const short port = atoi(argv[1]);
    printf("\n%s web server on port %d\n\n", NAME, port);

#ifdef HIGH
    if (run_gaps() != 0) {
        fprintf(stderr, "Failed to start the GAPS handler");
        return -1;
    }
#endif

    /* Web server */
    return web_server(port);
}
