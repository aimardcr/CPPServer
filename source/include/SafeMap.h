#ifndef SAFE_MAP_H
#define SAFE_MAP_H

#include <map>
#include <string>

template<typename T>
class SafeMap {
public:
    const T& get(const std::string& key, const T& defaultValue = T()) const {
        auto it = data.find(key);
        return it != data.end() ? it->second : defaultValue;
    }

    T& operator[](const std::string& key) {
        return data[key];
    }

    const T& operator[](const std::string& key) const {
        static const T defaultValue = T();
        auto it = data.find(key);
        return it != data.end() ? it->second : defaultValue;
    }

    bool has(const std::string& key) const {
        return data.find(key) != data.end();
    }

    void set(const std::string& key, const T& value) {
        data[key] = value;
    }

    void clear() {
        data.clear();
    }

    auto begin() { return data.begin(); }
    auto end() { return data.end(); }
    auto begin() const { return data.begin(); }
    auto end() const { return data.end(); }
    
    size_t size() const { return data.size(); }
    bool empty() const { return data.empty(); }

private:
    std::map<std::string, T> data;
};

#endif // SAFE_MAP_H