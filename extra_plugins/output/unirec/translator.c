/**
 * \file translator.c
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \author Imrich Stoffa <xstoff02@stud.fit.vutbr.cz>
 * \brief Conversion of IPFIX to UniRec format (source file)
 */

/* Copyright (C) 2015 - 2017 CESNET, z.s.p.o.
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
 * This software is provided ``as is, and any express or implied
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

#include <string.h>
#include <inttypes.h>
#include "unirecplugin.h"
#include "translator.h"
#include "fields.h"
#include <unirec/unirec.h>

// Prototypes
static int
translate_uint(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_ip(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_mac(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_tcpflags(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_time(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_float(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_bytes(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
static int
translate_bool(const struct fds_drec_field *field, struct conf_unirec *conf,
        const struct translator_table_rec *def);
//TODO fds_iemgr_elem_find_id()
//fds_iemgr_elem_find_id()
//Element if exists, otherwise NULL.


// Path to unirec elements config file
const char *UNIREC_ELEMENTS_FILE = "./unirec-elements.txt"; //TODO is this where it should be located

/**
 * \brief Creates IPFIX id from string
 *
 * @param ipfixToken String in eXXidYY format, where XX is enterprise number and YY is element ID
 * @return Returns id that is used to compare field against IPFIX template
 */
static ipfixElement_t ipfix_from_string(char *ipfixToken)
{
   ipfixElement_t element;
   char *endptr;

   element.en = strtol(ipfixToken + 1, &endptr, 10);
   element.id = strtol(endptr + 2, (char **) NULL, 10);

   return element;
}

/**
 * \brief Loads all available elements from configuration file UNIREC_ELEMENTS_FILE
 * @param[in] ctx Instance of IPFIXcol2 context (only for log!)
 * @param[out] count Number of parsed fields in the config file
 * @return List of UniRec elements on success, NULL otherwise
 */
unirecField_t *load_IPFIX2UR_mapping(ipx_ctx_t *ctx, uint32_t *urcount, uint32_t *ipfixcount)
{
    FILE *uef = NULL;
    char *line;
    size_t lineSize = 100;
    ssize_t res;
    char *token, *state; /* Variables for strtok  */
    unirecField_t *fields = NULL, *curfld = NULL;
    uint32_t numurfields = 0;
    uint32_t numipfixfields = 0;

    /* Open the file */
    uef = fopen(UNIREC_ELEMENTS_FILE, "r");
    if (uef == NULL) {
        IPX_CTX_ERROR(ctx, "Could not open file \"%s\" (%s:%d)", UNIREC_ELEMENTS_FILE, __FILE__, __LINE__);
        return NULL;
    }

    /* Init buffer */
    line = malloc(lineSize);
    if (line == NULL ){
        IPX_CTX_ERROR(ctx, "Memory allocation failed. (%s:%d)", __FILE__, __LINE__);
        fclose(uef);
        return NULL;
    }

    /* Process all lines */
    while (1) {
        /* Read one line */
        res = getline(&line, &lineSize, uef);
        if (res <= 0) {
           break;
        }

        /* Exclude comments */
        if (line[0] == '#') continue;

        /* Create new element structure, make space for ipfixElCount ipfix elements and NULL */
        curfld = malloc(sizeof(unirecField_t));
        if (curfld == NULL ){
            IPX_CTX_ERROR(ctx, "Memory allocation failed. (%s:%d)", __FILE__, __LINE__);
            fclose(uef);
            free(line);
            return NULL;
        }
        curfld->name = NULL;

        /* Read individual tokens */
        int position = 0;
        int ret;
        for (token = strtok_r(line, " \t", &state); token != NULL; token = strtok_r(NULL, " \t", &state), position++) {
            switch (position) {
            case 0:
                curfld->name = strdup(token);
                break;
            case 1:
                if ((ret = ur_get_field_type_from_str(token)) == UR_E_INVALID_TYPE) {
                    IPX_CTX_ERROR(ctx, "Unknown UniRec type \"%s\" of field \"%s\"", token, curfld->name);
                    fclose(uef);
                    free(line);
                    return NULL;
                } else {
                    curfld->unirec_type_str = strdup(token);
                    curfld->unirec_type = (ur_field_type_t) ret;
                }
                break;
            case 2: {
                /* Split the string */
                char *ipfixToken, *ipfixState; /* Variables for strtok  */
                int ipfixPosition = 0;
                curfld->ipfixCount = 0;
                for (ipfixToken = strtok_r(token, ",", &ipfixState);
                     ipfixToken != NULL;
                     ipfixToken = strtok_r(NULL, ",", &ipfixState), ipfixPosition++) {
                    /* Enlarge the element if necessary */
                    if (ipfixPosition > 0) {
                        curfld = realloc(curfld, sizeof(unirecField_t) + (ipfixPosition) * sizeof(uint64_t));
                    }
                    /* Store the ipfix element id */
                    curfld->ipfix[ipfixPosition] = ipfix_from_string(ipfixToken);
                    curfld->ipfixCount++;
                    numipfixfields++;
                    /* Fill in Unirec field type based on ipfix element id */
                }
                break;
                } // case 3 end
            } // switch end
        }

        /* Check that all necessary data was provided */
        if (position < 3) {
            if (curfld->name) {
                free(curfld->name);
            }
            free(curfld);
            continue;
        }

        numurfields++;
        curfld->next = fields;
        fields = curfld;
    }

    fclose(uef);
    free(line);

    if (ipfixcount != NULL) {
        *ipfixcount = numipfixfields;
    }
    if (urcount != NULL) {
        *urcount = numurfields;
    }
    return fields;
}

