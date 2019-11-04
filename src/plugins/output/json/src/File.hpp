/**
 * \file src/plugins/output/json/src/File.hpp
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Configuration of JSON output plugin (source file)
 * \date 2018
 */

/* Copyright (C) 2015-2018 CESNET, z.s.p.o.
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

#ifndef JSON_FILE_H
#define JSON_FILE_H

#include <atomic>
#include <string>
#include <ctime>

#include <pthread.h>
#include "Storage.hpp"
#include "Config.hpp"

/**
 * \brief The class for file output interface
 */
class File : public Output {
public:
    // Constructor & destructor
    File(const struct cfg_file &cfg, ipx_ctx_t *ctx);
    ~File();

    // Store a record to the file
    int process(const char *str, size_t len);

    void flush();
private:
    /** Minimal window size */
    const unsigned int _WINDOW_MIN_SIZE = 60; // seconds

    /** Configuration of a thread */
    typedef struct thread_ctx_s {
        ipx_ctx_t *ctx;              /**< Plugin instance context    */
        pthread_t thread;            /**< Thread                     */
        pthread_rwlock_t rwlock;     /**< Data rwlock                */
        std::atomic<bool> stop;      /**< Stop flag for termination  */

        unsigned int window_size;    /**< Size of a time window      */
        time_t window_time;          /**< Current time window        */
        std::string storage_path;    /**< Storage path (template)    */
        std::string file_prefix;     /**< File prefix                */
        calg m_calg;                 /**< Compression                */

        void *file;                  /**< File descriptor            */
    } thread_ctx_t;

    /** Thread for changing time windows */
    thread_ctx_t *_thread;

    // Get a directory path for a time window
    static int dir_name(const time_t &tm, const std::string &tmplt,
        std::string &dir);
    // Create a directory for a time window
    static int dir_create(ipx_ctx_t *ctx, const std::string &path);
    // Create a file for a time window
    static void *file_create(ipx_ctx_t *ctx, const std::string &tmplt, const std::string &prefix,
        const time_t &tm, calg m_calg);
    // Window changer
    static void *thread_window(void *context);
};

#endif // JSON_FILE_H

