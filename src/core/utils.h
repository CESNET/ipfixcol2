//
// Created by lukashutak on 21/03/18.
//

#ifndef IPFIXCOL_UTILS_H
#define IPFIXCOL_UTILS_H


#include <string.h>

/** Size of an error buffer message                                               */
#define IPX_STRERROR_SIZE 128

/**
 * \def ipx_strerror
 * \brief Re-entrant strerror wrapper
 *
 * Main purpose is to solve issues with different strerror_r definitions.
 * \warning The function is defined as a macro that creates a local buffer for an error message.
 *   Therefore, it cannot be called multiple times at the same scope.
 * \code{.c}
 *   const char *err_str;
 *   ipx_strerror(errno, err_str);
 *   printf("ERROR: %s\n", err_str);
 * \endcode
 * \param[in]  errnum An error code
 * \param[out] buffer Pointer to the local buffer
 */
#define ipx_strerror(errnum, buffer)                                                     \
    char ipx_strerror_buffer[IPX_STRERROR_SIZE];                                         \
    (buffer) = _Generic((&strerror_r),                                                   \
        char *(*)(int, char *, size_t): /* GNU-specific   */                             \
            strerror_r((errnum), ipx_strerror_buffer, IPX_STRERROR_SIZE),                \
        int   (*)(int, char *, size_t): /* XSI-compliant  */                             \
            (strerror_r((errnum), ipx_strerror_buffer, IPX_STRERROR_SIZE) >= 0           \
                ? ipx_strerror_buffer                                                    \
                : "Unknown error (strerror_r failed!)")                                  \
    )

#endif //IPFIXCOL_UTILS_H