/**
 * \brief Set a value of an unsigned integer
 *
 * \param[out] field    Pointer to the data field
 * \param[in]  lnf_type Type of a LNF field
 * \param[in]  value    New value
 * \return On success returns #IPX_OK.
 * \return If the \p value cannot fit in the \p field of the defined \p size, stores a saturated
 *   value and returns the value #IPX_ERR_TRUNC.
 * \return If the \p lnf_type is not supported, a value of the \p field is unchanged and returns
 *   #IPX_ERR_ARG.
 */
static inline int
translate_set_uint_lnf(void *field, ur_field_type_t urtype, uint64_t value)
{
    switch (urtype) {
    case UR_TYPE_UINT64:
        *((uint64_t *) field) = value;
        return IPX_OK;

    case UR_TYPE_UINT32:
        if (value > UINT32_MAX) {
            *((uint32_t *) field) = UINT32_MAX; // byte conversion not required
            return IPX_ERR_TRUNC;
        }

        *((uint32_t *) field) = (uint32_t) value;
        return IPX_OK;

    case UR_TYPE_UINT16:
        if (value > UINT16_MAX) {
            *((uint16_t *) field) = UINT16_MAX; // byte conversion not required
            return IPX_ERR_TRUNC;
        }

        *((uint16_t *) field) = (uint16_t) value;
        return IPX_OK;

    case UR_TYPE_UINT8:
        if (value > UINT8_MAX) {
            *((uint8_t *) field) = UINT8_MAX;
            return IPX_ERR_TRUNC;
        }

        *((uint8_t *) field) = (uint8_t) value;
        return IPX_OK;

    default:
        return IPX_ERR_ARG;
    }
}

/**
 * \brief Convert an unsigned integer
 * \details \copydetails ::translator_func
 */
static int
translate_uint(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    uint64_t value = 0;
    if (fds_get_uint_be(field->data, field->size, &value) != FDS_OK) {
        // Failed
        return 1;
    }

    void *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);
    ur_field_type_t urtype = ur_get_type(def->ur_field_id);
    // Store the value to the buffer
    if (translate_set_uint_lnf(fieldp, urtype, value) == IPX_ERR_ARG) {
        // Failed
        return 1;
    }

    return 0;
}

/**
 * \brief Convert an IP address
 * \details \copydetails ::translator_func
 */
