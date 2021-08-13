#include <libfds.h>
#include <cstdio>
#include <stdexcept>
#include <string>

#include "ipfix/util.hpp"
#include "utils/filelist.hpp"
#include "ipfix/fdsreader.hpp"

int main(int argc, char **argv)
{
    printf("Hello world\n");
    const char *filename = argv[1];
    int rc;

    // Set up iemgr
    /*
    fds_iemgr_t *iemgr;
    iemgr = fds_iemgr_create();
    if (!iemgr) {
        throw std::bad_alloc();
    }

    rc = fds_iemgr_read_dir(iemgr, fds_api_cfg_dir());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot read iemgr definitions");
    }
    */
    unique_fds_iemgr iemgr = make_iemgr();
    iemgr.get();

    FileList file_list;
    file_list.add_files(filename);

    FDSReader reader(iemgr.get());

/*

    // Set up file
    fds_file_read_ctx read_ctx;
    fds_file_t *file;

    file = fds_file_init();
    if (!file) {
        throw std::bad_alloc();
    }

    rc = fds_file_open(file, filename, FDS_FILE_READ);
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot open file");
    }

    rc = fds_file_set_iemgr(file, iemgr.get());
    if (rc != FDS_OK) {
        throw std::runtime_error("cannot set file iemgr");
    }
*/

    // Loop through records
    unsigned long records = 0;
    unsigned long biflow = 0;
    std::string next_filename;
    while (true) {
        fds_drec drec;
        if (next_filename == "" || !reader.read_record(drec)) {
            if (!file_list.pop(next_filename)) {
                break;
            }
            reader.set_file(next_filename);
            continue;
        }

        records++;

        if (drec.tmplt->flags & FDS_TEMPLATE_BIFLOW) {
            biflow++;
        }

    /*
        rc = fds_file_read_rec(file, &drec, &read_ctx);

        if (rc == FDS_OK) {
            records++;

            if (drec.tmplt->flags & FDS_TEMPLATE_BIFLOW) {
                biflow++;
            }

        } else if (rc == FDS_EOC) {
            break;

        } else {
            throw std::runtime_error("error reading record");
        }
    */
    }

    printf("Read %lu records, %lu were biflow\n", records, biflow);
    return 0;
}