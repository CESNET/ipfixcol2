/**
 * \author Michal Režňák
 * \date   8.9.17
 */
#ifndef IPFIX_COMMON_H
#define IPFIX_COMMON_H

/** Status code for success                                                   */
#define IPX_OK         (0)
/** Status code for invalid format description                                */
#define IPX_ERR       (-1)
/** Status code for memory allocation error                                   */
#define IPX_NOMEM     (-2)
/** Status code when something was not found                                  */
#define IPX_NOT_FOUND (-3)
/** Status code when returned element is alright but shouldn't be used        */
#define IPX_OK_OLD    (-4)

#endif //IPFIX_COMMON_H
