//
// Created by lukashutak on 28/03/18.
//

#ifndef IPFIXCOL_MSGGEN_H
#define IPFIXCOL_MSGGEN_H

class ipfix_set {
protected:
    // TODO: buffer

public:
    virtual void
    overwrite_id(uint16_t id) = 0;
    virtual void
    overwrite_len(uint16_t len) = 0;
    virtual void
    add_padding(uint16_t size) = 0;

    virtual uint16_t
    get_size();
};

class ipfix_msg {
private:
    // TODO:

public:
    /** Constructor */
    ipfix_msg();
    /** Destructor  */
    ~ipfix_msg();

    void
    set_version(uint16_t version);
    void
    set_len(uint16_t len);
    void
    set_odid(uint32_t odid);
    void
    set_seq(uint32_t seq_num);
    void
    set_exp(uint32_t exp_time);

    void
    add_set(ipfix_set &set); // just copy?
};

class ipfix_drec {
private:

public:
    static const uint16_t SIZE_STATIC = 0;
    static const uint16_t SIZE_VAR = 65535;

    ipfix_drec();
    ~ipfix_drec();

    void
    append_int(int64_t value, uint16_t size = 8);
    void
    append_uint(uint64_t value, uint16_t size = 8);
    void
    append_float(double value, uint16_t size = 8);
    void
    append_bool(uint8_t value, uint16_t size = SIZE_AUTO);
    void
    append_string(const std::string &value, uint16_t size = SIZE_AUTO);
    void
    append_datetime(struct timespec ts, enum fds_iemgr_element_type type, uint16_t size = SIZE_AUTO);
    void
    append_ip(const std::string &value, uint16_t size = SIZE_AUTO);
    void
    append_mac(const std::string &value, uint16_t size = SIZE_AUTO);
    void
    append_octets(const void *data, uint16_t data_len, uint16_t size = SIZE_AUTO);
};

class ipfix_dset : public ipfix_set {
private:

public:
    ipfix_dset(uint16_t set_id);
    ~ipfix_dset();

    void
    overwrite_id(uint16_t id);
    void
    overwrite_len(uint16_t len);
    void
    add_padding(uint16_t size);

    void
    add_rec(const ipfix_drec &rec);
};


class ipfix_trec {
private:
    enum fds_template_type _type;

public:
    static const uint16_t SIZE_VAR = 65535;

    ipfix_trec(uint16_t id);
    ipfix_trec(uint16_t id, uint16_t scope_id);
    ~ipfix_trec();

    void
    clear();

    void
    add_field(uint16_t id, uint16_t size, uint32_t en = 0);

    void
    overwrite_id(uint16_t id);
    void
    overwrite_scope(uint16_t id);
    void
    overwrite_len(uint16_t len);
};

class ipfix_tset : public ipfix_set {
private:
    // TODO
public:
    ipfix_tset(uint16_t id);
    ~ipfix_tset();

    void
    overwrite_id(uint16_t id);
    void
    overwrite_len(uint16_t len);
    void
    add_padding(uint16_t size);

    void
    add_rec(const ipfix_trec &rec);
};

#endif //IPFIXCOL_MSGGEN_H
