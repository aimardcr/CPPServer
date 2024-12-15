#ifndef MIME_TYPES_H
#define MIME_TYPES_H

#include <string>
#include <array>

struct MimeType {
    std::string mimeType;
    std::string extension;
    std::array<unsigned char, 8> magic;
    size_t magicSize;
};

const MimeType MIME_TYPES[] = {
    // PDF
    {
        "application/pdf",
        ".pdf",
        {0x25, 0x50, 0x44, 0x46, 0x2D, 0x00, 0x00, 0x00},
        5
    },
    
    // PNG
    {
        "image/png",
        ".png",
        {0x89, 0x50, 0x4E, 0x47, 0x0D, 0x0A, 0x1A, 0x0A},
        8
    },
    
    // JPEG
    {
        "image/jpeg",
        ".jpg",
        {0xFF, 0xD8, 0xFF, 0x00, 0x00, 0x00, 0x00, 0x00},
        3
    },

    // GIF87a
    {
        "image/gif",
        ".gif",
        {0x47, 0x49, 0x46, 0x38, 0x37, 0x61, 0x00, 0x00},
        6
    },
    
    // GIF89a
    {
        "image/gif",
        ".gif",
        {0x47, 0x49, 0x46, 0x38, 0x39, 0x61, 0x00, 0x00},
        6
    },
    
    // ZIP
    {
        "application/zip",
        ".zip",
        {0x50, 0x4B, 0x03, 0x04, 0x00, 0x00, 0x00, 0x00},
        4
    },
    
    // RAR
    {
        "application/x-rar-compressed",
        ".rar",
        {0x52, 0x61, 0x72, 0x21, 0x1A, 0x07, 0x00, 0x00},
        7
    },
    
    // WEBP
    {
        "image/webp",
        ".webp",
        {0x52, 0x49, 0x46, 0x46, 0x00, 0x00, 0x00, 0x00},
        4
    },
    
    // 7Z
    {
        "application/x-7z-compressed",
        ".7z",
        {0x37, 0x7A, 0xBC, 0xAF, 0x27, 0x1C, 0x00, 0x00},
        6
    },
    
    // MP3
    {
        "audio/mpeg",
        ".mp3",
        {0x49, 0x44, 0x33, 0x00, 0x00, 0x00, 0x00, 0x00},
        3
    },
    
    // MP4
    {
        "video/mp4",
        ".mp4",
        {0x66, 0x74, 0x79, 0x70, 0x69, 0x73, 0x6F, 0x6D},
        8
    },
    
    // DOCX
    {
        "application/vnd.openxmlformats-officedocument.wordprocessingml.document",
        ".docx",
        {0x50, 0x4B, 0x03, 0x04, 0x14, 0x00, 0x06, 0x00},
        8
    }
};

bool checkMimeType(const unsigned char* data, size_t length, const MimeType& signature);

#endif // MIME_TYPES_H