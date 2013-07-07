/*
 * Copyright (c) 2010-2012 Karl N. Redgate
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 */

#include <sys/ioctl.h>
#include <net/if.h>
#include <linux/if_tun.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <fcntl.h>

#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int debug = 0;

struct dirstats {
    long int pkts;
    long int octets;
    long int dropped;
};

struct tunnel_stats {
    int epoch;
    int written;
    struct dirstats rx;
    struct dirstats tx;
};

/*
 */
static void
idle( struct tunnel_stats *s ) {
    if ( debug ) printf( "idle\n" );
    if ( s->epoch == s->written ) return;
    FILE *f = fopen( ".link-stats", "w" );
    if ( f == NULL ) {
        return;
    }
    fprintf( f, "%ld %ld %ld %ld\n", s->tx.pkts, s->tx.octets,
                                     s->rx.pkts, s->rx.octets );
    fclose( f );
    rename( ".link-stats", "link-stats" );
    s->written = s->epoch;
    printf( "stats updated\n" );
}

/*
 */
static ssize_t
forward( int in, int out, struct dirstats *s ) {
    unsigned char buffer[2048];

    ssize_t octets = read( in, buffer, sizeof(buffer) );

    ssize_t written = write( out, buffer, octets );

    if ( written < 0 ) {
        perror( "write" );
    }

    if ( written < octets ) {
        perror( "write" );
    }

    s->pkts++;
    s->octets += octets;

    return written;
}

/*
 */
static void
loop( int tap, int sock ) {
    struct tunnel_stats s;
    memset( &s, 0, sizeof(s) );

    struct timeval timeout;
    fd_set fds;

    for (;;) {
        timeout.tv_sec = 5;
        timeout.tv_usec = 0;

        FD_ZERO( &fds );
        FD_SET( sock, &fds );
        FD_SET( tap, &fds );

        int result = select( 32, &fds, NULL, NULL, &timeout );
        if ( result == -1 ) {
            perror( "select" );
            continue;
        }

        if ( result == 0 ) {
            idle( &s );
            continue;
        }

        if ( FD_ISSET(tap, &fds) ) {
            ssize_t octets = forward( tap, sock, &(s.tx) );
            s.epoch += 1;
            if ( debug ) printf( "tap->sock %zd octets\n", octets );
        }

        if ( FD_ISSET(sock, &fds) ) {
            ssize_t octets = forward( sock, tap, &(s.rx) );
            s.epoch += 1;
            if ( debug ) printf( "sock->tap %zd octets\n", octets );
        }
    }
}

/*
 */
static int
tap_open( char *name ) {
    int tap = open( "/dev/net/tun", O_RDWR );
    if ( tap < 0 ) {
        perror( "open tunnel driver" );
        exit( -1 );
    }

    struct ifreq request;
    memset( &request, 0, sizeof(request) );
    request.ifr_flags = IFF_TAP | IFF_NO_PI;
    snprintf( request.ifr_name, IFNAMSIZ, "%s", name );

    if ( ioctl(tap, TUNSETIFF, &request) < 0 ) {
        perror( "create tunnel device" );
        exit( -1 );
    }

    return tap;
}

/*
 */
static struct addrinfo*
parse( char *address, char *service ) {
    struct addrinfo hints = {
        .ai_family = AF_UNSPEC,
        .ai_socktype = SOCK_DGRAM,
        .ai_protocol = 0,
        .ai_flags = (AI_NUMERICSERV | AI_ADDRCONFIG),
    };

    struct addrinfo *info;
    int error = getaddrinfo( address, service, &hints, &info );

    switch ( error ) {
    case 0: break;
    case EAI_SERVICE:
        fprintf( stderr, "bad service '%s'\n", service );
        exit( -1 );
    case EAI_NONAME:
        fprintf( stderr, "bad address '%s'\n", address );
        exit( -1 );
    default:
        fprintf( stderr, "address parse error = %d\n", error );
        exit( -1 );
    }

    if ( info == NULL ) {
        fprintf( stderr, "cannot translate: %s:%s\n", address, service );
        exit( -1 );
    }

    return info;
}

/*
 */
static int
socket_open( char *laddr, char *lport, char *raddr, char *rport ) {

    int error;

    struct addrinfo *local  = parse( laddr, lport );
    struct addrinfo *remote = parse( raddr, rport );

    int sock = socket( local->ai_family, local->ai_socktype, local->ai_protocol );
    if ( sock < 0 ) {
        perror( "socket" );
        exit( -1 );
    }

    /* Bind to local port */
    error = bind( sock, local->ai_addr, local->ai_addrlen );
    if ( error < 0 ) {
        perror( "bind" );
        exit( -1 );
    }

    /* Bind to remote port */
    error = connect( sock, remote->ai_addr, remote->ai_addrlen );
    if ( error < 0 ) {
        perror( "connect" );
        exit( -1 );
    }

    return sock;
}

/*
 */
int
main( int argc, char **argv ) {
    if ( argc < 6 ) {
        fprintf( stderr, "usage: %s interface-name"
                                  " local-address local-port"
                                  " remote-address remote-port\n", argv[0] );
        exit( -1 );
    }

    int tap = tap_open( argv[1] );
    int sock = socket_open( argv[2], argv[3], argv[4], argv[5] );

    loop( tap, sock );

    return 0;
}

/*
 * vim:autoindent
 * vim:expandtab
 */
