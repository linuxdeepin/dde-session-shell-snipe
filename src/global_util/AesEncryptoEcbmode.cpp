#include "AesEncryptoEcbmode.h"
#include <stdio.h>

#include <iostream>
#include <fstream>
#include <sstream>

#include <aes.h>
#include <filters.h>
#include <modes.h>
#include <hex.h>
#include <osrng.h>
using CryptoPP::HexEncoder;
using CryptoPP::HexDecoder;


using namespace CryptoPP;
static byte key[CryptoPP::AES::DEFAULT_KEYLENGTH];
static byte secondkey[CryptoPP::AES::DEFAULT_KEYLENGTH];

void initKV()
{
    memset(key, 0x00, CryptoPP::AES::DEFAULT_KEYLENGTH);
    memset(secondkey, 0x00, CryptoPP::AES::DEFAULT_KEYLENGTH);


    // 随机生成密钥
    AutoSeededRandomPool rnd;
    for (int j = 0; j < CryptoPP::AES::DEFAULT_KEYLENGTH; ++j) {
        rnd.GenerateBlock(key, CryptoPP::AES::DEFAULT_KEYLENGTH);
    }
    // 固定二次密钥
    char tmpK[] = "txicbc-uniontech";
    for (int j = 0; j < CryptoPP::AES::DEFAULT_KEYLENGTH; ++j) {
        secondkey[j] = tmpK[j];
    }
}


string dmcg_ecb_encrypt(string plainText)
{
    // 初始化密钥
    initKV();
    string cipherText;
    cipherText.clear();

    CryptoPP::ECB_Mode<AES>::Encryption e1;
    e1.SetKey(secondkey, sizeof(secondkey));
    StringSource(plainText, true,
                 new StreamTransformationFilter(e1,
                                                new StringSink(cipherText)
                                               ) // StreamTransformationFilter
                );

    string cipherTextHex;
    cipherTextHex.clear();
    StringSource(cipherText, true,
                 new HexEncoder(
                     new StringSink(cipherTextHex)
                 ) // HexEncoder
                ); // StringSource


    return cipherTextHex;
}

string dmcg_ecb_decrypt(string cipherHexText)
{
        //解码Hex
    string cipherText;
    StringSource(cipherHexText, true,
                 new HexDecoder(
                     new StringSink(cipherText)
                 ) // HexEncoder
                ); // StringSource
    
    string plainText; 
    CryptoPP::ECB_Mode<AES>::Decryption d;
    d.SetKey(secondkey, sizeof(secondkey));
    StringSource(cipherText, true,
                 new StreamTransformationFilter(d,
                                                new StringSink(plainText)
                                               ) // StreamTransformationFilter
                );
    return plainText;
}

