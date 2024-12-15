#include "MimeType.h"

bool checkMimeType(const unsigned char* data, size_t length, const MimeType& signature) {
    if (length < signature.magicSize) return false;
    
    for (size_t i = 0; i < signature.magicSize; i++) {
        if (signature.magic[i] != data[i]) return false;
    }
    
    return true;
}