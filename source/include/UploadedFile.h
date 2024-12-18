#ifndef UPLOADED_FILE_H
#define UPLOADED_FILE_H

#include <string>
#include <vector>
#include <filesystem>
#include <fstream>

class UploadedFile {
public:
    UploadedFile() = default;
    UploadedFile(const std::string& name, 
                 const std::string& filename,
                 const std::string& content_type,
                 std::vector<char>&& data)
        : name_(name)
        , filename_(filename)
        , content_type_(content_type)
        , data_(std::move(data)) {}

    const std::string& getName() const { return name_; }
    
    const std::string& getFilename() const { return filename_; }
    
    const std::string& getContentType() const { return content_type_; }
    
    size_t getSize() const { return data_.size(); }
    
    const std::vector<char>& getData() const { return data_; }
    
    bool save(const std::string& path) const {
        try {
            std::filesystem::path dir = std::filesystem::path(path).parent_path();
            if (!dir.empty()) {
                std::filesystem::create_directories(dir);
            }
            
            std::ofstream file(path, std::ios::binary);
            if (!file) return false;
            
            file.write(data_.data(), data_.size());
            return true;
        } catch (const std::exception&) {
            return false;
        }
    }

private:
    std::string name_;        
    std::string filename_;    
    std::string content_type_;
    std::vector<char> data_;  
};

#endif // UPLOADED_FILE_H