static int
translate_ip(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    void *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);
    switch (field->size) {
    case 4: // IPv4
        *((ip_addr_t *) fieldp) = ip_from_4_bytes_be((char *) field->data);
        break;
    case 16: // IPv6
        memcpy(fieldp, field->data, field->size);
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert TCP flags
 * \note TCP flags can be also stored in 16bit in IPFIX, but LNF file supports
 *   only 8 bits flags. Therefore, we need to truncate the field if necessary.
 * \details \copydetails ::translator_func
 */
static int
translate_tcpflags(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    (void) def;

    uint8_t *fieldp = (uint8_t *) ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);
    switch (field->size) {
    case 1:
        *fieldp = *field->data;
        break;
    case 2: {
            uint16_t new_value = ntohs(*(uint16_t *) field->data);
            *fieldp = (uint8_t) new_value; // Preserve only bottom 8 bites
        }
        break;
    default:
        // Invalid size of the field
        return 1;
    }

    return 0;
}

/**
 * \brief Convert bool field
 * \details \copydetails ::translator_func
 */
static int
translate_bool(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    void *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);

    bool value;
    if (fds_get_bool(field->data, field->size, &value) != FDS_OK) {
        return 1;
    }

    ur_field_type_t urtype = ur_get_type(def->ur_field_id);
    switch (urtype) {
    case UR_TYPE_INT8:
    case UR_TYPE_UINT8:
        *((uint8_t *) fieldp) = value ? 1 : 0;
        break;
    case UR_TYPE_INT16:
    case UR_TYPE_UINT16:
        *((uint16_t *) fieldp) = value ? 1 : 0;
        break;
    case UR_TYPE_INT32:
    case UR_TYPE_UINT32:
        *((uint32_t *) fieldp) = value ? 1 : 0;
        break;
    case UR_TYPE_INT64:
    case UR_TYPE_UINT64:
        *((uint64_t *) fieldp) = value ? 1 : 0;
        break;
    default:
        /* unsupported type */
        return 1;
    }

    return 0;
}

/**
 * \brief Convert float/double field
 * \details \copydetails ::translator_func
 */
static int
translate_float(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    void *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);

    double value;
    if (fds_get_float_be(field->data, field->size, &value) != FDS_OK) {
        return 1;
    }

    ur_field_type_t urtype = ur_get_type(def->ur_field_id);
    if (urtype == UR_TYPE_FLOAT) {
        *((float *) fieldp) = (float) value;
    } else if (urtype == UR_TYPE_DOUBLE) {
        *((double *) fieldp) = value;
    } else {
        /* unsupported type */
        return 1;
    }

    return 0;
}

/**
 * \brief Convert a MAC address
 * \details \copydetails ::translator_func
 */
static int
translate_bytes(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    ur_set_var(conf->translator->urtmpl, conf->ur_message, def->ur_field_id, field->data, field->size);

    return 0;
}

/**
 * \brief Convert a MAC address
 * \note We have to keep the address in network byte order. Therefore, we
 *   cannot use a converter for unsigned int
 * \details \copydetails ::translator_func
 */
static int
translate_mac(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    if (field->size != 6 || ur_get_type(def->ur_field_id) != UR_TYPE_MAC) {
        return 1;
    }

    void *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);
    memcpy(fieldp, field->data, 6U);
    return 0;
}

/**
 * \brief Convert a timestamp
 * \details \copydetails ::translator_func
 */
