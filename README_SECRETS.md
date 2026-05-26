Secrets handling

- Do NOT commit `src/secrets.h` or `src/ca_cert.h`.
- Copy `src/secrets.example.h` → `src/secrets.h` and fill in your device MACs and any other private values.
- Copy `src/ca_cert.example.h` → `src/ca_cert.h` and paste your CA certificate PEM.
- `.gitignore` already contains entries for `src/secrets.h` and `src/ca_cert.h`.
