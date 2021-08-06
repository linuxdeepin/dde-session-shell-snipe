#ifndef AESENCRYPTOECBMODE_H
#define AESENCRYPTOECBMODE_H
#include <string>
using namespace std;
void initKV();
string dmcg_ecb_encrypt(string plainText);
string dmcg_ecb_decrypt(string cipherHexText);
#endif // AESENCRYPTOECBMODE_H
