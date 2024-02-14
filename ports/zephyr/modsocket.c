/*
 * This file is part of the MicroPython project, http://micropython.org/
 *
 * The MIT License (MIT)
 *
 * Copyright (c) 2023 Mariano Goluboff
 * Copyright (c) 2017 Linaro Limited
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "py/mpconfig.h"
#ifdef CONFIG_NET_SOCKETS

#include "py/runtime.h"
#include "py/stream.h"

#include <stdio.h>
#include <errno.h>
#include <zephyr/kernel.h>
// Zephyr's generated version header
#include <version.h>
#include <zephyr/net/net_pkt.h>
#include <zephyr/net/dns_resolve.h>
#include <zephyr/net/socket.h>
#include <zephyr/posix/fcntl.h>

#if (CONFIG_NET_SOCKETS_SOCKOPT_TLS || CONFIG_NET_SOCKETS_OFFLOAD)
#include <zephyr/net/tls_credentials.h>
#endif

#define DEBUG_PRINT 1
#if DEBUG_PRINT // print debugging info
#define DEBUG_printf printf
#else // don't print debugging info
#define DEBUG_printf(...) (void)0
#endif

typedef struct _socket_obj_t {
    mp_obj_base_t base;
    int ctx;

    #define STATE_NEW 0
    #define STATE_CONNECTING 1
    #define STATE_CONNECTED 2
    #define STATE_PEER_CLOSED 3
    int8_t state;
    sa_family_t family;
} socket_obj_t;

STATIC const mp_obj_type_t socket_type;

// Helper functions

#define RAISE_ERRNO(x) { int _err = x; if (_err < 0) mp_raise_OSError(-_err); }
#define RAISE_SOCK_ERRNO(x) { if ((int)(x) == -1) mp_raise_OSError(errno); }

STATIC void socket_check_closed(socket_obj_t *socket) {
    if (socket->ctx == -1) {
        // already closed
        mp_raise_OSError(EBADF);
    }
}

STATIC void parse_inet_addr(socket_obj_t *socket, mp_obj_t addr_in, struct sockaddr *sockaddr) {
    // We employ the fact that port and address offsets are the same for IPv4 & IPv6
    struct sockaddr_in *sockaddr_in = (struct sockaddr_in *)sockaddr;

    mp_obj_t *addr_items;

    sockaddr_in->sin_family = socket->family;
    size_t address_array_size = (sockaddr_in->sin_family == AF_INET6) ? 4 : 2;
    mp_obj_get_array_fixed_n(addr_in, address_array_size, &addr_items);
    RAISE_ERRNO(net_addr_pton(sockaddr_in->sin_family, mp_obj_str_get_str(addr_items[0]), &sockaddr_in->sin_addr));
    sockaddr_in->sin_port = htons(mp_obj_get_int(addr_items[1]));
}

STATIC mp_obj_t format_inet_addr(struct sockaddr *addr, mp_obj_t port) {
    // We employ the fact that port and address offsets are the same for IPv4 & IPv6
    struct sockaddr_in6 *sockaddr_in6 = (struct sockaddr_in6 *)addr;
    char buf[40];
    net_addr_ntop(addr->sa_family, &sockaddr_in6->sin6_addr, buf, sizeof(buf));
    mp_obj_tuple_t *tuple = mp_obj_new_tuple(addr->sa_family == AF_INET ? 2 : 4, NULL);

    tuple->items[0] = mp_obj_new_str(buf, strlen(buf));
    // We employ the fact that port offset is the same for IPv4 & IPv6
    // not filled in
    // tuple->items[1] = mp_obj_new_int(ntohs(((struct sockaddr_in*)addr)->sin_port));
    tuple->items[1] = port;

    if (addr->sa_family == AF_INET6) {
        tuple->items[2] = MP_OBJ_NEW_SMALL_INT(0); // flow_info
        tuple->items[3] = MP_OBJ_NEW_SMALL_INT(sockaddr_in6->sin6_scope_id);
    }

    return MP_OBJ_FROM_PTR(tuple);
}

socket_obj_t *socket_new(void) {
    socket_obj_t *socket = m_new_obj_with_finaliser(socket_obj_t);
    socket->base.type = (mp_obj_t)&socket_type;
    socket->state = STATE_NEW;
    return socket;
}

// Methods

STATIC void socket_print(const mp_print_t *print, mp_obj_t self_in, mp_print_kind_t kind) {
    socket_obj_t *self = self_in;
    if (self->ctx == -1) {
        mp_printf(print, "<socket NULL>");
    } else {
        mp_printf(print, "<socket %p family=%d>", self->ctx, self->family);
    }
}

STATIC mp_obj_t socket_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 4, false);

    socket_obj_t *socket = socket_new();

    int family = AF_INET;
    int socktype = SOCK_STREAM;
    int proto = -1;

    if (n_args >= 1) {
        family = mp_obj_get_int(args[0]);
        if (n_args >= 2) {
            socktype = mp_obj_get_int(args[1]);
            if (n_args >= 3) {
                proto = mp_obj_get_int(args[2]);
            }
        }
    }

    if (proto == -1) {
        proto = IPPROTO_TCP;
        if (socktype != SOCK_STREAM) {
            proto = IPPROTO_UDP;
        }
    }

    socket->ctx = zsock_socket(family, socktype, proto);
    RAISE_SOCK_ERRNO(socket->ctx);
    socket->family = family;
    return MP_OBJ_FROM_PTR(socket);
}

#if CONFIG_NET_SOCKETS_OFFLOAD
STATIC mp_obj_t socket_tlswrap(size_t n_args, const mp_obj_t *pos_args, mp_map_t *kwargs) {
    enum { ARG_sec_tag, ARG_verify, ARG_hostname };
    
    static const mp_arg_t allowed_args[] = {
        { MP_QSTR_sec_tag, MP_ARG_REQUIRED | MP_ARG_INT },
        { MP_QSTR_verify, MP_ARG_INT, {.u_int = TLS_PEER_VERIFY_REQUIRED} },
        { MP_QSTR_hostname, MP_ARG_KW_ONLY | MP_ARG_OBJ, {.u_obj = mp_const_none} },
    };

    // parse args
    mp_arg_val_t args[MP_ARRAY_SIZE(allowed_args)];
    mp_arg_parse_all(n_args - 1, pos_args + 1, kwargs, MP_ARRAY_SIZE(allowed_args), allowed_args, args);

    socket_obj_t *socket = pos_args[0];
    sec_tag_t sec_tag = args[ARG_sec_tag].u_int;
    setsockopt(socket->ctx, SOL_TLS, TLS_SEC_TAG_LIST, &sec_tag, sizeof(sec_tag));
    int verify = args[ARG_verify].u_int;
    setsockopt(socket->ctx, SOL_TLS, TLS_PEER_VERIFY, &verify, sizeof(verify));
   
    if (args[ARG_hostname].u_obj != mp_const_none) {
        const char *hostname = mp_obj_str_get_str(args[ARG_hostname].u_obj);
        setsockopt(socket->ctx, SOL_TLS, TLS_HOSTNAME, hostname, strlen(hostname));
    }
    return mp_const_none;
}
MP_DEFINE_CONST_FUN_OBJ_KW(socket_tlswrap_obj, 1, socket_tlswrap);
#endif

STATIC mp_obj_t socket_bind(mp_obj_t self_in, mp_obj_t addr_in) {
    socket_obj_t *socket = self_in;
    socket_check_closed(socket);

    struct sockaddr sockaddr;
    parse_inet_addr(socket, addr_in, &sockaddr);

    int res = zsock_bind(socket->ctx, &sockaddr, sizeof(sockaddr));
    RAISE_SOCK_ERRNO(res);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_bind_obj, socket_bind);

STATIC mp_obj_t socket_connect(mp_obj_t self_in, mp_obj_t addr_in) {
    socket_obj_t *socket = self_in;
    socket_check_closed(socket);

    struct sockaddr sockaddr;
    parse_inet_addr(socket, addr_in, &sockaddr);

    int res = zsock_connect(socket->ctx, &sockaddr, sizeof(sockaddr));
    RAISE_SOCK_ERRNO(res);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_connect_obj, socket_connect);

// method socket.listen([backlog])
STATIC mp_obj_t socket_listen(size_t n_args, const mp_obj_t *args) {
    socket_obj_t *socket = args[0];
    socket_check_closed(socket);

    mp_int_t backlog = MICROPY_PY_SOCKET_LISTEN_BACKLOG_DEFAULT;
    if (n_args > 1) {
        backlog = mp_obj_get_int(args[1]);
        backlog = (backlog < 0) ? 0 : backlog;
    }

    int res = zsock_listen(socket->ctx, backlog);
    RAISE_SOCK_ERRNO(res);

    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_listen_obj, 1, 2, socket_listen);

STATIC mp_obj_t socket_accept(mp_obj_t self_in) {
    socket_obj_t *socket = self_in;
    socket_check_closed(socket);

    struct sockaddr sockaddr;
    socklen_t addrlen = sizeof(sockaddr);
    int ctx = zsock_accept(socket->ctx, &sockaddr, &addrlen);

    socket_obj_t *socket2 = socket_new();
    socket2->ctx = ctx;

    mp_obj_tuple_t *client = mp_obj_new_tuple(2, NULL);
    client->items[0] = MP_OBJ_FROM_PTR(socket2);
    // TODO
    client->items[1] = mp_const_none;

    return MP_OBJ_FROM_PTR(client);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(socket_accept_obj, socket_accept);

STATIC mp_uint_t sock_write(mp_obj_t self_in, const void *buf, mp_uint_t size, int *errcode) {
    socket_obj_t *socket = self_in;
    if (socket->ctx == -1) {
        // already closed
        *errcode = EBADF;
        return MP_STREAM_ERROR;
    }

    ssize_t len = zsock_send(socket->ctx, buf, size, 0);
    if (len == -1) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }

    return len;
}

STATIC mp_obj_t socket_send(mp_obj_t self_in, mp_obj_t buf_in) {
    mp_buffer_info_t bufinfo;
    mp_get_buffer_raise(buf_in, &bufinfo, MP_BUFFER_READ);
    int err = 0;
    mp_uint_t len = sock_write(self_in, bufinfo.buf, bufinfo.len, &err);
    if (len == MP_STREAM_ERROR) {
        mp_raise_OSError(err);
    }
    return mp_obj_new_int_from_uint(len);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_send_obj, socket_send);

STATIC mp_uint_t sock_read(mp_obj_t self_in, void *buf, mp_uint_t max_len, int *errcode) {
    socket_obj_t *socket = self_in;
    if (socket->ctx == -1) {
        // already closed
        *errcode = EBADF;
        return MP_STREAM_ERROR;
    }

    ssize_t recv_len = zsock_recv(socket->ctx, buf, max_len, 0);
    if (recv_len == -1) {
        *errcode = errno;
        return MP_STREAM_ERROR;
    }

    return recv_len;
}

STATIC mp_obj_t socket_recv(mp_obj_t self_in, mp_obj_t len_in) {
    mp_int_t max_len = mp_obj_get_int(len_in);
    vstr_t vstr;
    // +1 to accommodate for trailing \0
    vstr_init_len(&vstr, max_len + 1);

    int err;
    mp_uint_t len = sock_read(self_in, vstr.buf, max_len, &err);

    if (len == MP_STREAM_ERROR) {
        vstr_clear(&vstr);
        mp_raise_OSError(err);
    }

    if (len == 0) {
        vstr_clear(&vstr);
        return mp_const_empty_bytes;
    }

    vstr.len = len;
    return mp_obj_new_bytes_from_vstr(&vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_recv_obj, socket_recv);

STATIC mp_obj_t socket_setsockopt(size_t n_args, const mp_obj_t *args) {
    (void)n_args; // always 4
    mp_warning(MP_WARN_CAT(RuntimeWarning), "setsockopt() not implemented");
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_setsockopt_obj, 4, 4, socket_setsockopt);

STATIC mp_obj_t socket_setblocking(mp_obj_t self_in, mp_obj_t blocking) {
    socket_obj_t *socket = self_in;
    int ret = zsock_fcntl(socket->ctx, F_GETFL, 0);
    if (ret == -1) {
        mp_raise_OSError(-errno);
    }
    if (mp_obj_is_true(blocking)) {
        ret = zsock_fcntl(socket->ctx, F_SETFL, ret & ~(O_NONBLOCK));
    } else {
        ret = zsock_fcntl(socket->ctx, F_SETFL, ret | O_NONBLOCK);
    }
    if (ret == -1) {
        mp_raise_OSError(-errno);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_setblocking_obj, socket_setblocking);

STATIC mp_obj_t socket_makefile(size_t n_args, const mp_obj_t *args) {
    (void)n_args;
    return args[0];
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(socket_makefile_obj, 1, 3, socket_makefile);

STATIC mp_uint_t sock_ioctl(mp_obj_t o_in, mp_uint_t request, uintptr_t arg, int *errcode) {
    socket_obj_t *socket = o_in;
    (void)arg;
    switch (request) {
        case MP_STREAM_CLOSE:
            if (socket->ctx != -1) {
                int res = zsock_close(socket->ctx);
                RAISE_SOCK_ERRNO(res);
                if (res == -1) {
                    *errcode = errno;
                    return MP_STREAM_ERROR;
                }
                socket->ctx = -1;
            }
            return 0;

        default:
            *errcode = MP_EINVAL;
            return MP_STREAM_ERROR;
    }
}

#if CONFIG_PDN
STATIC mp_obj_t socket_pdn(mp_obj_t self_in, mp_obj_t id) {
    if (!mp_obj_is_int(id)) {
        mp_raise_TypeError(MP_ERROR_TEXT("ID must be an integer"));
    }
    socket_obj_t *socket = self_in;
    int pdn_id = mp_obj_get_int(id);
    int ret = setsockopt(socket->ctx, SOL_SOCKET, SO_BINDTOPDN, &pdn_id, sizeof(pdn_id));
    if (ret) {
        mp_raise_OSError(ret);
    }
    return mp_const_none;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(socket_pdn_obj, socket_pdn);
#endif

STATIC const mp_rom_map_elem_t socket_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR___del__), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_close), MP_ROM_PTR(&mp_stream_close_obj) },
    { MP_ROM_QSTR(MP_QSTR_bind), MP_ROM_PTR(&socket_bind_obj) },
    { MP_ROM_QSTR(MP_QSTR_connect), MP_ROM_PTR(&socket_connect_obj) },
    { MP_ROM_QSTR(MP_QSTR_listen), MP_ROM_PTR(&socket_listen_obj) },
    { MP_ROM_QSTR(MP_QSTR_accept), MP_ROM_PTR(&socket_accept_obj) },
    { MP_ROM_QSTR(MP_QSTR_send), MP_ROM_PTR(&socket_send_obj) },
    { MP_ROM_QSTR(MP_QSTR_recv), MP_ROM_PTR(&socket_recv_obj) },
    { MP_ROM_QSTR(MP_QSTR_setsockopt), MP_ROM_PTR(&socket_setsockopt_obj) },
    { MP_ROM_QSTR(MP_QSTR_setblocking), MP_ROM_PTR(&socket_setblocking_obj) },

    { MP_ROM_QSTR(MP_QSTR_read), MP_ROM_PTR(&mp_stream_read_obj) },
    { MP_ROM_QSTR(MP_QSTR_readinto), MP_ROM_PTR(&mp_stream_readinto_obj) },
    { MP_ROM_QSTR(MP_QSTR_readline), MP_ROM_PTR(&mp_stream_unbuffered_readline_obj) },
    { MP_ROM_QSTR(MP_QSTR_write), MP_ROM_PTR(&mp_stream_write_obj) },
    { MP_ROM_QSTR(MP_QSTR_makefile), MP_ROM_PTR(&socket_makefile_obj) },
    #if CONFIG_NET_SOCKETS_OFFLOAD
    { MP_ROM_QSTR(MP_QSTR_tlswrap), MP_ROM_PTR(&socket_tlswrap_obj) },
    #if CONFIG_PDN
    { MP_ROM_QSTR(MP_QSTR_pdn), MP_ROM_PTR(&socket_pdn_obj) },
    #endif
    #endif
};
STATIC MP_DEFINE_CONST_DICT(socket_locals_dict, socket_locals_dict_table);

STATIC const mp_stream_p_t socket_stream_p = {
    .read = sock_read,
    .write = sock_write,
    .ioctl = sock_ioctl,
};

STATIC MP_DEFINE_CONST_OBJ_TYPE(
    socket_type,
    MP_QSTR_socket,
    MP_TYPE_FLAG_NONE,
    make_new, socket_make_new,
    print, socket_print,
    protocol, &socket_stream_p,
    locals_dict, &socket_locals_dict
    );

//
// getaddrinfo() implementation
//

typedef struct _getaddrinfo_state_t {
    mp_obj_t result;
    struct k_sem sem;
    mp_obj_t port;
    int status;
} getaddrinfo_state_t;

void dns_resolve_cb(enum dns_resolve_status status, struct dns_addrinfo *info, void *user_data) {
    getaddrinfo_state_t *state = user_data;
    DEBUG_printf("dns status: %d\n", status);

    if (info == NULL) {
        if (status == DNS_EAI_ALLDONE) {
            status = 0;
        }
        state->status = status;
        k_sem_give(&state->sem);
        return;
    }

    mp_obj_tuple_t *tuple = mp_obj_new_tuple(5, NULL);
    tuple->items[0] = MP_OBJ_NEW_SMALL_INT(info->ai_family);
    // info->ai_socktype not filled
    tuple->items[1] = MP_OBJ_NEW_SMALL_INT(SOCK_STREAM);
    // info->ai_protocol not filled
    tuple->items[2] = MP_OBJ_NEW_SMALL_INT(IPPROTO_TCP);
    tuple->items[3] = MP_OBJ_NEW_QSTR(MP_QSTR_);
    tuple->items[4] = format_inet_addr(&info->ai_addr, state->port);
    mp_obj_list_append(state->result, MP_OBJ_FROM_PTR(tuple));
}

STATIC mp_obj_t mod_getaddrinfo(size_t n_args, const mp_obj_t *args) {
    mp_obj_t host_in = args[0], port_in = args[1];
    const char *host = mp_obj_str_get_str(host_in);
    mp_int_t family = AF_INET;
    if (n_args > 2) {
        family = mp_obj_get_int(args[2]);
    }
    getaddrinfo_state_t state;
    // Just validate that it's int
    (void)mp_obj_get_int(port_in);
    state.port = port_in;
    state.result = mp_obj_new_list(0, NULL);
    #if !CONFIG_NET_SOCKETS_OFFLOAD
    k_sem_init(&state.sem, 0, UINT_MAX);

    for (int i = 2; i--;) {
        int type = (family != AF_INET6 ? DNS_QUERY_TYPE_A : DNS_QUERY_TYPE_AAAA);
        RAISE_ERRNO(dns_get_addr_info(host, type, NULL, dns_resolve_cb, &state, 3000));
        k_sem_take(&state.sem, K_FOREVER);
        if (family != 0) {
            break;
        }
        family = AF_INET6;
    }

    // Raise error only if there's nothing to return, otherwise
    // it may be IPv4 vs IPv6 differences.
    mp_int_t len = MP_OBJ_SMALL_INT_VALUE(mp_obj_len(state.result));
    if (state.status != 0 && len == 0) {
        mp_raise_OSError(state.status);
    }
    #else // CONFIG_NET_SOCKETS_OFFLOAD
    struct addrinfo *ai = NULL;
    struct addrinfo hints = {
        .ai_family = family
    };

    RAISE_ERRNO(getaddrinfo(host, NULL, &hints, &ai));

    do {
        mp_obj_tuple_t *tuple = mp_obj_new_tuple(5, NULL);
        tuple->items[0] = MP_OBJ_NEW_SMALL_INT(ai->ai_addr->sa_family);
        tuple->items[1] = MP_OBJ_NEW_SMALL_INT(ai->ai_socktype);
        tuple->items[2] = MP_OBJ_NEW_SMALL_INT(ai->ai_protocol);
        tuple->items[3] = MP_OBJ_NEW_QSTR(MP_QSTR_);
        tuple->items[4] = format_inet_addr(ai->ai_addr, state.port);
        mp_obj_list_append(state.result, MP_OBJ_FROM_PTR(tuple));
    } while (ai->ai_next != NULL);
    freeaddrinfo(ai);
    #endif // !CONFIG_NET_SOCKETS_OFFLOAD
    return state.result;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_getaddrinfo_obj, 2, 3, mod_getaddrinfo);

#if !CONFIG_NET_SOCKETS_OFFLOAD
STATIC mp_obj_t pkt_get_info(void) {
    struct k_mem_slab *rx, *tx;
    struct net_buf_pool *rx_data, *tx_data;
    net_pkt_get_info(&rx, &tx, &rx_data, &tx_data);
    mp_obj_tuple_t *t = MP_OBJ_TO_PTR(mp_obj_new_tuple(4, NULL));
    t->items[0] = MP_OBJ_NEW_SMALL_INT(k_mem_slab_num_free_get(rx));
    t->items[1] = MP_OBJ_NEW_SMALL_INT(k_mem_slab_num_free_get(tx));
    t->items[2] = MP_OBJ_NEW_SMALL_INT(rx_data->avail_count);
    t->items[3] = MP_OBJ_NEW_SMALL_INT(tx_data->avail_count);
    return MP_OBJ_FROM_PTR(t);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_0(pkt_get_info_obj, pkt_get_info);
#endif

STATIC const mp_rom_map_elem_t mp_module_socket_globals_table[] = {
    { MP_ROM_QSTR(MP_QSTR___name__), MP_ROM_QSTR(MP_QSTR_socket) },
    // objects
    { MP_ROM_QSTR(MP_QSTR_socket), MP_ROM_PTR(&socket_type) },
    // class constants
    { MP_ROM_QSTR(MP_QSTR_AF_INET), MP_ROM_INT(AF_INET) },
    { MP_ROM_QSTR(MP_QSTR_AF_INET6), MP_ROM_INT(AF_INET6) },
    { MP_ROM_QSTR(MP_QSTR_AF_PACKET), MP_ROM_INT(AF_PACKET) },

    { MP_ROM_QSTR(MP_QSTR_SOCK_STREAM), MP_ROM_INT(SOCK_STREAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_DGRAM), MP_ROM_INT(SOCK_DGRAM) },
    { MP_ROM_QSTR(MP_QSTR_SOCK_RAW), MP_ROM_INT(SOCK_RAW) },
    #if CONFIG_NET_SOCKETS_OFFLOAD
    { MP_ROM_QSTR(MP_QSTR_IPPROTO_TLS_1_2), MP_ROM_INT(IPPROTO_TLS_1_2) },
    { MP_ROM_QSTR(MP_QSTR_TLS_PEER_VERIFY_NONE), MP_ROM_INT(TLS_PEER_VERIFY_NONE) },
    { MP_ROM_QSTR(MP_QSTR_TLS_PEER_VERIFY_OPTIONAL), MP_ROM_INT(TLS_PEER_VERIFY_OPTIONAL) },
    { MP_ROM_QSTR(MP_QSTR_TLS_PEER_VERIFY_REQUIRED), MP_ROM_INT(TLS_PEER_VERIFY_REQUIRED) },
    #endif

    { MP_ROM_QSTR(MP_QSTR_SOL_SOCKET), MP_ROM_INT(1) },
    { MP_ROM_QSTR(MP_QSTR_SO_REUSEADDR), MP_ROM_INT(2) },

    { MP_ROM_QSTR(MP_QSTR_getaddrinfo), MP_ROM_PTR(&mod_getaddrinfo_obj) },
    #if !CONFIG_NET_SOCKETS_OFFLOAD
    { MP_ROM_QSTR(MP_QSTR_pkt_get_info), MP_ROM_PTR(&pkt_get_info_obj) },
    #endif
};

STATIC MP_DEFINE_CONST_DICT(mp_module_socket_globals, mp_module_socket_globals_table);

const mp_obj_module_t mp_module_socket = {
    .base = { &mp_type_module },
    .globals = (mp_obj_dict_t *)&mp_module_socket_globals,
};

MP_REGISTER_EXTENSIBLE_MODULE(MP_QSTR_socket, mp_module_socket);

#endif // CONFIG_NET_SOCKETS
