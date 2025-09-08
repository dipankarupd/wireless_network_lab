int auth(unsigned char *out, const unsigned char *in,
                unsigned long long inlen, const unsigned char *k);

int auth_verify(const unsigned char *h, const unsigned char *in,
                       unsigned long long inlen, const unsigned char *k);

int prf(unsigned char* out, unsigned long long outlen,
               const unsigned char* in, unsigned long long inlen,
               const unsigned char* k);
