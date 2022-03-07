#include "network.h"

#include <sys/epoll.h>
static void net_set_error(char *err, const char *fmt, ...)
{
    va_list ap;
    if (!err)
        return;
    va_start(ap, fmt);
    vsnprintf(err, NET_ERR_LEN, fmt, ap);
    va_end(ap);
}
int net_set_reuse_addr(char *err, int fd)
{
    int yes = 1;
    /* Make sure connection-intensive things like the pressure test
     * will be able to close/open sockets a hundred of times */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes)) == -1) {
        net_set_error(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_set_reuse_port(char *err, int fd)
{
    int yes = 1;
    /* Make sure connection-intensive things like the pressure test
     * will be able to close/open sockets a hundred of times */
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &yes, sizeof(yes)) == -1) {
        net_set_error(err, "setsockopt SO_REUSEADDR: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_set_tcp_no_nagle(char *err, int fd)
{
    int yes = 1;
    /* Make sure connection-intensive things like the pressure test
     * will be able to close/open sockets a hundred of times */
    if (setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &yes, sizeof(yes)) == -1) {
        net_set_error(err, "setsockopt TCP_NODELAY: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
/* At present, only tcp socket will need to set a
 * large enough mss value to accept a large enough packet size at one time */
int net_set_mss_value(char *err, int fd, int mss_value)
{
    int mss = mss_value;
    if (setsockopt(fd, IPPROTO_TCP, TCP_MAXSEG, &mss, sizeof(mss)) == -1) {
        net_set_error(err, "setsockopt TCP_MAXSEG: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_set_recv_buf_size(char *err, int fd, int recv_size)
{
    int _recv_size = recv_size;
    if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &_recv_size,
                   sizeof(_recv_size)) == -1) {
        net_set_error(err, "setsockopt SO_RCVBUF: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_set_send_buf_size(char *err, int fd, int send_size)
{
    int _send_size = send_size;
    if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF, &_send_size,
                   sizeof(_send_size)) == -1) {
        net_set_error(err, "setsockopt SO_SNDBUF: %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_non_block(char *err, int fd)
{
    int flags;
    /* Set the socket  non-blocking.
     * Note that fcntl(2) for F_GETFL and F_SETFL can't be
     * interrupted by a signal. */
    if ((flags = fcntl(fd, F_GETFL)) == -1) {
        net_set_error(err, "fcntl(F_GETFL): %s", strerror(errno));
        return NET_ERR;
    }

    flags |= O_NONBLOCK;

    if (fcntl(fd, F_SETFL, flags) == -1) {
        net_set_error(err, "fcntl(F_SETFL, O_NONBLOCK): %s", strerror(errno));
        return NET_ERR;
    }
    return NET_OK;
}
int net_listen(char *err,
               int s,
               struct sockaddr *sa,
               socklen_t len,
               int backlog)
{
    if (bind(s, sa, len) == -1) {
        net_set_error(err, "bind: %s", strerror(errno));
        close(s);
        return NET_ERR;
    }

    if (listen(s, backlog) == -1) {
        net_set_error(err, "listen: %s", strerror(errno));
        close(s);
        return NET_ERR;
    }
    return NET_OK;
}
int net_UDP_server(char *err, int port, char *bindaddr)
{
    int s = -1, ret;
    char _port[6]; /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_flags = AI_PASSIVE; /* No effect if bindaddr != NULL */

    if ((ret = getaddrinfo(bindaddr, _port, &hints, &servinfo)) != 0) {
        net_set_error(err, "%s", gai_strerror(ret));
        return NET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;
        if (net_set_reuse_addr(err, s) == NET_ERR)
            goto error;
        if (net_set_reuse_port(err, s) == NET_ERR)
            goto error;
        if (bind(s, p->ai_addr, p->ai_addrlen) < 0) {
            close(s);
            continue;
        }
        goto end;
    }
    if (p == NULL) {
        net_set_error(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1)
        close(s);
    s = NET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
int net_TCP_server(char *err, int port, char *bindaddr, int backlog)
{
    int s = -1, ret;
    char _port[6]; /* strlen("65535") */
    struct addrinfo hints, *servinfo, *p;

    snprintf(_port, 6, "%d", port);
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE; /* No effect if bindaddr != NULL */

    if ((ret = getaddrinfo(bindaddr, _port, &hints, &servinfo)) != 0) {
        net_set_error(err, "%s", gai_strerror(ret));
        return NET_ERR;
    }
    for (p = servinfo; p != NULL; p = p->ai_next) {
        if ((s = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == -1)
            continue;

        if (net_set_reuse_addr(err, s) == NET_ERR)
            goto error;
        if (net_set_reuse_port(err, s) == NET_ERR)
            goto error;
        if (net_set_mss_value(err, s, 310) == NET_ERR)
            goto error;
        if (net_listen(err, s, p->ai_addr, p->ai_addrlen, backlog) == NET_ERR)
            s = NET_ERR;
        goto end;
    }
    if (p == NULL) {
        net_set_error(err, "unable to bind socket, errno: %d", errno);
        goto error;
    }

error:
    if (s != -1)
        close(s);
    s = NET_ERR;
end:
    freeaddrinfo(servinfo);
    return s;
}
int net_generic_accept(char *err,
                       int serversock,
                       struct sockaddr *sa,
                       socklen_t *len)
{
    int fd;
    while (1) {
        fd = accept(serversock, sa, len);
        if (net_set_tcp_no_nagle(err, fd) == NET_ERR)
            return NET_ERR;
        if (fd == -1) {
            if (errno == EINTR)
                continue;
            else {
                net_set_error(err, "accept: %s", strerror(errno));
                return NET_ERR;
            }
        }
        break;
    }
    return fd;
}
int net_TCP_accept(char *err,
                   int serversock,
                   char *ip,
                   size_t ip_len,
                   int *port)
{
    int fd;
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);
    if ((fd = net_generic_accept(err, serversock, (struct sockaddr *) &sa,
                                 &salen)) == -1)
        return NET_ERR;

    struct sockaddr_in *s = (struct sockaddr_in *) &sa;
    if (ip)
        inet_ntop(AF_INET, (void *) &(s->sin_addr), ip, ip_len);
    if (port)
        *port = ntohs(s->sin_port);

    return fd;
}
int net_UDP_accept(char *err,
                   int port,
                   char *recv_buf,
                   int listen_fd,
                   int MAX_BUF_LEN,
                   int *Is_Heartbeat,
                   struct sockaddr_in *heartbeat_addr)
{
    int cfd = -1, reuse = 1, ret;
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    int recv_bytes = recvfrom(listen_fd, recv_buf, MAX_BUF_LEN, 0,
                              (struct sockaddr *) &client_addr, &client_len);
    if (recv_bytes > 0) {
        if (htons(client_addr.sin_port) == Heartbeat_PORT) {
            *Is_Heartbeat = 1;
            heartbeat_addr->sin_addr = client_addr.sin_addr;
            heartbeat_addr->sin_port = client_addr.sin_port - ntohs(1);
        }
        cfd = socket(PF_INET, SOCK_DGRAM, 0);
        if (net_non_block(err, cfd) == NET_ERR)
            goto err;  // set non_blocking
        if (getsockname(listen_fd, (struct sockaddr *) &sin, &len) == -1) {
            net_set_error(err, "accept: %s", strerror(errno));
        }
        if (cfd >= 0) {
            if (net_set_reuse_addr(err, cfd) == NET_ERR)
                goto err;
            if (net_set_reuse_port(err, cfd) == NET_ERR)
                goto err;
            if (net_set_send_buf_size(err, cfd, 25600) == NET_ERR)
                goto err;
            ret = bind(cfd, (struct sockaddr *) &sin, sizeof(struct sockaddr));
            if (ret) {
                net_set_error(err, "bind: %s", strerror(errno));
            }
            client_addr.sin_family = PF_INET;
            if (connect(cfd, (struct sockaddr *) &client_addr,
                        sizeof(struct sockaddr)) == -1) {
                net_set_error(err, "connect: %s", strerror(errno));
                goto err;
            }
        } else {
            net_set_error(err, "udp_accept:: %s", strerror(errno));
            return -1;
        }
    }
    return cfd;
err:
    close(cfd);
}
int net_TCP_client(char *err, char *server_addr, int server_port)
{
    int sockfd;
    struct sockaddr_in dest;
    /*---Open socket for streaming---*/
    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        net_set_error(err, "creating connected socket: %s", strerror(errno));
    }
    /*---Initialize server address/port struct---*/
    bzero(&dest, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_port = htons(server_port);
    if (inet_pton(AF_INET, server_addr, &dest.sin_addr.s_addr) == 0) {
        net_set_error(err, "Initialize server address/port struct: %s",
                      strerror(errno));
        goto err;
    }
    if (net_set_reuse_addr(err, sockfd) == NET_ERR)
        goto err;
    if (net_set_reuse_port(err, sockfd) == NET_ERR)
        goto err;
    if (net_non_block(err, sockfd) == NET_ERR)
        goto err;

    /*---Connect to server---*/
    if (connect(sockfd, (struct sockaddr *) &dest, sizeof(dest)) != 0) {
        if (errno != EINPROGRESS) {
            net_set_error(err, "Connect to server: %s", strerror(errno));
            goto err;
        } else if (errno == EINPROGRESS) {
            printf("---nonblocking connection---\n");
            /*---create epoll event---*/
            int epfd, nfd;
            struct epoll_event ev, ev_ret[10];
            epfd = epoll_create(1);
            if (epfd < 0) {
                net_set_error(err, "Initialize epfd: %s", strerror(errno));
                goto err;
            }
            memset(&ev, 0, sizeof(ev));
            /*
                where connect() failed with EINPROGRESS (and only in this case),
                you have to wait for the socket to be writable
            */
            ev.events = EPOLLIN | EPOLLOUT | EPOLLET;
            ev.data.fd = sockfd;
            if (epoll_ctl(epfd, EPOLL_CTL_ADD, sockfd, &ev) != 0) {
                net_set_error(err, "epoll_ctl: %s", strerror(errno));
                goto err;
            }
            nfd = epoll_wait(epfd, ev_ret, 10, -1);  // timeout is 1 sec
            epoll_ctl(epfd, EPOLL_CTL_DEL, sockfd, NULL);
            close(epfd);
            if (nfd < 0 || !(ev.events & EPOLLOUT)) {
                net_set_error(err, "epoll return: %s", strerror(errno));
                goto err2;
            }
            int result;
            socklen_t result_len = sizeof(result);
            if (getsockopt(sockfd, SOL_SOCKET, SO_ERROR, &result, &result_len) <
                0) {
                net_set_error(err, "getsockopt: %s", strerror(errno));
                goto err2;
            }

            if (result != 0) {
                // connection failed; error code is in 'result'
                net_set_error(err, "Connect to server: %s", strerror(result));
                goto err2;
            }
            printf("Connect to server:%s:%d success\n", server_addr,
                   server_port);
            int n = send(sockfd, "RSU:10001", 15, 0);
        }
    }
    return sockfd;
err:
    return NET_ERR;
err2:
    close(sockfd);
    return NET_ERR;
}
/* Format an IP,port pair into something easy to parse. */
static int net_format_addr(char *buf, size_t buf_len, char *ip, int port)
{
    return snprintf(buf, buf_len, "%s:%d", ip, port);
}
static int net_peer_to_string(int fd, char *ip, socklen_t ip_len, int *port)
{
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (getpeername(fd, (struct sockaddr *) &sa, &salen) == -1) {
        goto error;
    }
    if (ip_len == 0) {
        goto error;
    }

    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *) &sa;
        if (ip) {
            inet_ntop(AF_INET, (void *) &(s->sin_addr), ip, ip_len);
        }
        if (port) {
            *port = ntohs(s->sin_port);
        }
    } else {
        goto error;
    }

    return 0;

error:
    if (ip) {
        if (ip_len >= 2) {
            ip[0] = '?';
            ip[1] = '\0';
        } else if (ip_len == 1) {
            ip[0] = '\0';
        }
    }
    if (port) {
        *port = 0;
    }
    return -1;
}
static int net_sock_name(int fd, char *ip, socklen_t ip_len, int *port)
{
    struct sockaddr_storage sa;
    socklen_t salen = sizeof(sa);

    if (getsockname(fd, (struct sockaddr *) &sa, &salen) == -1) {
        if (port)
            *port = 0;
        ip[0] = '?';
        ip[1] = '\0';
        return -1;
    }
    if (sa.ss_family == AF_INET) {
        struct sockaddr_in *s = (struct sockaddr_in *) &sa;
        if (ip) {
            inet_ntop(AF_INET, (void *) &(s->sin_addr), ip, ip_len);
        }
        if (port) {
            *port = ntohs(s->sin_port);
        }
    }

    return 0;
}
/* Like net_format_addr() but extract ip and port from the socket's peer. */
int net_format_peer(int fd, char *buf, size_t buf_len)
{
    char ip[INET6_ADDRSTRLEN];
    int port;

    net_peer_to_string(fd, ip, sizeof(ip), &port);

    return net_format_addr(buf, buf_len, ip, port);
}
int net_format_sock(int fd, char *buf, size_t buf_len)
{
    char ip[INET6_ADDRSTRLEN];
    int port;

    net_sock_name(fd, ip, sizeof(ip), &port);

    return net_format_addr(buf, buf_len, ip, port);
}
int net_TCP_read(int fd, char *buf, int count)
{
    int nread, tolen = 0;
    while (tolen != count) {
        nread = read(fd, buf, count - tolen);
        if (nread == 0) {
            return tolen;
        }
        if (nread == -1) {
            return -1;
        }
        tolen += nread;
        buf += nread;
    }

    return tolen;
}
size_t net_TCP_write(int fd, char *buf, int count)
{
    size_t nwritten, tolen = 0;
    while (tolen != count) {
        nwritten = write(fd, buf, count - tolen);
        if (nwritten == 0) {
            return tolen;
        }
        if (nwritten == -1) {
            return -1;
        }
        tolen += nwritten;
        buf += nwritten;
    }

    return tolen;
}