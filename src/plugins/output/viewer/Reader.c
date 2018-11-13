/**
 * \file src/plugins/output/viewer/Reader.c
 * \author Jan Kala <xkalaj01@stud.fit.vutbr.cz>
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
#include <libfds.h>
#include "Reader.h"
#include <ipfixcol2.h>

void read_packet(ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr) {

    const struct fds_ipfix_msg_hdr *ipfix_msg_hdr;
    ipfix_msg_hdr = (const struct fds_ipfix_msg_hdr*)ipx_msg_ipfix_get_packet(msg);

    if (ipfix_msg_hdr->length < FDS_IPFIX_MSG_HDR_LEN){
        return;
    }

    // Print packet header
    printf("## Message header\n");
    printf("\tversion:\t%"PRIu16"\n",ntohs(ipfix_msg_hdr->version));
    printf("\tlength:\t\t%"PRIu16"\n",ntohs(ipfix_msg_hdr->length));
    printf("\texport time:\t%"PRIu32"\n",ntohl(ipfix_msg_hdr->export_time));
    printf("\tsequence no.:\t%"PRIu32"\n",ntohl(ipfix_msg_hdr->seq_num));
    printf("\tODID:\t\t%"PRIu32"\n", ntohl(ipfix_msg_hdr->odid));

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
}

void read_set(struct ipx_ipfix_set *set, ipx_msg_ipfix_t *msg, const fds_iemgr_t *iemgr, uint32_t *rec_i) {
    uint8_t *set_end = (uint8_t *)set->ptr + ntohs(set->ptr->length);
    uint16_t set_id = ntohs(set->ptr->flowset_id);

    const uint32_t rec_cnt = ipx_msg_ipfix_get_drec_cnt(msg);

    printf("** Set header\n");
    printf("\tset ID: %"PRIu16"\n",set_id);
    printf("\tlength: %"PRIu16"\n",ntohs(set->ptr->length));

    // Template set
    if (set_id == FDS_IPFIX_SET_TMPLT || set_id ==FDS_IPFIX_SET_OPTS_TMPLT){

        struct fds_tset_iter tset_iter;
        fds_tset_iter_init(&tset_iter, set->ptr);

        // Iteration through all templates in the set
        while (fds_tset_iter_next(&tset_iter) == FDS_OK){
            // Read and print single template
            read_template_set(&tset_iter, set_id,iemgr);
            putchar('\n');
        }
        return;
    }
    // Data set
    if (set_id >= FDS_IPFIX_SET_MIN_DSET){
        struct ipx_ipfix_record *ipfix_rec = ipx_msg_ipfix_get_drec(msg,*rec_i);

        //All the records in the set has same template id, so we extract it from the first record and print it
        printf("\ttemplate id: %"PRIu16"\n", ipfix_rec->rec.tmplt->id);

        // Iteration through the records which belongs to the current set
        while ((ipfix_rec != NULL) && (ipfix_rec->rec.data < set_end) && (*rec_i < rec_cnt)){
            // Print record header
            printf("-- Record [n.%"PRIu32" of %"PRIu32"]\n", *rec_i+1, rec_cnt);

            // Get the specific record and read all the fields
            read_record(&ipfix_rec->rec, 1, iemgr); //add iemgr
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

void read_template_set(struct fds_tset_iter *tset_iter, uint16_t set_id, const fds_iemgr_t *iemgr) {
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
        fds_template_destroy(tmplt);
        return;
    }

    // Printing out the header
    printf("-- Template header\n");
    printf("\ttemplate id: %"PRIu16"\n", tmplt->id);
    printf("\tfield count: %"PRIu16"\n", tmplt->fields_cnt_total);

    // Using IEManager to fill the definitions of the fields in the template
    if(fds_template_ies_define(tmplt, iemgr , false) != FDS_OK){
        printf("*Error while assigning element definitions in template*\n");
        fds_template_destroy(tmplt);
        return;
    }

    printf("\tfields:\n");
    // Iteration through the fields and printing them out
    for (uint16_t i = 0; i < tmplt->fields_cnt_total ; ++i) {
        struct fds_tfield current = tmplt->fields[i];
        printf("\t");
        printf("en:%*"PRIu32" ",WRITER_EN_SPACE, current.en);
        printf("id:%*"PRIu16" ",WRITER_ID_SPACE, current.id);
        printf("size:");
        // In case of variable length print keyword "var"
        current.length == FDS_IPFIX_VAR_IE_LEN ? printf("%*s ", WRITER_SIZE_SPACE, "var") : printf("%*"PRIu16" ", WRITER_SIZE_SPACE,current.length);

        // Unknown field definition
        if (current.def == NULL){
            printf("<Unknown field name>\n");
        }
        // Known field definition
        else {
            printf("%*s ", WRITER_ORG_NAME_SPACE, current.def->scope->name);
            printf("%s", current.def->name);
        }
        putchar('\n');
    }
    fds_template_destroy(tmplt);
}

void print_indent(unsigned int n){
    for (unsigned int i = 0; i < n; i++){
        putchar('\t');
    }
}

void read_record(struct fds_drec *rec, unsigned int indent, const fds_iemgr_t *iemgr) {
    // Write info from header about the record template
    print_indent(indent);
    printf("field count: %"PRIu16"\n", rec->tmplt->fields_cnt_total);
    print_indent(indent);
    printf("data length: %"PRIu16"\n", rec->tmplt->data_length);
    print_indent(indent);
    printf("size       : %"PRIu16"\n", rec->size);

    // Iterate through all the fields in record
    struct fds_drec_iter iter;
    fds_drec_iter_init(&iter, rec, 0);

    print_indent(indent);
    printf("fields:\n");
    while (fds_drec_iter_next(&iter) != FDS_EOC) {
        struct fds_drec_field field = iter.field;
        read_field(&field,indent, iemgr, rec->snap, false); // add iemgr
    }
}

const char * fds_semantic2str(enum fds_ipfix_list_semantics semantic){
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

void read_field(struct fds_drec_field *field, unsigned int indent, const fds_iemgr_t *iemgr, const fds_tsnapshot_t *snap, bool in_basicList) {
    // Write info from header about field
    print_indent(indent);
    if (!in_basicList) {
        printf("en:%*" PRIu32 " id:%*" PRIu16" ", WRITER_EN_SPACE, field->info->en, WRITER_ID_SPACE, field->info->id);
    }

    enum fds_iemgr_element_type type;
    char *org;
    char *field_name;
    const char *unit;

    // Get the organisation name, field name and unit of the data.
    if (field->info->def == NULL){
        type = FDS_ET_OCTET_ARRAY;
        org = "<unknown>";
        field_name = "<unknown>";
        unit = "";
    }
    else {
        type = field->info->def->data_type;
        org = field->info->def->scope->name;
        field_name = field->info->def->name;
        if (field->info->def->data_unit != FDS_EU_NONE){
            unit = fds_iemgr_unit2str(field->info->def->data_unit);
        }
        else{
            unit = "";
        }
    }

    switch(type){
    case FDS_ET_BASIC_LIST: {
        // Iteration through the basic list
        bool did_read = false;
        struct fds_blist_iter blist_it;

        printf("%*s %s", WRITER_ORG_NAME_SPACE, org, field_name);
        fds_blist_iter_init(&blist_it,field, iemgr);
        printf("%4s[semantic: %s]"," ",fds_semantic2str(blist_it.semantic)); // Semantic is known after initialization

        while (fds_blist_iter_next(&blist_it) == FDS_OK){
            if (!did_read){
                putchar('\n');
                did_read = true;
            }
            read_field(&blist_it.field,indent+1, iemgr, snap, true);
        }
        if (!did_read){ // if we didn't read a single field
            printf("%4s : empty\n"," ");
        }
        return;
    }
    case FDS_ET_SUB_TEMPLATE_LIST:
    case FDS_ET_SUB_TEMPLATE_MULTILIST: {
        // Iteration through the subTemplate and subTemplateMulti lists
        printf("%*s %s\n", WRITER_ORG_NAME_SPACE, org, field_name);
        struct fds_stlist_iter stlist_iter;
        print_indent(indent);
        fds_stlist_iter_init(&stlist_iter, field, snap, 0);
        printf("- semantic: %d\n",stlist_iter.semantic);
        while (fds_stlist_iter_next(&stlist_iter) == FDS_OK){
            read_record(&stlist_iter.rec, indent+1, iemgr);
            putchar('\n');
        }
        putchar('\b');
        return;
    }
    default:
        printf("%*s %-*s : ", WRITER_ORG_NAME_SPACE,org, WRITER_FIELD_NAME_SPACE, field_name);
    }

    // Read and write the data from the field
    char buffer[1024];
    int res = fds_field2str_be(field->data, field->size, type, buffer, sizeof(buffer));

    if(res >= 0){
        // Conversion was successful
        if (type == FDS_ET_STRING){
            printf("\"%s\"", buffer);
        }
        else {
            printf("%s", buffer);
        }

        if (*unit != 0)
            printf(" %s", unit);
        putchar('\n');

        return;
    }
    else if (res == FDS_ERR_BUFFER){
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

