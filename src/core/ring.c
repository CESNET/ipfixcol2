/**
 * \file src/core/ring.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Ring buffer for messages (source file)
 * \date 2018
 */

/* Copyright (C) 2018 CESNET, z.s.p.o.
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

#include <stdlib.h> // aligned_malloc
//#include <unistd.h>
#include <pthread.h>
#include <time.h>

#include "ring.h"
#include "verbose.h"


// START TODO: move into header files
#include <stdalign.h>
#include <assert.h>
#include <inttypes.h>

#ifndef IPX_CLINE_SIZE
/** Expected CPU cache-line size        */
#define IPX_CLINE_SIZE 64
#endif

/** User specific cache-line alignment  */
#define __ipx_aligned(x) __attribute__((__aligned__(x)))
/** Cache-line alignment                */
#define __ipx_cache_aligned __ipx_aligned(IPX_CLINE_SIZE)
// END

/** Internal identification of the ring buffer */
static const char *module = "Ring buffer";

/** \brief Data structure for a reader only */
struct ring_reader {
    /**
     * \brief Reader head in the buffer (start of the next read operation)
     * \note Value range [0..size - 1]. Must NOT point behind the end of the buffer.
     */
    uint32_t data_idx;
    /**
     * \brief Reader head (start of the next read operation)
     * \warning Not limited by the buffer's boundary. Overflow is expected behavior.
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t read_idx;
    /**
     * \brief Last known index of writer head (start of the data that still belongs to a writer)
     * \note In other words, a reader can read up to here (exclusive!).
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t exchange_idx;
    /**
     * \brief Reader index of the last sync with a writer (update of the sync structure)
     * \note Value range [0..UINT32_MAX]. Based on #read_idx.
     */
    uint32_t read_commit_idx;
    /** \brief Total size of the ring buffer (number of pointers)                    */
    uint32_t size;
    /**
     * \brief Size of a synchronization block
     * \note After writing at least this amount of data, update synchronization structure.
     */
    uint32_t div_block;

    /** Previously read messages - only 0 or 1 */
    uint32_t last;
};

/** \brief Data structure for writers only */
struct ring_writer {
    /**
     * \brief Writer head in the buffer (start of the next write operation)
     * \note Value range [0..size - 1]. Must NOT point behind the end of the buffer.
     */
    uint32_t data_idx;
    /**
     * \brief Writer head (start of the next write operation)
     * \warning This value can be read by a reader! Therefore, modification MUST be always atomic.
     * \warning Not limited by the buffer's boundary. Overflow is expected behavior.
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t write_idx;
    /**
     * \brief Last known index of reader head (start of the data that still belongs to a reader)
     * \note In other words, a writer can write up to here (exclusive!).
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t exchange_idx;
    /**
     * \brief Writer index of the last sync with a reader (update of the sync structure)
     * \note Value range [0..UINT32_MAX]. Based on #write_idx.
     */
    uint32_t write_commit_idx;
    /** \brief Total size of the ring buffer (number of pointers)                    */
    uint32_t size;
    /**
     * \brief Size of a synchronization block
     * \note After writing at least this amount of data, update synchronization structure.
     */
    uint32_t div_block;
};

/** \brief Exchange data structure for reader and writers */
struct ring_sync {
    /**
     * \brief Reader head
     * \note End of data read by a reader, i.e. a writer can write up to here (exclusive!).
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t           write_idx;
    /**
     * \brief Writer head
     * \note End of data written by a writer, i.e. a reader can read up to here (exclusive!).
     * \note Value range [0..UINT32_MAX].
     */
    uint32_t           read_idx;
    /** \brief Synchronization mutex (MUST be always used to access data structures here)    */
    pthread_mutex_t    mutex;

    /** \brief Reader condition variable (empty buffer) */
    pthread_cond_t     cond_reader;
    /** \brief Writer condition variable (full buffer)  */
    pthread_cond_t     cond_writer;
};

/** \brief Ring buffer */
struct ipx_ring {
    /** A Reader only structure (cache aligned)         */
    struct ring_reader reader      __ipx_cache_aligned;
    /** Writers only structure (cache aligned)          */
    struct ring_writer writer      __ipx_cache_aligned;
    /** Writer lock                                     */
    pthread_spinlock_t writer_lock __ipx_cache_aligned;
    /** Synchronization structure (cache-aligned)       */
    struct ring_sync   sync        __ipx_cache_aligned;
    /** Multiple writers mode                           */
    bool               mw_mode;
    /** Ring data (array of pointers)                   */
    ipx_msg_t        **data;
};

