/**
 * \file
 * \author Jakub Antonín Štigler <xstigl00@stud.fit.vutbr.cz>
 * \brief Connection to ipfix producer over TCP (header file)
 * \date 2024
 *
 * Copyright: (C) 2023 CESNET, z.s.p.o.
 * SPDX-License-Identifier: BSD-3-Clause
 */

#pragma once

#include <memory>  // std::unique_ptr

#include <ipfixcol2.h> // ipx_ctx_t, ipx_session

#include "ByteVector.hpp"     // ByteVector
#include "Decoder.hpp"        // Decoder
#include "DecoderFactory.hpp"
#include "UniqueFd.hpp"       // UniqueFd

namespace tcp_in {

/** Connection to ipfix producer over TCP */
class Connection {
public:
    /**
     * @brief Creates new connection with this TCP connection file descriptor.
     * @param fd File descriptor of the new tcp connection.
     * @param factory The decoder factory to decide the decoder of this connection
     * @throws when fails to create new session
     */
    Connection(UniqueFd fd, DecoderFactory& factory, ipx_ctx_t *ctx);

    Connection(const Connection &) = delete;

    /**
     * @brief Reads from the TCP session while there is data.
     * @param ctx Ipx context (used to send the readed message)
     * @returns false when eof has been reached
     * @throws when invalid data is received, when connection throws, it should be closed.
     */
    bool receive(ipx_ctx_t *ctx);

    /**
     * @brief Closes the session of this connection.
     * @param ctx plugin context
     * @throws when fails to create message about destroying the session
     */
    void close(ipx_ctx_t *ctx);

    /**
     * @brief Gets the file descriptor of the TCP connection
     * @return int TCP file descriptor
     */
    inline int get_fd() const noexcept {
        return m_fd.get();
    }

    inline const ipx_session *get_session() const noexcept {
        return m_session;
    }

    inline const Decoder &get_decoder() const noexcept {
        return *m_decoder;
    }

    ~Connection();

private:
    void send_msg(ipx_ctx_t *ctx, ByteVector &&msg);

    /** TCP file descriptor */
    UniqueFd m_fd;
    /** Decoder factory */
    DecoderFactory &m_factory;
    /** Plugin context for logging */
    ipx_ctx_t *m_ctx;
    /** The session identifier */
    ipx_session *m_session = nullptr;
    /** true if this connection didn't receive any full messages, otherwise false. */
    bool m_new_connnection = true;
    /** selected decoder or nullptr. */
    std::unique_ptr<Decoder> m_decoder = nullptr;
};

} // namespace tcp_in
