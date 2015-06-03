// From: http://stackoverflow.com/questions/2616011/easy-way-to-parse-a-url-in-c-cross-platform
// http://stackoverflow.com/users/882436/tom
// CC-BY-SA license http://creativecommons.org/licenses/by-sa/3.0/
/*
    Re-formatted the code and added support for fragments.
    Kristina Simpson 2014
*/

#pragma once

#include <string>
#include <algorithm>

namespace uri 
{
    struct uri
    {
    public:
        std::string query_string() const { return query_string_; }
        std::string path()  const { return path_; }
        std::string protocol()  const { return protocol_; }
        std::string host() const { return host_; }
        std::string port() const { return port_; }
        std::string fragment() const { return fragment_; }

        static uri parse(const std::string &url)
        {
            uri result;

            typedef std::string::const_iterator iterator_t;

            if(url.length() == 0) {
                return result;
            }

            iterator_t uriEnd = url.end();

            // get query start
            iterator_t queryStart = std::find(url.begin(), uriEnd, '?');
            
            iterator_t fragmentStart = std::find(url.begin(), uriEnd, '#');

            // protocol
            iterator_t protocolStart = url.begin();
            iterator_t protocolEnd = std::find(protocolStart, uriEnd, ':');            //"://");

            if(protocolEnd != uriEnd) {
                std::string prot = &*(protocolEnd);
                if ((prot.length() > 3) && (prot.substr(0, 3) == "://")) {
                    result.protocol_ = std::string(protocolStart, protocolEnd);
                    protocolEnd += 3;   //      ://
                } else {
                    protocolEnd = url.begin();  // no protocol
                }
            } else {
                protocolEnd = url.begin();  // no protocol
            }

            // host
            iterator_t hostStart = protocolEnd;
            iterator_t pathStart = std::find(hostStart, uriEnd, '/');  // get pathStart

            iterator_t hostEnd = std::find(protocolEnd, 
                (pathStart != uriEnd) ? pathStart : queryStart,
                ':');  // check for port

            result.host_ = std::string(hostStart, hostEnd);

            // port
            if((hostEnd != uriEnd) && ((&*(hostEnd))[0] == ':')) { // we have a port
                ++hostEnd;
                iterator_t portEnd = (pathStart != uriEnd) ? pathStart : queryStart;
                result.port_ = std::string(hostEnd, portEnd);
            } else {
                result.port_ = "80";
            }

            // path
            if(pathStart != uriEnd) {
                result.path_ = std::string(pathStart, queryStart);
            }

            // query
            if(queryStart != uriEnd) {
                result.query_string_ = std::string(queryStart, fragmentStart);
            }
            
            if(fragmentStart != uriEnd) {
                result.fragment_ = std::string(fragmentStart, uriEnd);
            }

            return result;
        }

    private:
        std::string query_string_;
        std::string path_;
        std::string protocol_;
        std::string host_;
        std::string port_;
        std::string fragment_;
    };  // uri
}
