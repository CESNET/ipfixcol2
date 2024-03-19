/**
 * \file src/plugins/output/json/src/File.cpp
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

#include <ipfixcol2.h>
#include "File.hpp"

#include <stdexcept>
#include <string>
#include <vector>
#include <cstring>
#include <cerrno>
#include <cstdio>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <climits>
#include <zlib.h>

/**
 * \brief Class constructor
 * \param[in] cfg Parsed configuration
 * \param[in] ctx Instance context
 */
File::File(const struct cfg_file &cfg, ipx_ctx_t *ctx) : Output(cfg.name, ctx)
{
    // Prepare a configuration of the thread for changing time windows
    _thread = new thread_ctx_t;
    _thread->file = nullptr;
    _thread->stop = false;

    _thread->ctx = ctx;
    _thread->storage_path = cfg.path_pattern;
    _thread->file_prefix = cfg.prefix;
    _thread->window_size = cfg.window_size;
    _thread->m_calg = cfg.m_calg;
    time(&_thread->window_time);

    if (cfg.window_size < _WINDOW_MIN_SIZE) {
        throw std::runtime_error("(File output) Window size is too small (min. size: "
            + std::to_string(_WINDOW_MIN_SIZE) + ")");
    }

    // Make sure the path ends with '/' character
    if (_thread->storage_path.back() != '/') {
        _thread->storage_path += '/';
    }

    if (cfg.window_align) {
        // Window alignment
        _thread->window_time = (_thread->window_time / _thread->window_size) *
            _thread->window_size;
    }

    // Create directory & first file
    void *new_file = file_create(ctx, _thread->storage_path, _thread->file_prefix,
        _thread->window_time, _thread->m_calg);
    if (!new_file) {
        delete _thread;
        throw std::runtime_error("(File output) Failed to create a time window file.");
    }

    _thread->file = new_file;

    pthread_rwlockattr_t attr;
    if (pthread_rwlockattr_init(&attr) != 0) {
        if (_thread->m_calg == calg::GZIP) {
            gzclose((gzFile)_thread->file);
        } else {
            fclose((FILE *)_thread->file);
        }
        delete _thread;
        throw std::runtime_error("(File output) Rwlockattr initialization failed!");
    }

#ifdef PTHREAD_RWLOCK_WRITER_NONRECURSIVE_INITIALIZER_NP
    // PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP avoids writer starvation
    // It is a non-portable GNU extension only available on Linux systems
    if (pthread_rwlockattr_setkind_np(&attr, PTHREAD_RWLOCK_PREFER_WRITER_NONRECURSIVE_NP) != 0) {
        if (_thread->m_calg == calg::GZIP) {
            gzclose((gzFile)_thread->file);
        } else {
            fclose((FILE *)_thread->file);
        }
        pthread_rwlockattr_destroy(&attr);
        delete _thread;
        throw std::runtime_error("(File output) Rwlockattr setkind failed!");
    }
#endif

    if (pthread_rwlock_init(&_thread->rwlock, &attr) != 0) {
        if (_thread->m_calg == calg::GZIP) {
            gzclose((gzFile)_thread->file);
        } else {
            fclose((FILE *)_thread->file);
        }
        pthread_rwlockattr_destroy(&attr);
        delete _thread;
        throw std::runtime_error("(File output) Rwlock initialization failed!");
    }

    pthread_rwlockattr_destroy(&attr);
    if (pthread_create(&_thread->thread, NULL, &File::thread_window, _thread) != 0) {
        if (_thread->m_calg == calg::GZIP) {
            gzclose((gzFile)_thread->file);
        } else {
            fclose((FILE *)_thread->file);
        }
        pthread_rwlock_destroy(&_thread->rwlock);
        delete _thread;
        throw std::runtime_error("(File output) Failed to start a thread for changing time "
            "windows.");
    }
}

/**
 * \brief Class destructor
 *
 * Close all opened files
 */
File::~File()
{
    if (_thread) {
        _thread->stop = true;
        pthread_join(_thread->thread, NULL);
        pthread_rwlock_destroy(&_thread->rwlock);

        if (_thread->file) {
            if (_thread->m_calg == calg::GZIP) {
                gzclose((gzFile)_thread->file);
            } else {
                fclose((FILE *)_thread->file);
            }
        }

        delete _thread;
    }
}

/**
 * \brief Thread function for changing time windows
 * \param[in,out] context Thread configuration
 * \return Nothing
 */
void *
File::thread_window(void *context)
{
    thread_ctx_t *data = (thread_ctx_t *) context;
    IPX_CTX_DEBUG(data->ctx, "(File output) Thread started...", '\0');

    while(!data->stop) {
        // Sleep
        struct timespec tim;
        tim.tv_sec = 0;
        tim.tv_nsec = 100000000L; // 0.1 sec
        nanosleep(&tim, NULL);

        // Get current time
        time_t now;
        time(&now);

        if (difftime(now, data->window_time) <= data->window_size) {
            continue;
        }

        // New time window
        pthread_rwlock_wrlock(&data->rwlock);
        if (data->file) {
            if (data->m_calg == calg::GZIP) {
                gzclose((gzFile)data->file);
            } else {
                fclose((FILE *)data->file);
            }
            data->file = nullptr;
        }

        data->window_time += data->window_size;
        void *file = file_create(data->ctx, data->storage_path, data->file_prefix, data->window_time, data->m_calg);
        if (!file) {
            IPX_CTX_ERROR(data->ctx, "(File output) Failed to create a time window file.", '\0');
        }

        // Null pointer is also valid...
        data->file = file;
        pthread_rwlock_unlock(&data->rwlock);
    }

    IPX_CTX_DEBUG(data->ctx, "(File output) Thread terminated.", '\0');
    return NULL;
}

