#include <errno.h>
#include <libpirate.h>
#include <limits.h>
#include <pal/envelope.h>
#include <pal/pal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/*
 * Application resource getters
 */

int get_pal_fd()
{
    long res;
    char *fdstr, *endptr;

    if(!(fdstr = getenv("PAL_FD")))
        return -1;

    errno = 0;
    res = strtol(fdstr, &endptr, 10);

    if(errno || *endptr || res > INT_MAX || res < 0)
        return -1;

    return res;
}

// FIXME: Do we want to sort these in clang so we can search smarter here?
char *lookup_pirate_resource_param(struct pirate_resource *pr,
        const char *name)
{
    size_t i;

    for(i = 0; i < pr->pr_params_len; ++i)
        if(!strcmp(pr->pr_params[i].prp_name, name))
            return pr->pr_params[i].prp_value;

    return NULL;
}

int get_boolean_res(int fd, const char *name, bool *outp)
{
    int res = 0;
    pal_env_t env = EMPTY_PAL_ENV(PAL_NO_TYPE);

    if((res = pal_send_resource_request(fd, "boolean", name, 0)))
        ;
    else if((res = pal_recv_env(fd, &env, 0)))
        ;
    else if(env.type != PAL_RESOURCE)
        res = 1;
    else {
        pal_env_iterator_t it = pal_env_iterator_start(&env);

        if(pal_env_iterator_size(it) != sizeof(*outp))
            res = 1;
        else
            memcpy(outp, pal_env_iterator_data(it), sizeof(*outp));
    }

    pal_free_env(&env);

    return res;
}

int get_integer_res(int fd, const char *name, int64_t *outp)
{
    int res = 0;
    pal_env_t env = EMPTY_PAL_ENV(PAL_NO_TYPE);

    if((res = pal_send_resource_request(fd, "integer", name, 0)))
        ;
    else if((res = pal_recv_env(fd, &env, 0)))
        ;
    else if(env.type != PAL_RESOURCE)
        res = 1;
    else {
        pal_env_iterator_t it = pal_env_iterator_start(&env);

        if(pal_env_iterator_size(it) != sizeof(*outp))
            res = 1;
        else
            memcpy(outp, pal_env_iterator_data(it), sizeof(*outp));
    }

    pal_free_env(&env);

    return res;
}

int get_string_res(int fd, const char *name, char **outp)
{
    int res = 0;
    size_t size;
    pal_env_t env = EMPTY_PAL_ENV(PAL_NO_TYPE);

    if((res = pal_send_resource_request(fd, "string", name, 0)))
        ;
    if((res = pal_recv_env(fd, &env, 0)))
        ;
    else if(env.type != PAL_RESOURCE)
        res = 1;
    else {
        pal_env_iterator_t it = pal_env_iterator_start(&env);

        size = pal_env_iterator_size(it);
        if(!(*outp = malloc(size + 1)))
            res = -errno;
        else {
            memcpy(*outp, pal_env_iterator_data(it), size);
            (*outp)[size] = '\0';
        }
    }

    pal_free_env(&env);

    return res;
}

int get_file_res(int fd, const char *name, int *outp)
{
    int res = 0;
    pal_env_t env = EMPTY_PAL_ENV(PAL_NO_TYPE);

    if((res = pal_send_resource_request(fd, "file", name, 0)))
        ;
    else if((res = pal_recv_env(fd, &env, 0)))
        ;
    else if(env.type != PAL_RESOURCE)
        res = 1;
    else if(env.fds_count != 1)
        res = 1;
    else
        *outp = env.fds[0];

    pal_free_env(&env);

    return res;
}

int get_pirate_channel_cfg(int fd, const char *name, char **outp)
{
    int res = 0;
    size_t size;
    pal_env_t env = EMPTY_PAL_ENV(PAL_NO_TYPE);

    if((res = pal_send_resource_request(fd, "pirate_channel", name, 0)))
        ;
    if((res = pal_recv_env(fd, &env, 0)))
        ;
    else if(env.type != PAL_RESOURCE)
        res = 1;
    else {
        pal_env_iterator_t it = pal_env_iterator_start(&env);

        size = pal_env_iterator_size(it);
        if(!(*outp = malloc(size + 1)))
            res = -errno;
        else {
            memcpy(*outp, pal_env_iterator_data(it), size);
            (*outp)[size] = '\0';
        }
    }

    pal_free_env(&env);

    return res;
}

