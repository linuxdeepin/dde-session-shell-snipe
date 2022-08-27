#ifdef __cplusplus
extern "C" {
#endif
#include <openssl/sm2.h>
#include <openssl/sm4.h>
#ifdef __cplusplus
}
#endif


int main(int argc, char *argv[])
{
  const unsigned char ct[] = {};
  size_t ct_size;
  size_t *pt_size;
  sm2_plaintext_size(ct, ct_size, pt_size);
  return 0;
}
