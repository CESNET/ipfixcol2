FROM debian:11 AS build
LABEL maintainer="Dmitry Novikov <nerosketch@gmail.com>"

RUN apt-get update && apt-get -y install git gcc g++ cmake make python3-docutils zlib1g-dev librdkafka-dev libxml2-dev liblz4-dev libzstd-dev libnfo-dev

WORKDIR /usr/src

RUN git clone --depth=1 https://github.com/CESNET/libfds.git && \
    cd libfds && \
    mkdir build && cd build && cmake .. -DCMAKE_INSTALL_PREFIX=/usr && \
    make && make install

# libfds.so
# Install the project...
# -- Install configuration: "Release"
# -- Installing: /usr/include/libfds.h
# -- Installing: /usr/include/libfds/api.h
# -- Installing: /usr/include/libfds/converters.h
# -- Installing: /usr/include/libfds/iemgr.h
# -- Installing: /usr/include/libfds/drec.h
# -- Installing: /usr/include/libfds/file.h
# -- Installing: /usr/include/libfds/filter.h
# -- Installing: /usr/include/libfds/ipfix_parsers.h
# -- Installing: /usr/include/libfds/ipfix_structs.h
# -- Installing: /usr/include/libfds/ipfix_filter.h
# -- Installing: /usr/include/libfds/template.h
# -- Installing: /usr/include/libfds/template_mgr.h
# -- Installing: /usr/include/libfds/trie.h
# -- Installing: /usr/include/libfds/xml_parser.h
# -- Installing: /usr/lib/x86_64-linux-gnu/libfds.so.0.3.0
# -- Installing: /usr/lib/x86_64-linux-gnu/libfds.so.0
# -- Installing: /usr/lib/x86_64-linux-gnu/libfds.so
# -- Installing: /etc/libfds/system
# -- Installing: /etc/libfds/system/aliases.xml
# -- Installing: /etc/libfds/system/mappings.xml
# -- Installing: /etc/libfds/system/elements
# -- Installing: /etc/libfds/system/elements/cesnet.xml
# -- Installing: /etc/libfds/system/elements/netflowExtra.xml
# -- Installing: /etc/libfds/system/elements/netscaler.xml
# -- Installing: /etc/libfds/system/elements/netflow.xml
# -- Installing: /etc/libfds/system/elements/cisco.xml
# -- Installing: /etc/libfds/system/elements/flowmon.xml
# -- Installing: /etc/libfds/system/elements/muni.xml
# -- Installing: /etc/libfds/system/elements/cert.xml
# -- Installing: /etc/libfds/system/elements/vmware.xml
# -- Installing: /etc/libfds/system/elements/iana.xml
# -- Installing: /etc/libfds/user
# -- Installing: /etc/libfds/user/elements

ADD . /usr/src/ipfixcol2
RUN cd /usr/src/ipfixcol2 && mkdir build && cd build && cmake .. && \
    make && make install
# Install the project...
# -- Install configuration: "Release"
# -- Installing: /usr/local/include/ipfixcol2.h
# -- Installing: /usr/local/include/ipfixcol2/message.h
# -- Installing: /usr/local/include/ipfixcol2/message_garbage.h
# -- Installing: /usr/local/include/ipfixcol2/message_ipfix.h
# -- Installing: /usr/local/include/ipfixcol2/message_session.h
# -- Installing: /usr/local/include/ipfixcol2/plugins.h
# -- Installing: /usr/local/include/ipfixcol2/session.h
# -- Installing: /usr/local/include/ipfixcol2/utils.h
# -- Installing: /usr/local/include/ipfixcol2/verbose.h
# -- Installing: /usr/local/include/ipfixcol2/api.h
# -- Installing: /usr/local/bin/ipfixcol2
# -- Installing: /usr/local/lib/ipfixcol2/libdummy-input.so
# -- Installing: /usr/local/lib/ipfixcol2/libtcp-input.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-tcp-input.7
# -- Installing: /usr/local/lib/ipfixcol2/libudp-input.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-udp-input.7
# -- Installing: /usr/local/lib/ipfixcol2/libipfix-input.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-ipfix-input.7
# -- Installing: /usr/local/lib/ipfixcol2/libfds-input.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-fds-input.7
# -- Installing: /usr/local/lib/ipfixcol2/libanonymization-intermediate.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-anonymization-inter.7
# -- Installing: /usr/local/lib/ipfixcol2/libdummy-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-dummy-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libfds-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-fds-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libjson-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-json-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libjson-kafka-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-json-kafka-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libtimecheck-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-timecheck-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libviewer-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-viewer-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libipfix-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-ipfix-output.7
# -- Installing: /usr/local/lib/ipfixcol2/libforwarder-output.so
# -- Installing: /usr/local/share/man/man7/ipfixcol2-forwarder-output.7
# -- Installing: /usr/local/bin/ipfixsend2

# ------------------ Production ------------------
FROM debian:11-slim AS prod
LABEL maintainer="Dmitry Novikov <nerosketch@gmail.com>"

RUN apt-get update && apt-get install -y zlib1g librdkafka1 libxml2 liblz4-1 libzstd1 libnfo1 && rm -rf /var/lib/apt/lists/* && mkdir /var/spool/ipfixcol

COPY --from=build /usr/lib/x86_64-linux-gnu/libfds.so* /usr/lib/x86_64-linux-gnu/
COPY --from=build /etc/libfds /etc/libfds
COPY --from=build /usr/local/bin/ipfix* /usr/local/bin/
COPY --from=build /usr/local/lib/ipfixcol2 /usr/local/lib/ipfixcol2

VOLUME /var/spool/ipfixcol
EXPOSE 4739/udp

COPY ./ipfixcol2.conf.xml.example /etc/ipfixcol2.conf.xml.example

CMD ["/usr/local/bin/ipfixcol2", "-c", "/etc/ipfixcol2.conf.xml.example"]