static int
translate_time(const struct fds_drec_field *field, struct conf_unirec *conf,
    const struct translator_table_rec *def)
{
    if (field->info->en != 0) {
        // Non-standard field are not supported right now
        return 1;
    }

    // Determine data type of a timestamp
    enum fds_iemgr_element_type type;
    switch (field->info->id) {
    case 150: // flowStartSeconds
    case 151: // flowEndSeconds
        type = FDS_ET_DATE_TIME_SECONDS;
        break;
    case 152: // flowStartMilliseconds
    case 153: // flowEndMilliseconds
        type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    case 154: // flowStartMicroseconds
    case 155: // flowEndMicroseconds
        type = FDS_ET_DATE_TIME_MICROSECONDS;
        break;
    case 156: // flowStartNanoseconds
    case 157: // flowEndNanoseconds
        type = FDS_ET_DATE_TIME_NANOSECONDS;
        break;
    case 258: // collectionTimeMilliseconds
        type = FDS_ET_DATE_TIME_MILLISECONDS;
        break;
    default:
        // Other fields are not supported
        return 1;
    }

    ur_time_t *fieldp = ur_get_ptr_by_id(conf->translator->urtmpl, conf->ur_message, def->ur_field_id);

    switch (type) {
    case FDS_ET_DATE_TIME_SECONDS:
    case FDS_ET_DATE_TIME_MILLISECONDS: {
           uint64_t value;
           if (fds_get_datetime_lp_be(field->data, field->size, type, &value) != FDS_OK) {
               // Failed
               return 1;
           }

           *fieldp = ur_time_from_sec_msec(value / 1000, value % 1000);
        }
        break;
    case FDS_ET_DATE_TIME_MICROSECONDS:
    case FDS_ET_DATE_TIME_NANOSECONDS: {
           struct timespec value;
           if (fds_get_datetime_hp_be(field->data, field->size, type, &value) != FDS_OK) {
               // Failed
               return 1;
           }

           /* TODO create a better conversion function in UniRec API */
           *fieldp = ur_time_from_sec_msec(value.tv_sec, value.tv_nsec / 1000);
        }
        break;
    default:
        /* Incompatible type of field */
        return 1;
    }

    return 0;
}

/**
 * \brief Compare conversion definitions
 * \note Only definitions of IPFIX Information Elements are used for comparison
 *   i.e. other values can be undefined.
 * \param[in] p1 Left definition
 * \param[in] p2 Right definition
 * \return The same as memcmp i.e. < 0, == 0, > 0
 */
static int
transtator_cmp(const void *p1, const void *p2)
{
    const struct translator_table_rec *elem1, *elem2;
    elem1 = (const translator_table_rec_t *) p1;
    elem2 = (const translator_table_rec_t *) p2;

    uint64_t elem1_val = ((uint64_t) elem1->ipfix.pen) << 16 | elem1->ipfix.ie;
    uint64_t elem2_val = ((uint64_t) elem2->ipfix.pen) << 16 | elem2->ipfix.ie;

    if (elem1_val == elem2_val) {
        return 0;
    } else {
        return (elem1_val < elem2_val) ? (-1) : 1;
    }
}

static translator_func
get_func_by_elementtypes(ur_field_type_t urt, const struct fds_iemgr_elem *ielem)
{
    enum fds_iemgr_element_type ipt = ielem->data_type;

    switch (urt) {
    case UR_TYPE_STRING:
    case UR_TYPE_BYTES:
        if (ipt == FDS_ET_STRING ||
                ipt == FDS_ET_OCTET_ARRAY) {
            return translate_bytes;
        }
        break;
    case UR_TYPE_CHAR:
    case UR_TYPE_UINT8:
    case UR_TYPE_INT8:
    case UR_TYPE_UINT16:
    case UR_TYPE_INT16:
    case UR_TYPE_UINT32:
    case UR_TYPE_INT32:
    case UR_TYPE_UINT64:
    case UR_TYPE_INT64:
        if (strcmp(ielem->name, "tcpControlBits") == 0) {
            return translate_tcpflags;
        } else if (ipt == FDS_ET_BOOLEAN) {
            return translate_bool;
        } else if (ipt == FDS_ET_UNSIGNED_8 ||
                ipt == FDS_ET_UNSIGNED_16 ||
                ipt == FDS_ET_UNSIGNED_32 ||
                ipt == FDS_ET_UNSIGNED_64 ||
                ipt == FDS_ET_SIGNED_8 ||
                ipt == FDS_ET_SIGNED_16 ||
                ipt == FDS_ET_SIGNED_32 ||
                ipt == FDS_ET_SIGNED_64) {
            return translate_uint;
        }
        break;
    case UR_TYPE_FLOAT:
    case UR_TYPE_DOUBLE:
            if (ipt == FDS_ET_FLOAT_32 ||
                    ipt == FDS_ET_FLOAT_64) {
                return translate_float;
            }
        break;
    case UR_TYPE_IP:
        if (ipt == FDS_ET_IPV4_ADDRESS || ipt == FDS_ET_IPV6_ADDRESS) {
            return translate_ip;
        }
        break;
    case UR_TYPE_MAC:
        if (ipt == FDS_ET_MAC_ADDRESS) {
            return translate_mac;
        }
        break;
    case UR_TYPE_TIME:
        if (ipt == FDS_ET_DATE_TIME_SECONDS ||
                ipt == FDS_ET_DATE_TIME_MILLISECONDS ||
                ipt == FDS_ET_DATE_TIME_MICROSECONDS ||
                ipt == FDS_ET_DATE_TIME_NANOSECONDS) {
            return translate_time;
        }
        break;
    }
    return NULL;
}

