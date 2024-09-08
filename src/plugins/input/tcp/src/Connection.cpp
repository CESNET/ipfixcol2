/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Connection to ipfix producer over TCP (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#include "Connection.hpp"

#include <stdexcept> // runtime_error
#include <string>    // string, to_string
#include <cstdint>   // uint8_t

#include <sys/socket.h> // sockaddr_storage, socklen_t, getpeername, sockaddr, AF_INET, AF_INET6
#include <netinet/in.h> // ntohs, sockaddr_in, sockaddr_in6, in_addr, IN6_IS_ADDR_V4MAPPED

#include <ipfixcol2.h> // ipx_*, IPX_*, fds_ipfix_msg_hdr

#include "UniqueFd.hpp"   // UniqueFd
#include "Decoder.hpp"    // Decoder
#include "Connection.hpp" // Connection
#include "ByteVector.hpp" // ByteVector

namespace tcp_in {

Connection::Connection(UniqueFd fd, DecoderFactory &factory, ipx_ctx *ctx) :
    m_fd(std::move(fd)),
    m_factory(factory),
    m_ctx(ctx)
{
    const char *err_str;

    sockaddr_storage src_addr;
    socklen_t src_addr_len = sizeof(src_addr);
    if (getpeername(m_fd.get(), reinterpret_cast<struct sockaddr *>(&src_addr), &src_addr_len) == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to get local IP address: " + std::string(err_str));
    }

    sockaddr_storage dst_addr;
    socklen_t dst_addr_len = sizeof(dst_addr);
    if (getpeername(m_fd.get(), reinterpret_cast<struct sockaddr *>(&dst_addr), &dst_addr_len) == -1) {
        ipx_strerror(errno, err_str);
        throw std::runtime_error("Failed to get remote IP address: " + std::string(err_str));
    }

    if (src_addr.ss_family != dst_addr.ss_family) {
        throw std::runtime_error(
            "Local and remote IP address must be of the same type (IPv4/IPv6)"
        );
    }

    ipx_session_net net = {};

    net.l3_proto = src_addr.ss_family;
    if (net.l3_proto == AF_INET) { // IPv4
        const sockaddr_in *src4 = reinterpret_cast<const sockaddr_in *>(&src_addr);
        const sockaddr_in *dst4 = reinterpret_cast<const sockaddr_in *>(&dst_addr);
        net.port_src = ntohs(src4->sin_port);
        net.port_dst = ntohs(dst4->sin_port);
        net.addr_src.ipv4 = src4->sin_addr;
        net.addr_dst.ipv4 = dst4->sin_addr;
    } else if (net.l3_proto == AF_INET6) { // IPv6
        const sockaddr_in6 *src6 = reinterpret_cast<sockaddr_in6 *>(&src_addr);
        const sockaddr_in6 *dst6 = reinterpret_cast<sockaddr_in6 *>(&dst_addr);
        net.port_src = ntohs(src6->sin6_port);
        net.port_dst = ntohs(dst6->sin6_port);
        if (IN6_IS_ADDR_V4MAPPED(&src6->sin6_addr) && IN6_IS_ADDR_V4MAPPED(&dst6->sin6_addr)) {
            // IPv4 mapped into IPv6
            net.l3_proto = AF_INET; // Overwrite family type!
            net.addr_src.ipv4 = *reinterpret_cast<const in_addr *>(
                reinterpret_cast<const uint8_t *>(&src6->sin6_addr) + 12
            );
            net.addr_dst.ipv4 = *reinterpret_cast<const in_addr *>(
                reinterpret_cast<const uint8_t *>(&dst6->sin6_addr) + 12
            );
        } else {
            net.addr_src.ipv6 = src6->sin6_addr;
            net.addr_dst.ipv6 = dst6->sin6_addr;
        }
    } else {
        throw std::runtime_error(
            "New connection with an unsupported IP address family rejected, family id: "
                + std::to_string(src_addr.ss_family)
        );
    }

    m_session = ipx_session_new_tcp(&net);
}

bool Connection::receive(ipx_ctx_t *ctx) {
    if (!m_decoder) {
        m_decoder = m_factory.detect_decoder(m_fd.get());
        if (!m_decoder) {
            return true;
        }

        IPX_CTX_INFO(
            m_ctx,
            "Using %s Decoder for the new connection",
            m_decoder->get_name()
        );
    }

    auto &buffer = m_decoder->decode();
    buffer.process_decoded([=](ByteVector &&msg) { send_msg(ctx, std::move(msg)); });
    return !buffer.is_eof_reached();
}

void Connection::send_msg(ipx_ctx_t *ctx, ByteVector &&msg) {
    if (m_new_connnection) {
        // Send information about new transform session
        ipx_msg_session_t *session = ipx_msg_session_create(m_session, IPX_MSG_SESSION_OPEN);
        if (!session) {
            throw std::runtime_error(
                "Failed to create new message session, closing connection "
                    + std::string(m_session->ident)
            );
        }

        ipx_ctx_msg_pass(ctx, ipx_msg_session2base(session));
        m_new_connnection = false;
    }

    ipx_msg_ctx msg_ctx;
    msg_ctx.session = m_session;
    msg_ctx.odid = ntohl(reinterpret_cast<const fds_ipfix_msg_hdr *>(msg.data())->odid);
    msg_ctx.stream = 0; // Streams are not supported over TCP

    ipx_msg_ipfix_t *ipfix_msg = ipx_msg_ipfix_create(ctx, &msg_ctx, msg.data(), msg.size());
    if (!ipfix_msg) {
        throw std::runtime_error(
            "Failed to send message for session " + std::string(m_session->ident)
        );
    }

    ipx_ctx_msg_pass(ctx, ipx_msg_ipfix2base(ipfix_msg));

    // release the message data so that it is not freed by the destructor
    msg.take();
}

void Connection::close(ipx_ctx_t *ctx) {
    if (m_new_connnection) {
        ipx_session_destroy(m_session);
        m_session = nullptr;
        return;
    }

    ipx_msg_session_t *msg_session = ipx_msg_session_create(m_session, IPX_MSG_SESSION_CLOSE);
    if (!msg_session) {
        throw std::runtime_error(
            "Failed to create message for closing session " + std::string(m_session->ident)
        );
    }

    ipx_ctx_msg_pass(ctx, ipx_msg_session2base(msg_session));

    ipx_msg_garbage_cb cb = reinterpret_cast<ipx_msg_garbage_cb>(&ipx_session_destroy);
    ipx_msg_garbage_t *msg_garbage = ipx_msg_garbage_create(m_session, cb);
    if (!msg_garbage) {
        throw std::runtime_error(
            "Failed create garbage message for session " + std::string(m_session->ident)
        );
    }

    ipx_ctx_msg_pass(ctx, ipx_msg_garbage2base(msg_garbage));
    m_session = nullptr;
}

Connection::~Connection() {
    if (m_session && m_new_connnection) {
        ipx_session_destroy(m_session);
    }
}

} // namespace tcp_in
