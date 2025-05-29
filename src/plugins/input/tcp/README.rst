TCP (input plugin)
==================

The plugin receives IPFIX messages over TCP transport protocol from one or more exporters
and pass them into the collector. Multiple instances of the plugin can run concurrently.
However, they must listen on different ports or local IP addresses.

Unlike UDP, TCP is reliable transport protocol that allows exporters to detect connection or
disconnection of the collector. Therefore, the issues with templates retransmission and
initial period of inability to interpret flow records does not apply here.

Received IPFIX messages may be compressed using LZ4 stream compression. The compression is
enabled on the exporter and the plugin detects it automatically.

Received IPFIX messages may be secured with TLS. The TLS decoder is enabled by specifying the
certificateFile parameter. Inbound TLS messages are automatically detected.

Example configuration
---------------------

.. code-block:: xml

    <input>
        <name>TCP input</name>
        <plugin>tcp</plugin>
        <params>
            <localPort>4739</localPort>
            <localIPAddress></localIPAddress>
            <!-- Uncomment to enable tls
            <tls>
                <certificateFile>path/to/certificate.crt</certificateFile>
                <privateKeyFile>path/to/private.key</privateKeyFile>
                <caFile>path/to/ca.crt</caFile>
                <verifyPeer>true</verifyPeer>
            </tls>
            -->
        </params>
    </input>

Parameters
----------

Mandatory parameters:

:``localPort``:
    Local port on which the plugin listens. [default: 4739]
:``localIPAddress``:
    Local IPv4/IPv6 address on which the TCP input plugin listens. If the element
    is left empty, the plugin binds to all available network interfaces. The element can occur
    multiple times (one IP address per occurrence) to manually select multiple interfaces.
    [default: empty]

Optional parameters:

:``tls``:
    Contains TLS configuration. When this is present, all insecure connections are refused by
    default. This can be changed with the parameter ``allowInsecure`` inside of the ``tls`` block.

Tls configuration
-----------------

All paths may be absolute or relative to the cwd of ipfixcol2.

Mandatory parameters:

:``certificateFile``:
    Path to a PEM file with certificate.

:``privateKeyFile``:
    Path to a PEM file with private key. If this is not present, ``certificateFile`` is expected to
    also contain the private key. [default: same as ``certificateFile``]

:``allowInsecure``:
    Boolean value. If it is set to true, non-tls connections will be accepted. Otherwise only tls
    connections will be accepted. [default: false]

:``verifyPeer``:
    Boolean value. If true, ipfixcol as TLS server will verify its clients certificates. Trusted
    certificates are set with the following parameters. [default: false]

The following parameters will set the trusted certificates. If the parameter is not present, the
given source type of trusted certificates is not used. If the value is empty, the default is used.
If none of the parameters are present, the defaults will be used for all. Specifying trusted
certificates (certificate authority) is relevant only if the option ``verifyPeer`` is set to true.

:``caFile``:
    Path to file with certificate authority. Default uses value from environment variable
    ``SSL_CERT_FILE`` or uses the system defaults.

:``caDir``:
    Path to directory with trusted certificates (certificate authorities). Default uses value from
    environment variable ``SSL_CERT_DIR`` or the system defaults.

:``caStore``:
    Path to certificate store with trusted certificates (certificate authorities). Default uses
    system defaults. This is available only when using OpenSSL 3 and newer. When used with older
    OpenSSL, the plugin will return error.

Notes
-----
The LZ4 compression uses special format that compatible with
`ipfixproble <https://github.com/CESNET/ipfixprobe>`.

How to create certificates
--------------------------

Here I describe how to create custom certificates and certificate authorities using
`easyrsa <https://github.com/OpenVPN/easy-rsa>`. This can be done on single machine or two machines
where one is the CA and other has certificate signed by CA.

Go to the path where you want to create the certificate infrastructure.

Download easyrsa and make it executable. This can be done with on both of the machines:

.. code-block:: sh

    wget https://raw.githubusercontent.com/OpenVPN/easy-rsa/refs/heads/master/easyrsa3/easyrsa
    chmod +x easyrsa

Initialize easyrsa. This will create folder ``pki`` in the current directory with the
infrastructure. This is done on both the system that requests the certificate, and system that signs
the certificate. In case of one system, do this only once.

.. code-block:: sh

    ./easyrsa init-pki

Create new certificate authority. This will ask you for name of the authority and new password. The
certificate authority will be created at ``pki/ca.crt``. This is done on the system that creates the
certificate.

.. code-block:: sh

    ./easyrsa build-ca

Create certificate request with a name (``Name``). This is done on the system that requests the
certificate. Often the certificate is used on server which may be addressed in multiple ways. The
certificate needs to be signed for all these names. This is done with the parameter ``--san``
(subject alternative names). The parameter is list of names separated with comma. Each name must
have type. The type may be for example ``IP`` for ip address or ``DNS`` for dns name. Other types
exist. In this example I will set the san to localhost ip address. Creating the certificate request
will ask you for a new password and name of the cretificate. You can leave the name as default and
just press enter. The request file will be created at ``pki/reqs/Name.req`` and the private key will
be created at ``pki/private/Name.key``.

.. code-block:: sh

    ./easyrsa --san='IP:127.0.0.1' gen-req Name

If the ca system and system requesting certificate are different, copy the request file to the CA
system and import it with this step. If both systems are the same, you can skip this step.

.. code-block:: sh

    ./easyrsa import-req path/to/request/Name.req Name

Now you can sign the request on the server with the CA. The request is signed for specific use
either as server or client. In this example I will create certificate for server. The option
``--copy-ext`` will make sure to copy the SANs. This will ask you for confirmation and than password
for the CA. If you want to create certificate for client, use ``client`` instead of ``server``. The
certificate will be created at ``pki/issued/Name.crt``.

.. code-block:: sh

    ./easyrsa --copy-ext sign-req server Name

So right now the important files are on the machines:

:CA machine:
    - ``pki/ca.crt`` - certificate of certificate authority.
    - ``pki/issued/Name.crt`` - certificate for the other machine.

:other machine:
    - ``pki/private/Name.key`` - private key for this machine certificate.

The certificate for the other machine has to be transfered to the other machine. It is no secret so
it doesn't require secure connection. Before you do that, it is good to add the CA certificate to
the certificate of the other machine so that it may send it alongside with its certificate. To do
that, you can just append the contents of ``pki/ca.crt`` to ``pki/issued/Name.crt``. Make sure that
the CA certificate is the second certificate in that file. Now you are free to transfer
``pki/issued/Name.crt`` to the other machine and use it there alongside with its key.

**Inline files:**

The last step also created inline files which may be directly used. Inline file contains the
information in more human readable way alongside with the data itself. The inline files differ
depending on whether you used single machine or multiple machines.

If you used single machine, there will be file ``pki/inline/private/Name.inline``. This file
contains both the machine certificate and CA certificate in correct order and also the private key.
It may be used in place of the certificate file and also in place of the private key file. You don't
have to copy the CA certificate into this file, because it is already here.

If you used multiple machines, there will be file ``pki/inline/Name.inline`` that contains the
machine certificate and CA certificate. The private key is not in this file, because the CA machine
doesn't have it. You can transfer this onto the other machine instead of the crt file and use it in
its place. You don't have to copy the CA certificate into this file, because it is already there.