/**
 * \brief Store a record to a file
 * \param[in] str JSON record
 * \param[in] len Length of the record
 * \return #IPX_OK on success
 * \return #IPX_ERR_DENIED in case of a fatal error (the output cannot continue)
 */
int
File::process(const char *str, size_t len)
{
    pthread_rwlock_rdlock(&_thread->rwlock);
    if (_thread->file) {
        // Store the record
        if (_thread->m_calg == calg::GZIP) {
            gzwrite((gzFile)_thread->file, str, len);
        } else {
            fwrite(str, len, 1, (FILE *)_thread->file);
        }
    }
    pthread_rwlock_unlock(&_thread->rwlock);
    return IPX_OK;
}

void
File::flush()
{
    pthread_rwlock_rdlock(&_thread->rwlock);
    if (_thread->file) {
        if (_thread->m_calg == calg::GZIP) {
            gzflush((gzFile)_thread->file, Z_SYNC_FLUSH);
        } else {
            fflush((FILE *)_thread->file);
        }
    }
    pthread_rwlock_unlock(&_thread->rwlock);
}

/**
 * \brief Get a directory path for a time window
 * \param[in]  tm    Time window
 * \param[in]  tmplt Template of the directory path
 * \param[out] dir   Directory path
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int
File::dir_name(const time_t &tm, const std::string &tmplt, std::string &dir)
{
    char dir_fmt[PATH_MAX];

    // Get UTC time
    struct tm gm;
    if (gmtime_r(&tm, &gm) == NULL) {
        return 1;
    }

    // Convert time template to a string
    if (strftime(dir_fmt, sizeof(dir_fmt), tmplt.c_str(), &gm) == 0) {
        return 1;
    }

    dir = std::string(dir_fmt);
    return 0;
}

/**
 * \brief Create a directory for a time window
 *
 * \warning
 *   Directory must ends with '/'. Otherwise only directories before last symbol '/' will be
 *   created.
 * \param[in] ctx  Instance context
 * \param[in] path Directory path
 * \return On success returns 0. Otherwise returns non-zero value.
 */
int
File::dir_create(ipx_ctx_t *ctx, const std::string &path)
{
    const mode_t mask = S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH;

    if (path.empty()) {
        return 1;
    }

    std::string tmp = path;
    std::size_t pos = std::string::npos;
    std::vector<std::string> mkdir_todo;
    bool stop = 0;

    // Try to create directories from the end
    while (!stop && (pos = tmp.find_last_of('/', pos)) != std::string::npos) {
        // Try to create a directory
        std::string aux_str = tmp.substr(0, pos + 1);
        if (aux_str == "/") {
            // Root
            IPX_CTX_ERROR(ctx, "(File output) Failed to create a storage directory '%s'.",
                path.c_str());
            return 1;
        }

        // Create a directory
        if (mkdir(aux_str.c_str(), mask) == 0) {
            // Directory created
            break;
        }

        // Failed to create the directory
        switch(errno) {
        case EEXIST:
            // Directory already exists
            stop = 1;
            break;
        case ENOENT:
            // Parent directory is missing
            mkdir_todo.push_back(aux_str);
            pos--;
            continue;
        default:
            // Other errors
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(ctx, "(File output) Failed to create a directory %s (%s).",
                aux_str.c_str(), err_str);
            return 1;
        }
    }

    // Create remaining directories
    while (!mkdir_todo.empty()) {
        std::string aux_str = mkdir_todo.back();
        mkdir_todo.pop_back();

        if (mkdir(aux_str.c_str(), mask) != 0) {
            // Failed to create directory
            const char *err_str;
            ipx_strerror(errno, err_str);
            IPX_CTX_ERROR(ctx, "(File output) Failed to create a directory %s (%s).",
                aux_str.c_str(), err_str);
            return 1;
        }
    }

    return 0;
}

/**
 * \brief Create a file for a time window
 *
 * Check/create a directory hierarchy and create a new file for time window.
 * \param[in] ctx    Instance context - only for logging
 * \param[in] tmplt  Output path template
 * \param[in] prefix File prefix
 * \param[in] tm     Timestamp
 * \return On success returns pointer to the file, Otherwise returns NULL.
 */
void *
File::file_create(ipx_ctx_t *ctx, const std::string &tmplt, const std::string &prefix,
    const time_t &tm, calg m_calg)
{
    char file_fmt[20];

    // Get UTC time
    struct tm gm;
    if (gmtime_r(&tm, &gm) == NULL) {
        IPX_CTX_ERROR(ctx, "(File output) Failed to convert time to UTC.", '\0');
        return NULL;
    }

    // Convert time template to a string
    if (strftime(file_fmt, sizeof(file_fmt), "%Y%m%d%H%M", &gm) == 0) {
        IPX_CTX_ERROR(ctx, "(File output) Failed to create a name of a flow file.", '\0');
        return NULL;
    }

    // Check/create a directory
    std::string directory;
    if (dir_name(tm, tmplt, directory) != 0) {
        IPX_CTX_ERROR(ctx, "(File output) Failed to process output path pattern!", '\0');
        return NULL;
    }

    if (dir_create(ctx, directory) != 0) {
        return NULL;
    }

    std::string file_name;
    void *file;
    if (m_calg == calg::GZIP) {
        file_name = directory + prefix + file_fmt + ".gz";
        file = gzopen(file_name.c_str(), "a9");
    } else {
        file_name = directory + prefix + file_fmt;
        file = fopen(file_name.c_str(), "a");
    }
    if (!file) {
        // Failed to create a flow file
        const char *err_str;
        ipx_strerror(errno, err_str);
        IPX_CTX_ERROR(ctx, "Failed to create a flow file '%s' (%s).", file_name.c_str(), err_str);
        return NULL;
    }

    return file;
}
