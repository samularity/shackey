/* libssh2 build configuration for ESP-IDF (lwIP + newlib + mbedTLS) */
#ifndef LIBSSH2_CONFIG_H
#define LIBSSH2_CONFIG_H

#ifndef LIBSSH2_MBEDTLS
#define LIBSSH2_MBEDTLS
#endif

/* Headers */
#define HAVE_UNISTD_H
#define HAVE_INTTYPES_H
#define HAVE_SYS_SELECT_H
#define HAVE_SYS_SOCKET_H
#define HAVE_SYS_IOCTL_H
#define HAVE_SYS_TIME_H
#define HAVE_ARPA_INET_H
#define HAVE_NETINET_IN_H

/* Socket non-blocking support */
#define HAVE_O_NONBLOCK

#endif /* LIBSSH2_CONFIG_H */
