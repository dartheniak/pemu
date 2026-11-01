//
// Host name lookup for the PS4.
// curl's built-in lookup (getaddrinfo) and the PS4 system resolver (sceNetResolver*) both
// crash the app, so on the PS4 every host is resolved here with a tiny DNS client (udp query
// to public DNS servers) and handed to curl as an ip.
//

#ifndef PEMU_PS4_DNS_H
#define PEMU_PS4_DNS_H

#include <string>

namespace pemu {
    // "http://host:port/path" -> host, port (80/443 by default). False if the url has no host.
    bool urlHost(const std::string &url, std::string *host, int *port);

    // resolve "host" to a dotted ipv4 address. Hosts already numeric are returned as is.
    // Results are cached.
    bool resolveHost(const std::string &host, std::string *ip, std::string *error);
}

#endif //PEMU_PS4_DNS_H