translator_t *
translator_init(ipx_ctx_t *ctx, unirecField_t *map, uint32_t ipfixfieldcount)
{
    ur_field_id_t urfield;
    int ret;
    const fds_iemgr_t *iemgr = ipx_ctx_iemgr_get(ctx);
    const struct fds_iemgr_elem *ielem = NULL;
    translator_table_rec_t *t;

    IPX_CTX_INFO(ctx, "Initialization of translator.");

    translator_t *instance = calloc(1, sizeof(*instance));
    if (!instance) {
        IPX_CTX_ERROR(ctx, "Unable to allocate memory (%s:%d)", __FILE__, __LINE__);
        return NULL;
    }

    // Copy conversion table and sort it (just for sure)
    instance->table_capacity = ipfixfieldcount;
    instance->table = calloc(ipfixfieldcount, sizeof(*instance->table));
    instance->table_count = ipfixfieldcount;

    unirecField_t *p;
    size_t table_idx = 0;
    for (p = map; p != NULL; p = p->next) {
        ret = ur_get_id_by_name(p->name);
        if (ret != UR_E_INVALID_NAME) {
            urfield = ur_get_id_by_name(p->name);
        } else {
            IPX_CTX_ERROR(ctx, "Unknown name of the UniRec field '%s', something is corrupted because it must have been defined already. (%s:%d)", p->name, __FILE__, __LINE__);
        }

        for (uint32_t i = 0; i < p->ipfixCount; ++i) {
            t = &instance->table[table_idx++];
            t->ipfix.pen = p->ipfix[i].en;
            t->ipfix.ie = p->ipfix[i].id;
            t->ipfix_priority = i + 1;
            t->ur_field_id = urfield;

            ielem = fds_iemgr_elem_find_id(iemgr, p->ipfix[i].en, p->ipfix[i].id);
            if (ielem == NULL) {
                IPX_CTX_ERROR(ctx, "Unknown IPFIX element in libfds (en%did%d)", p->ipfix[i].en, p->ipfix[i].id);
                free(instance->table);
                free(instance);
                return NULL;
            }
            IPX_CTX_INFO(ctx, "\t%d:%d %s", p->ipfix[i].en, p->ipfix[i].id, ielem->name);

            t->func = get_func_by_elementtypes(p->unirec_type, ielem);
            if (t->func == translate_tcpflags) {
                IPX_CTX_INFO(ctx, "!!!!!!! Using translate_tcpflags (%s, %d)", ur_field_type_str[p->unirec_type], ielem->data_type);
            }
            if (t->func == NULL) {
                IPX_CTX_ERROR(ctx, "Unknown translation function for types (%s, %d)", ur_field_type_str[p->unirec_type], ielem->data_type);
            }
        }
    }
    const size_t table_elem_size = sizeof(instance->table[0]);
    qsort(instance->table, instance->table_count, table_elem_size, transtator_cmp);

    instance->ctx = ctx;
    return instance;
}

