// ca_cert.example.h — template for ca_cert.h
#ifndef CA_CERT_EXAMPLE_H
#define CA_CERT_EXAMPLE_H

// Copy this file to src/ca_cert.h and replace the placeholder with your
// actual CA certificate in PEM format.

const char CA_CERTIFICATE[] = R"EOF(
-----BEGIN CERTIFICATE-----
REPLACE_WITH_YOUR_CA_CERTIFICATE
-----END CERTIFICATE-----
)EOF";

#endif // CA_CERT_EXAMPLE_H
