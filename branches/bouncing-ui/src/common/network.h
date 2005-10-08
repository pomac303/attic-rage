#ifndef RAGE_NETWORK_H
#define RAGE_NETWORK_H

/* TODO: Replace this crap interface with a decent one.
 *
 * If the OS does not define struct addrinfo, this header should do
 * so, or define an equivalent.  It should provide asynchronous
 * versions of getaddrinfo() and getnameinfo() functions from RFC 2553
 * (and a corresponding freeaddrinfo() function).  Non-IPv6-capable
 * platforms should probably not return IPv6 addresses from the async
 * version of getaddrinfo().
 *
 * Because Windows is lame, it is simplest to make non-Windows OSes
 * "#define closesocket(X) close(X)" and use closesocket() to close
 * socket FDs.  net_connect(), net_bind() and net_sockets() would be
 * replaced with calls directly to the corresponding OS function.
 */

typedef struct netstore_ netstore;

void net_input_remove(int tag);
int net_input_add(int sok, int flags, void *func, void *data);
netstore *net_store_new (void);
void net_store_destroy (netstore *ns);
int net_connect (netstore *ns, int sok4, int sok6, int *sok_return);
char *net_resolve (netstore *ns, char *hostname, int port, char **real_host);
void net_bind (netstore *tobindto, int sok4, int sok6);
char *net_ip (unsigned long addr);
void net_sockets (int *sok4, int *sok6);

#endif