ipx_ring_t *
ipx_ring_init(uint32_t size, bool mw_mode)
{
    ipx_ring_t *ring;

    // Prepare data structures
    ring = aligned_alloc(alignof(struct ipx_ring), sizeof(struct ipx_ring));
    if (!ring) {
        IPX_ERROR(module, "aligned_alloc() failed! (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    ring->data = aligned_alloc(alignof(*ring->data), sizeof(*ring->data) * size);
    if (!ring->data) {
        IPX_ERROR(module, "aligned_alloc() failed! (%s:%d)", __FILE__, __LINE__);
        goto exit_A;
    }

    // Initialize writers' spin lock
    int rc;
    if ((rc = pthread_spin_init(&ring->writer_lock, PTHREAD_PROCESS_PRIVATE)) != 0) {
        IPX_ERROR(module, "pthread_spin_init() failed! (%s:%d, err: %d)", __FILE__, __LINE__, rc);
        goto exit_B;
    }

    // Initialize sync mutex and conditional variables
    if ((rc = pthread_mutex_init(&ring->sync.mutex, NULL)) != 0) {
        IPX_ERROR(module, "pthread_mutex_init() failed! (%s:%d, err: %d)", __FILE__, __LINE__, rc);
        goto exit_C;
    }

    pthread_condattr_t cond_attr;
    if ((rc = pthread_condattr_init(&cond_attr)) != 0) {
        IPX_ERROR(module, "pthread_condattr_init() failed! (%s:%d, err: %d)", __FILE__, __LINE__,
            rc);
        goto exit_D;
    }

    if ((rc = pthread_condattr_setclock(&cond_attr, CLOCK_MONOTONIC)) != 0) {
        IPX_ERROR(module, "pthread_condattr_setclock() failed! (%s:%d, err: %d)", __FILE__,
            __LINE__, rc);
        goto exit_E;
    }

    if ((rc = pthread_cond_init(&ring->sync.cond_reader, &cond_attr)) != 0) {
        IPX_ERROR(module, "pthread_cond_init() failed! (%s:%d, err: %d)", __FILE__, __LINE__, rc);
        goto exit_E;
    }

    if ((rc = pthread_cond_init(&ring->sync.cond_writer, &cond_attr)) != 0) {
        IPX_ERROR(module, "pthread_cond_init() failed! (%s:%d, err: %d)", __FILE__, __LINE__, rc);
        goto exit_F;
    }
    pthread_condattr_destroy(&cond_attr);

    // Initialize ring variables
    ring->reader.size = size;
    ring->reader.div_block = size / 8;
    ring->reader.data_idx = 0;
    ring->reader.read_idx = 0;
    ring->reader.exchange_idx = 0;
    ring->reader.read_commit_idx = 0;
    ring->reader.last = 0;

    ring->writer.size = size;
    ring->writer.div_block = size / 8;
    ring->writer.data_idx = 0;
    ring->writer.exchange_idx = size; // Amount of empty memory
    ring->writer.write_idx = 0;
    ring->writer.write_commit_idx = 0;

    ring->sync.read_idx = 0;
    ring->sync.write_idx = size;

    ring->mw_mode = mw_mode;
    return ring;

    // In case failure
exit_F:
    pthread_cond_destroy(&ring->sync.cond_reader);
exit_E:
    pthread_condattr_destroy(&cond_attr);
exit_D:
    pthread_mutex_destroy(&ring->sync.mutex);
exit_C:
    pthread_spin_destroy(&ring->writer_lock);
exit_B:
    free(ring->data);
exit_A:
    free(ring);
    return NULL;
}

void
ipx_ring_destroy(ipx_ring_t *ring)
{
    // The last read message is not confirmed by the reader, it is 1 index behind -> "+ 1"
    if (ring->reader.read_idx + 1 != ring->writer.write_idx) {
        uint32_t cnt = ring->writer.write_idx - ring->reader.read_idx + 1;
        IPX_WARNING(module, "Destroying of a ring buffer that still contains %" PRIu32
            " unprocessed message(s)!", cnt);
    }

    pthread_cond_destroy(&ring->sync.cond_writer);
    pthread_cond_destroy(&ring->sync.cond_reader);
    pthread_mutex_destroy(&ring->sync.mutex);
    pthread_spin_destroy(&ring->writer_lock);
    free(ring->data);
    free(ring);
}

/**
 * \brief Wrapper around condition wait
 * \param[in] cond  Condition variable
 * \param[in] mutex Locked mutex
 * \param[in] msec  Number of milliseconds to wait
 * \return Same as the function pthread_cond_timedwait
 */
static inline int
ring_cond_timedwait(pthread_cond_t *restrict cond, pthread_mutex_t *restrict mutex, long msec)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    ts.tv_nsec += msec * 1000000;
    if (ts.tv_nsec >= 1000000000) {
        ts.tv_nsec %= 1000000000;
        ts.tv_sec += 1;
    }

    return pthread_cond_timedwait(cond, mutex, &ts);
}

/**
 * \brief Get a new empty field
 *
 * \note The function blocks until a required memory is ready. Before the next call of this
 *   function, the function ipx_ring_commit() MUST be called first, to commit performed
 *   modifications.
 * \param[in] ring Ring buffer
 * \return Pointer to a unused place in the buffer
 */
static inline ipx_msg_t **
ipx_ring_begin(ipx_ring_t *ring)
{
    // Prepare the next pointer to write
    ipx_msg_t **msg = &ring->data[ring->writer.data_idx];

    // Is there enough space?
    if (ring->writer.exchange_idx - ring->writer.write_idx > 0) {
        return msg;
    }

    // Get an empty space -> reader-writer synchronization
    pthread_mutex_lock(&ring->sync.mutex);
    ring->writer.exchange_idx = ring->sync.write_idx;
    while (ring->writer.exchange_idx - ring->writer.write_idx == 0) {
        // After sync the buffer is still full, try again later
        pthread_cond_signal(&ring->sync.cond_reader);
        ring_cond_timedwait(&ring->sync.cond_writer, &ring->sync.mutex, 10);
        ring->writer.exchange_idx = ring->sync.write_idx;
    }
    pthread_cond_signal(&ring->sync.cond_reader);
    pthread_mutex_unlock(&ring->sync.mutex);

    assert(ring->writer.exchange_idx - ring->writer.write_idx > 0);
    return msg;
}

/**
 * \brief Commit modifications of memory
 * \param[in] ring Ring buffer
 */
static inline void
ipx_ring_commit(ipx_ring_t *ring)
{
    register uint32_t new_idx = 1;
    ring->writer.data_idx++;

    if (ring->writer.size == ring->writer.data_idx) {
        // End of the ring buffer has been reached -> skip to the beginning
        ring->writer.data_idx = 0;
    }

    // Atomic update of writer index (Note: new_idx will be the same as writer.write_idx)
    new_idx += __sync_fetch_and_add(&ring->writer.write_idx, new_idx);

    // Sync positions with a reader, if necessary
    if (new_idx - ring->writer.write_commit_idx >= ring->writer.div_block) {
        pthread_mutex_lock(&ring->sync.mutex);
        ring->sync.read_idx = new_idx;
        ring->writer.exchange_idx = ring->sync.write_idx;
        ring->writer.write_commit_idx = new_idx;
        pthread_cond_signal(&ring->sync.cond_reader);
        pthread_mutex_unlock(&ring->sync.mutex);
    }
}

void
ipx_ring_push(ipx_ring_t *ring, ipx_msg_t *msg)
{
    ipx_msg_t **msg_space;

    if (ring->mw_mode) {
        pthread_spin_lock(&ring->writer_lock);
    }

    msg_space = ipx_ring_begin(ring);
    *msg_space = msg;
    ipx_ring_commit(ring);

    if (ring->mw_mode) {
        pthread_spin_unlock(&ring->writer_lock);
    }
}


ipx_msg_t *
ipx_ring_pop(ipx_ring_t *ring)
{
    // Consider previous memory block as processed
    ring->reader.data_idx += ring->reader.last;
    ring->reader.read_idx += ring->reader.last;
    ring->reader.last = 0;

    if (ring->reader.size == ring->reader.data_idx) {
        // The end of the ring buffer has been reached -> skip to the beginning
        ring->reader.data_idx = 0;
    }

    // Prepare the next pointer to read
    ipx_msg_t **msg = &ring->data[ring->reader.data_idx];

    // Sync positions with writers, if necessary
    if (ring->reader.read_idx - ring->reader.read_commit_idx >= ring->reader.div_block) {
        pthread_mutex_lock(&ring->sync.mutex);
        ring->sync.write_idx += ring->reader.read_idx - ring->reader.read_commit_idx;
        ring->reader.exchange_idx = ring->sync.read_idx;
        ring->reader.read_commit_idx = ring->reader.read_idx;
        pthread_cond_signal(&ring->sync.cond_writer);
        pthread_mutex_unlock(&ring->sync.mutex);
    }

    if (ring->reader.exchange_idx - ring->reader.read_idx > 0) {
        // Ok, the reader owns this part of the buffer
        // TODO: prefetch
        ring->reader.last = 1;
        return *msg; // Now, we can dereference the pointer
    }

    while (1) {
        // The reader has reached the end of the filled memory -> try to sync
        pthread_mutex_lock(&ring->sync.mutex);
        pthread_cond_signal(&ring->sync.cond_writer);
        // Wait until a writer sends a signal or a timeout expires
        ring_cond_timedwait(&ring->sync.cond_reader, &ring->sync.mutex, 10);
        ring->reader.exchange_idx = ring->sync.read_idx;
        pthread_mutex_unlock(&ring->sync.mutex);

        if (ring->reader.exchange_idx - ring->reader.read_idx > 0) {
            // TODO: prefetch
            ring->reader.last = 1;
            return *msg; // Now, we can dereference the pointer
        }

        // Writer still didn't perform sync -> try to steal all committed messages from writer
        pthread_mutex_lock(&ring->sync.mutex);
        ring->sync.read_idx = ring->reader.exchange_idx = __sync_fetch_and_add(&ring->writer.write_idx, 0);
        pthread_mutex_unlock(&ring->sync.mutex);

        if (ring->reader.exchange_idx - ring->reader.read_idx > 0) {
            // TODO: prefetch
            ring->reader.last = 1;
            return *msg; // Now, we can dereference the pointer
        }
    }
}

void
ipx_ring_mw_mode(ipx_ring_t *ring, bool mode)
{
    ring->mw_mode = mode;
}