/**
 * \file src/plugins/output/viewer/Reader.c
 * \author Jan Kala <xkalaj01@stud.fit.vutbr.cz>
 * \author Lukas Hutak <lukas.hutak@cesnet.cz>
 * \brief Viewer - output module for printing information about incoming packets on stdout (source file)
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

#include <inttypes.h>
#include <stdio.h>
#include <libfds.h>
#include "Reader.h"
#include <ipfixcol2.h>

void
read_packet(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr)
{
    const struct fds_ipfix_msg_hdr *ipfix_msg_hdr;
    ipfix_msg_hdr = (const struct fds_ipfix_msg_hdr*)ipx_msg_ipfix_get_packet(msg);

    if (ipfix_msg_hdr->length < FDS_IPFIX_MSG_HDR_LEN){
        return;
    }

    // Print packet header
    printf("--------------------------------------------------------------------------------\n");
    printf("IPFIX Message header:\n");
    printf("\tVersion:      %"PRIu16"\n",ntohs(ipfix_msg_hdr->version));
    printf("\tLength:       %"PRIu16"\n",ntohs(ipfix_msg_hdr->length));
    printf("\tExport time:  %"PRIu32"\n",ntohl(ipfix_msg_hdr->export_time));
    printf("\tSequence no.: %"PRIu32"\n",ntohl(ipfix_msg_hdr->seq_num));
    printf("\tODID:         %"PRIu32"\n", ntohl(ipfix_msg_hdr->odid));

    // Get number of sets
    struct ipx_ipfix_set *sets;
    size_t set_cnt;
    ipx_msg_ipfix_get_sets(msg, &sets, &set_cnt);

    // Record counter of total records in IPFIX message
    uint32_t rec_i = 0;

    // Iteration through all the sets
    for (uint32_t i = 0; i < set_cnt; ++i){
        read_set(&sets[i], msg, iemgr, &rec_i);
    }

    fflush(stdout);
}

void
read_set(struct ipx_ipfix_set *set, ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr, uint32_t *rec_i)
{
    uint8_t *set_end = (uint8_t *)set->ptr + ntohs(set->ptr->length);
    uint16_t set_id = ntohs(set->ptr->flowset_id);

    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);
    const char *set_type = "<unknown>";
    if (set_id == FDS_IPFIX_SET_TMPLT) {
        set_type = "Template Set";
    } else if (set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
        set_type = "Options Template Set";
    } else if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
        set_type = "Data Set";
    }

    printf("\n");
    printf("Set Header:\n");
    printf("\tSet ID: %"PRIu16" (%s)\n", set_id, set_type);
    printf("\tLength: %"PRIu16"\n", ntohs(set->ptr->length));

    if (set_id == FDS_IPFIX_SET_TMPLT || set_id == FDS_IPFIX_SET_OPTS_TMPLT) {
        // Template set
        struct fds_tset_iter tset_iter;
        fds_tset_iter_init(&tset_iter, set->ptr);
        const char *rec_fmt = (set_id == FDS_IPFIX_SET_TMPLT)
            ? "- Template Record (#%u)\n"
            : "- Options Template Record (#%u)\n";
        unsigned int rec_cnt = 0;

        // Iteration through all templates in the set
        while (fds_tset_iter_next(&tset_iter) == FDS_OK){
            // Read and print single template
            printf(rec_fmt, ++rec_cnt);
            read_template_set(&tset_iter, set_id,iemgr);
            putchar('\n');
        }
        return;
    }

    if (set_id >= FDS_IPFIX_SET_MIN_DSET) {
        // Data set
        struct ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg, *rec_i);
        if (ipfix_rec == NULL) return;

        // All the records in the set has same template id, so we extract it from the first record and print it
        printf("\tTemplate ID: %"PRIu16"\n", ipfix_rec->rec.tmplt->id);
        unsigned int iter_cnt = 0;

        // Iteration through the records which belongs to the current set
        while ((ipfix_rec != NULL) && (ipfix_rec->rec.data < set_end) && (*rec_i < rec_cnt)) {
            // Print record header
            printf("- Data Record (#%u) [Length: %"PRIu16"]:\n", ++iter_cnt, ipfix_rec->rec.size);
            // Get the specific record and read all the fields
            read_record(&ipfix_rec->rec, 1, iemgr);
            putchar('\n');

            // Get the next record
            (*rec_i)++;
            ipfix_rec = ipx_msg_ipfix_get_drec(msg, *rec_i);
        }
        return;
    }

    //Unknown set ID
    printf("\t<Unknown set ID>\n");
}

void
read_template_set(struct fds_tset_iter *tset_iter, uint16_t set_id, const fds_iemgr_t *iemgr)
{
    enum fds_template_type type;
    void *ptr;
    switch (set_id){
        case FDS_IPFIX_SET_TMPLT:
            type = FDS_TYPE_TEMPLATE;
            ptr = tset_iter->ptr.trec;
            break;
        case FDS_IPFIX_SET_OPTS_TMPLT:
            type = FDS_TYPE_TEMPLATE_OPTS;
            ptr = tset_iter->ptr.opts_trec;
            break;
        default:
            printf("\t<Undefined template>\n");
            return;
    }
    // Filling the template structure with data from raw packet
    uint16_t tmplt_size = tset_iter->size;
    struct fds_template *tmplt;
    if (fds_template_parse(type, ptr, &tmplt_size, &tmplt) != FDS_OK){
        printf("*Template parsing error*\n");
        return;
    }

    // Printing out the header
    printf("\tTemplate ID: %"PRIu16"\n", tmplt->id);
    printf("\tField Count: %"PRIu16"\n", tmplt->fields_cnt_total);
    if (type == FDS_TYPE_TEMPLATE_OPTS) {
        printf("\tScope Field Count: %"PRIu16"\n", tmplt->fields_cnt_scope);
    }

    // Using IEManager to fill the definitions of the fields in the template
    if(fds_template_ies_define(tmplt, iemgr , false) != FDS_OK){
        printf("*Error while assigning element definitions in template*\n");
        fds_template_destroy(tmplt);
        return;
    }

    // Iteration through the fields and printing them out
    for (uint16_t i = 0; i < tmplt->fields_cnt_total ; ++i) {
        struct fds_tfield current = tmplt->fields[i];
        printf("\t");
        printf("EN: %-*"PRIu32" ", WRITER_EN_SPACE, current.en);
        printf("ID: %-*"PRIu16" ", WRITER_ID_SPACE, current.id);
        printf("Size: ");
        // In case of variable length print keyword "var"
        current.length == FDS_IPFIX_VAR_IE_LEN
            ? printf("%-*s ", WRITER_SIZE_SPACE, "var.")
            : printf("%-*"PRIu16" ", WRITER_SIZE_SPACE, current.length);

        const char *pen_name = "<unknown>";
        const char *field_name = "<unknown>";
        if (current.def != NULL) {
            // Known definition
            pen_name = current.def->scope->name;
            field_name = current.def->name;
        } else {
            // Field is unknown ... try to find at least vendor
            const struct fds_iemgr_scope *scope_ptr = fds_iemgr_scope_find_pen(iemgr, current.en);
            if (scope_ptr != NULL) {
                pen_name = scope_ptr->name;
            }
        }

        printf("| %*s:%s", WRITER_ORG_NAME_SPACE, pen_name, field_name);
        if ((current.flags & FDS_TFIELD_SCOPE) != 0) {
            printf(" (scope)");
        }
        putchar('\n');
    }
    fds_template_destroy(tmplt);
}

void
print_indent(unsigned int n)
{
    for (unsigned int i = 0; i < n; i++){
        putchar('\t');
    }
}

void
read_record(struct fds_drec *rec, unsigned int indent, const fds_iemgr_t *iemgr)
{
    // Iterate through all the fields in record
    struct fds_drec_iter iter;
    fds_drec_iter_init(&iter, rec, FDS_DREC_PADDING_SHOW);

    while (fds_drec_iter_next(&iter) != FDS_EOC) {
        struct fds_drec_field field = iter.field;
        read_field(&field, indent, iemgr, rec->snap);
    }
}

const char *
fds_semantic2str(enum fds_ipfix_list_semantics semantic)
{
    switch (semantic){
        case (FDS_IPFIX_LIST_ALL_OF):
            return "All of";
        case (FDS_IPFIX_LIST_EXACTLY_ONE_OF):
            return "Exactly one of";
        case (FDS_IPFIX_LIST_ORDERED):
            return "Ordered";
        case (FDS_IPFIX_LIST_NONE_OF):
            return "None of";
        case (FDS_IPFIX_LIST_ONE_OR_MORE_OF):
            return "One or more of";
        default:
            return "Undefined";
    }
}

void
read_field(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
        const fds_tsnapshot_t *snap)
{
    // Write info from header about field
    print_indent(indent);
    printf("EN: %-*"PRIu32" ID: %-*"PRIu16" ", WRITER_EN_SPACE, field->info->en,
        WRITER_ID_SPACE, field->info->id);

    enum fds_iemgr_element_type type;
    const char *org;
    const char *field_name;
    const char *unit;

    // Get the organisation name, field name and unit of the data.
    if (field->info->def == NULL) {
        type = FDS_ET_OCTET_ARRAY;
        field_name = "<unknown>";
        org = "<unknown>";
        unit = "";

        // Try to find the scope
        const struct fds_iemgr_scope *scope_ptr = fds_iemgr_scope_find_pen(iemgr, field->info->en);
        if (scope_ptr != NULL) {
            org = scope_ptr->name;
        }
    } else {
        type = field->info->def->data_type;
        org = field->info->def->scope->name;
        field_name = field->info->def->name;
        if (field->info->def->data_unit != FDS_EU_NONE) {
            unit = fds_iemgr_unit2str(field->info->def->data_unit);
        } else {
            unit = "";
        }
    }

    if (fds_iemgr_is_type_list(type)) {
        // Process lists
        printf("%*s:%s", WRITER_ORG_NAME_SPACE, org, field_name);
        switch (type) {
        case FDS_ET_BASIC_LIST:
            // Note: header description will be complete in the function
            read_list_basic(field, indent, iemgr, snap);
            break;
        case FDS_ET_SUB_TEMPLATE_LIST:
            printf(" (subTemplateList, see below)\n");
            read_list_stl(field, indent, iemgr, snap);
            break;
        case FDS_ET_SUB_TEMPLATE_MULTILIST:
            printf(" (subTemplateMultiList, see below)\n");
            read_list_stml(field, indent, iemgr, snap);
            break;
        default:
            printf("*Unsupported list type*\n");
            break;
        }

        return;
    }

    printf("%*s:%-*s : ", WRITER_ORG_NAME_SPACE, org, WRITER_FIELD_NAME_SPACE, field_name);
    // Read and write the data from the field
    char buffer[1024];
    int res = fds_field2str_be(field->data, field->size, type, buffer, sizeof(buffer));

    if(res >= 0){
        // Conversion was successful
        if (type == FDS_ET_STRING) {
            printf("\"%s\"", buffer);
        } else if (type == FDS_ET_OCTET_ARRAY) {
            printf("0x%s",buffer);
        } else {
            printf("%s", buffer);
        }

        if (*unit != 0) {
            printf(" %s", unit);
        }
        putchar('\n');

        return;
    }
    else if (res == FDS_ERR_BUFFER) {
        // Buffer too small
        printf("<Data is too long to show>\n");
        return;
    }
    else {
        // Any other error
        printf("*Invalid value*\n");
        return;
    }
}

void
read_list_basic(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap)
{
    printf(" (basicList");

    struct fds_blist_iter it;
    fds_blist_iter_init(&it, field, iemgr);

    // Print information about the list -> make sure that the field definition is filled
    int rc = fds_blist_iter_next(&it);
    if (rc != FDS_EOC && rc != FDS_OK) {
        // Malformed
        printf(")\n");
        print_indent(indent);
        printf("  *Malformed data structure: %s*\n", fds_blist_iter_err(&it));
        return;
    }

    uint32_t ie_en = it.field.info->en;
    uint16_t ie_id = it.field.info->id;
    const char *name_scope = "<unknown>";
    const char *name_field = "<unknown>";
    if (it.field.info->def != NULL) {
        // Definition exists
        name_scope = it.field.info->def->scope->name;
        name_field = it.field.info->def->name;
    } else {
        // Definition not found... try to find the scope
        const struct fds_iemgr_scope *scope_ptr = fds_iemgr_scope_find_pen(iemgr, ie_en);
        if (scope_ptr != NULL) {
            name_scope = scope_ptr->name;
        }
    }

    printf(", List Semantic: %s)\n", fds_semantic2str(it.semantic));
    //print_indent(indent);
    //printf("> List fields: EN:%*"PRIu32" ID:%*"PRIu16" %*s:%-*s\n",
    //    WRITER_EN_SPACE, ie_en, WRITER_ID_SPACE, ie_id,
    //    WRITER_ORG_NAME_SPACE, name_scope, WRITER_FIELD_NAME_SPACE, name_field);

    // Re-initialize the iterator
    fds_blist_iter_init(&it, field, iemgr);
    unsigned int cnt_value = 0;
    bool more_values = true;

    while (more_values) {
        int rc = fds_blist_iter_next(&it);
        switch (rc) {
        case FDS_OK:  // Process the record
            break;
        case FDS_EOC: // End of the list
            more_values = false;
            continue;
        case FDS_ERR_FORMAT: // Something is wrong with the record
            printf("*Unable to continue due to malformed data: %s*\n", fds_blist_iter_err(&it));
            return;
        default:
            printf("*Internal error: fds_blist_iter_next(): unexpected return code*\n");
            return;
        }

        read_field(&it.field, indent + 1, iemgr, snap);
        cnt_value++;
    }

    if (cnt_value == 0) {
        print_indent(indent + 1);
        printf("EN: %-*"PRIu32" ID: %-*"PRIu16" ", WRITER_EN_SPACE, ie_en, WRITER_ID_SPACE, ie_id);
        printf("%*s:%-*s : ", WRITER_ORG_NAME_SPACE, name_scope, WRITER_FIELD_NAME_SPACE, name_field);
        printf("<empty>\n");
    }
}

void
read_list_stl(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap)
{
    struct fds_stlist_iter it;
    fds_stlist_iter_init(&it, field, snap, FDS_STL_REPORT);
    print_indent(indent);
    printf("> List semantic: %s, Template ID: %"PRIu16")\n", fds_semantic2str(it.semantic), it.tid);

    unsigned int cnt_rec = 0;
    bool more_records = true;

    while (more_records) {
        int rc = fds_stlist_iter_next(&it);
        switch (rc) {
        case FDS_OK:  // Process the record
            break;
        case FDS_EOC: // No more records
            more_records = false;
            continue;
        case FDS_ERR_NOTFOUND: // Template is not available
            print_indent(indent);
            printf("  *Template not available - unable to decode*\n");
            return;
        case FDS_ERR_FORMAT:   // Something is wrong with the record
            print_indent(indent);
            printf("*Unable to continue due to malformed data: %s*\n", fds_stlist_iter_err(&it));
            return;
        default:
            print_indent(indent);
            printf("*Internal error: fds_stlist_iter_next(): unexpected return code*\n");
            return;
        }

        print_indent(indent);
        printf("  - Data Record (#%u) [Length: %"PRIu16"]\n", ++cnt_rec, it.rec.size);
        read_record(&it.rec, indent + 1, iemgr);
    }

    if (cnt_rec == 0) {
        print_indent(indent + 1);
        printf(" <empty>\n");
    }
}

void
read_list_stml(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr,
    const fds_tsnapshot_t *snap)
{
    struct fds_stmlist_iter it;
    fds_stmlist_iter_init(&it, field, snap, FDS_STL_REPORT);
    print_indent(indent);
    printf("> List semantic: %s\n", fds_semantic2str(it.semantic));

    unsigned int cnt_block = 0;
    bool more_blocks = true;

    // For each block in the list
    while (more_blocks) {
        int rc_block = fds_stmlist_iter_next_block(&it);
        switch (rc_block) {
        case FDS_OK:  // Process the block
            break;
        case FDS_EOC: // No more blocks
            more_blocks = false;
            continue;
        case FDS_ERR_NOTFOUND: // Unable to read this block -> skip it
            break;
        case FDS_ERR_FORMAT:   // Something is wrong with the block
            print_indent(indent);
            printf("*Unable to continue due to malformed data: %s*\n", fds_stmlist_iter_err(&it));
            return;
        default:
            print_indent(indent);
            printf("*Internal error: fds_stmlist_iter_next_block(): unexpected return code*\n");
            return;
        }

        print_indent(indent);
        printf("- Top-level list header (#%u) [Template ID: %"PRIu16"]\n", ++cnt_block, it.tid);
        if (rc_block == FDS_ERR_NOTFOUND) {
            print_indent(indent);
            printf("  *Template not available - unable to decode*\n");
            continue;
        }

        unsigned int cnt_rec = 0;
        bool more_recs = true;

        while (more_recs) {
            int rc_rec = fds_stmlist_iter_next_rec(&it);
            switch (rc_rec) {
            case FDS_OK:  // Process the record
                break;
            case FDS_EOC: // No more records in the current block
                more_recs = false;
                continue;
            case FDS_ERR_FORMAT: // Something is wrong with the record
                print_indent(indent);
                printf("*Unable to continue due to malformed data: %s*\n", fds_stmlist_iter_err(&it));
                return;
            default:
                print_indent(indent);
                printf("*Internal error: fds_stmlist_iter_next_rec(): unexpected return code*\n");
                return;
            }

            print_indent(indent);
            printf("  - Data Record (#%u) [Length: %"PRIu16"]\n", ++cnt_rec, it.rec.size);
            read_record(&it.rec, indent + 1, iemgr);
        }

        if (cnt_rec == 0) {
            print_indent(indent + 1);
            printf(" <empty>\n");
        }
    }

    if (cnt_block == 0) {
        print_indent(indent);
        printf(" <empty>\n");
    }
}