int
translator_init_urtemplate(translator_t *tr, ur_template_t *urtmpl, char *urspec)
{

    tr->urtmpl = urtmpl;
    tr->req_fields = calloc(urtmpl->count, sizeof(uint8_t));
    tr->todo_fields = malloc(urtmpl->count * sizeof(uint8_t));
    tr->field_idx = calloc(ur_field_specs.ur_last_id, sizeof(ur_field_id_t));

    if (tr->req_fields == NULL || tr->todo_fields == NULL || tr->field_idx == NULL) {
        free(tr->req_fields);
        free(tr->todo_fields);
        free(tr->field_idx);
        return 1;
    }

    for (size_t i = 0; i < urtmpl->count; ++i) {
        tr->field_idx[urtmpl->ids[i]] = i;
    }

    char *token, *state;
    int ret;
    for (token = strtok_r(urspec, ",", &state); token != NULL; token = strtok_r(NULL, ",", &state)) {
        if (token[0] == '?') {
            /* optional */
        } else {
            ret = ur_get_id_by_name(token);
            if (ret != UR_FIELD_ID_MAX) {
                tr->req_fields[tr->field_idx[ret]] = 1;
            }
        }
    }
    /* disable translation of all fields that are not in the current UniRec template */
    for (size_t i = 0; i < tr->table_count; ++i) {
        if (ur_is_present(tr->urtmpl, tr->table[i].ur_field_id) == 0) {
            tr->table[i].func = NULL;
        }
    }
    return 0;
}

void
translator_destroy(translator_t *trans)
{
    free(trans->req_fields);
    free(trans->todo_fields);
    free(trans->field_idx);
    free(trans->table);
    free(trans);
}

int
translator_translate(translator_t *trans, struct conf_unirec *conf, struct fds_drec *ipfix_rec, uint16_t flags)
{
    size_t i;
    void *data = conf->ur_message;
    // Initialize a record iterator
    struct fds_drec_iter it;
    fds_drec_iter_init(&it, ipfix_rec, flags);

    // Try to convert all IPFIX fields
    translator_table_rec_t key;
    translator_table_rec_t *def;
    int converted_fields = 0;

    const size_t table_rec_cnt = trans->table_count;
    const size_t table_rec_size = sizeof(trans->table[0]);

    /* reinit UniRec */
    memcpy(trans->todo_fields, trans->req_fields, trans->urtmpl->count * sizeof(uint8_t));
    memset(data, 0, ur_rec_fixlen_size(trans->urtmpl));
    ur_clear_varlen(trans->urtmpl, data);

    while (fds_drec_iter_next(&it) != FDS_EOC) {
        // Find a conversion function
        const struct fds_tfield *info = it.field.info;

        key.ipfix.ie = info->id;
        key.ipfix.pen = info->en;

        def = bsearch(&key, trans->table, table_rec_cnt, table_rec_size, transtator_cmp);
        if (!def || !def->func) {
            // Conversion definition not found or translation disabled
            continue;
        }

        IPX_CTX_INFO(trans->ctx, "Processing field: %s (%d) %s", ur_get_name(def->ur_field_id), def->ur_field_id, ur_is_fixlen(def->ur_field_id) ? "fixlen" : "varlen");
        if (def->func(&it.field, conf, def) == 0) {
            trans->todo_fields[trans->field_idx[def->ur_field_id]] = 0;
        } else {
            // Conversion function failed
            IPX_CTX_WARNING(trans->ctx, "Failed to converter a IPFIX IE field  (ID: %" PRIu16 ", "
                    "PEN: %" PRIu32 ") to UniRec field.", info->id, info->en);
        }

        converted_fields++;
    }

    for (i = 0; i < trans->urtmpl->count && trans->todo_fields[i] == 0; ++i) {
        /* just check the whole array */
    }

    if (i < trans->urtmpl->count && trans->todo_fields[i] != 0) {
        IPX_CTX_WARNING(trans->ctx, "There is some required field that was not filled (%s), processed %d fields.", ur_get_name(trans->urtmpl->ids[i]), converted_fields);
        return 0;
    }

    IPX_CTX_INFO(trans->ctx, "Processed %d fields", converted_fields);
    // Cleanup
    return converted_fields;
}

void
free_IPFIX2UR_map(unirecField_t *map)
{
    unirecField_t *tmp, *head = map;
    if (map == NULL) {
        return;
    }

    while (head != NULL) {
        tmp = head->next;
        free(head->name);
        free(head->unirec_type_str);
        free(head);
        head = tmp;
    }
}