/*
 * Automatic resource initializers
 */

typedef int (*get_func_t)(int fd, const char *name, void *datap);

static void init_resources_common(const char *type, get_func_t get_func,
        struct pirate_resource *start, struct pirate_resource *stop)
{
    int fd;
    struct pirate_resource *pr;

    if(start == stop)
        return; // No resources present

    if((fd = get_pal_fd()) < 0) {
        fputs("PAL resources declared, but no PAL_FD present in environment. "
                "Are we running with PAL?\n", stderr);
        exit(1);
    }

    for(pr = start; pr < stop; ++pr) {
        int err;

        if(!pr || !pr->pr_name || !pr->pr_obj)
            fprintf(stderr, "Invalid pirate resource section for `%s'\n",
                    type);

        if((err = get_func(fd, pr->pr_name, pr->pr_obj))) {
            fprintf(stderr, "Fatal error getting %s resource %s: %s\n",
                    type, pr->pr_name,
                    err > 0 ? pal_strerror(err) : strerror(-err));
            exit(1);
        }
    }
}

extern struct pirate_resource __start_pirate_res_string[];
extern struct pirate_resource __stop_pirate_res_string[];

void __attribute__((constructor)) init_string_resources()
{
    init_resources_common("string", (get_func_t)&get_string_res,
            __start_pirate_res_string, __stop_pirate_res_string);
}

extern struct pirate_resource __start_pirate_res_integer[];
extern struct pirate_resource __stop_pirate_res_integer[];

void __attribute__((constructor)) init_integer_resources()
{
    init_resources_common("integer", (get_func_t)&get_integer_res,
            __start_pirate_res_integer, __stop_pirate_res_integer);
}

extern struct pirate_resource __start_pirate_res_boolean[];
extern struct pirate_resource __stop_pirate_res_boolean[];

void __attribute__((constructor)) init_boolean_resources()
{
    init_resources_common("boolean", (get_func_t)&get_boolean_res,
            __start_pirate_res_boolean, __stop_pirate_res_boolean);
}

extern struct pirate_resource __start_pirate_res_file[];
extern struct pirate_resource __stop_pirate_res_file[];

void __attribute__((constructor)) init_file_resources()
{
    init_resources_common("file", (get_func_t)&get_file_res,
            __start_pirate_res_file, __stop_pirate_res_file);
}

extern struct pirate_resource __start_pirate_res_pirate_channel[];
extern struct pirate_resource __stop_pirate_res_pirate_channel[];

void __attribute__((constructor)) init_pirate_channel_resources()
{
    int fd;
    struct pirate_resource *pr;
    struct pirate_resource *start = __start_pirate_res_pirate_channel;
    struct pirate_resource *stop = __stop_pirate_res_pirate_channel;

    if(start == stop)
        return; // No resources present

    if((fd = get_pal_fd()) < 0) {
        fputs("PAL resources declared, but no PAL_FD present in environment. "
                "Are we running with PAL?\n", stderr);
        exit(1);
    }

    for(pr = start; pr < stop; ++pr) {
        int err, perms = O_RDWR;
        char *cfg, *permstr;

        if(!pr || !pr->pr_name || !pr->pr_obj)
            fputs("Invalid pirate resource section for `pirate_channel'\n",
                    stderr);

        if((err = get_pirate_channel_cfg(fd, pr->pr_name, &cfg))) {
            fprintf(stderr, "Fatal error getting pirate_channel %s: %s\n",
                    pr->pr_name,
                    err > 0 ? pal_strerror(err) : strerror(-err));
            exit(1);
        }

        if((permstr = lookup_pirate_resource_param(pr, "permissions"))) {
            if(!strcmp(permstr, "readonly"))
                perms = O_RDONLY;
            else if(!strcmp(permstr, "writeonly"))
                perms = O_WRONLY;
        }

        if((*(int *)pr->pr_obj = pirate_open_parse(cfg, perms)) < 0) {
            fprintf(stderr, "Fatal error opening pirate_channel %s: %s\n",
                    pr->pr_name, strerror(errno));
            exit(1);
        }

        free(cfg);
    }
}
