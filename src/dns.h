/*
 * dns.h - Custom DNS resolver using MacTCP
 */

#ifndef DNS_H
#define DNS_H

#include "MacTCP.h"

/* DNS resolver return codes */
#define DNS_OK            0
#define DNS_ERR_FORMAT   -1   /* malformed hostname */
#define DNS_ERR_NXDOMAIN -2   /* host not found */
#define DNS_ERR_TIMEOUT  -3   /* DNS server didn't respond */
#define DNS_ERR_NETWORK  -4   /* network/UDP error */
#define DNS_ERR_SERVFAIL -5   /* DNS server error */

/*
 * Resolve a hostname to an IP address.
 * Tries UDP first, falls back to TCP if UDP times out.
 * dns_server is the IP of the DNS server to query.
 * Returns DNS_OK on success with *ip set to the resolved address.
 * Returns DNS_ERR_* on failure.
 */
short dns_resolve(const char *hostname, ip_addr *ip, ip_addr dns_server);

#endif /* DNS_H */
