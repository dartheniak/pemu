//
// Host name lookup with the PS4's own resolver (libSceNet).
// curl's built-in lookup (getaddrinfo / resolver thread) crashes the app on the PS4,
// so on that platform every host is resolved here and handed to curl as an ip.
//

#ifndef PEMU_PS4_DNS_H
#define PEMU_PS4_DNS_H

#include <string>

namespace pemu {
    // "http://host:port/path" -> host, port (80/443 by default). False if the url has no host.
    bool urlHost(const std::string &url, std::string *host, int *port);

    // resolve "host" to a dotted ipv4 address. Hosts already numeric are returned as is.
    // Results are cached. Only implemented on the PS4 (returns false elsewhere).
    bool resolveHost(const std::string &host, std::string *ip, std::string *error);
}

#endif //PEMU_PS4_DNS_H
