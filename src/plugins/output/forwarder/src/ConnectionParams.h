/**
 * \file src/plugins/output/forwarder/src/ConnectionParams.h
 * \author Michal Sedlak <xsedla0v@stud.fit.vutbr.cz>
 * \brief Parameters for estabilishing connection
 * \date 2021
 */

/* Copyright (C) 2021 CESNET, z.s.p.o.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in
 *    the documentation and/or other materials provided with the
 *    distribution.
 * 3. Neither the name of the Company nor the names of its contributors
 *    may be used to endorse or promote products derived from this
 *    software without specific prior written permission.
 *
 * ALTERNATIVELY, provided that this notice is retained in full, this
 * product may be distributed under the terms of the GNU General Public
 * License (GPL) version 2 or later, in which case the provisions
 * of the GPL apply INSTEAD OF those given above.
 *
 * This software is provided ``as is'', and any express or implied
 * warranties, including, but not limited to, the implied warranties of
 * merchantability and fitness for a particular purpose are disclaimed.
 * In no event shall the company or contributors be liable for any
 * direct, indirect, incidental, special, exemplary, or consequential
 * damages (including, but not limited to, procurement of substitute
 * goods or services; loss of use, data, or profits; or business
 * interruption) however caused and on any theory of liability, whether
 * in contract, strict liability, or tort (including negligence or
 * otherwise) arising in any way out of the use of this software, even
 * if advised of the possibility of such damage.
 *
 */

#pragma once

#include <netdb.h>
#include <unistd.h>

#include <string>
#include <memory>
#include <cstdint>
#include <cerrno>
#include <cstring>

using unique_addrinfo = std::unique_ptr<addrinfo, decltype(&freeaddrinfo)>;

enum class TransProto { Tcp, Udp };

struct ConnectionParams
{
    ConnectionParams(std::string address, std::string port, TransProto protocol)
    : address(address)
    , port(port)
    , protocol(protocol)
    {
    }

    unique_addrinfo 
    resolve_address()
    {
        addrinfo *info;
        addrinfo hints = {};
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = (protocol == TransProto::Tcp ? SOCK_STREAM : SOCK_DGRAM);
        hints.ai_protocol = (protocol == TransProto::Tcp ? IPPROTO_TCP : IPPROTO_UDP);
        if (getaddrinfo(address.c_str(), port.c_str(), &hints, &info) == 0) {
            return unique_addrinfo(info, &freeaddrinfo);
        } else {
            return unique_addrinfo(NULL, &freeaddrinfo);
        }
    }

    int 
    make_socket()
    {
        auto address_info = resolve_address();
        if (!address_info) {
            return -1;
        }

        int sockfd;
        addrinfo *p;

        for (p = address_info.get(); p != NULL; p = p->ai_next) {
            sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
            if (sockfd < 0) {
                continue;
            }

            if (protocol == TransProto::Udp) {
                sockaddr_in sa = {};
                sa.sin_family = AF_INET;
                sa.sin_port = 0;
                sa.sin_addr.s_addr = INADDR_ANY;
                sa.sin_port = 0;
                if (bind(sockfd, (sockaddr *)&sa, sizeof(sa)) != 0) {
                    close(sockfd);
                    continue;
                } 
            }

            if (connect(sockfd, p->ai_addr, p->ai_addrlen) != 0) {
                close(sockfd);
                continue;
            }
   
            break;
        }

        if (!p) {
            return -1;
        }
        
        return sockfd;
    }

    std::string 
    str()
    {
        return std::string(protocol == TransProto::Tcp ? "TCP" : "UDP") + ":" 
               + address + ":" 
               + port; 
    }

    std::string address;
    std::string port;
    TransProto protocol;
};