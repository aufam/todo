#include <iomanip>
#include <openssl/evp.h>

std::string password_hash(const std::string &password) {
    EVP_MD_CTX *mdctx = EVP_MD_CTX_new();  // Create a context
    if (mdctx == nullptr) {
        throw std::runtime_error("Failed to create EVP_MD_CTX");
    }

    // Initialize the context for SHA-256
    if (1 != EVP_DigestInit_ex(mdctx, EVP_sha256(), nullptr)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to initialize digest");
    }

    // Update the context with the password data
    if (1 != EVP_DigestUpdate(mdctx, password.c_str(), password.length())) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to update digest");
    }

    // Finalize and retrieve the hash
    unsigned char hash[EVP_MAX_MD_SIZE];  // Buffer for the hash
    unsigned int lengthOfHash = 0;  // Will hold the length of the hash

    if (1 != EVP_DigestFinal_ex(mdctx, hash, &lengthOfHash)) {
        EVP_MD_CTX_free(mdctx);
        throw std::runtime_error("Failed to finalize digest");
    }

    // Clean up
    EVP_MD_CTX_free(mdctx);

    // Convert the hash to a readable hexadecimal string
    std::stringstream ss;
    for (unsigned int i = 0; i < lengthOfHash; ++i) {
        ss << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(hash[i]);
    }

    return ss.str();
}

