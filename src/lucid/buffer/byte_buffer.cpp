// byte_buffer.cpp - ByteBuffer implementation
// Note: This file intentionally has minimal dependencies for testability

#include "byte_buffer.hpp"

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <algorithm>
#include <stdexcept>

namespace lucid {

// ========================================================================
// Constants
// ========================================================================

constexpr size_t ByteBuffer::DEFAULT_CAPACITY;
constexpr double ByteBuffer::GROWTH_FACTOR;

// ========================================================================
// Constructor / Destructor
// ========================================================================

ByteBuffer::ByteBuffer() : m_data(nullptr), m_size(0), m_capacity(0) {
}

ByteBuffer::ByteBuffer(size_t initial_capacity) : m_data(nullptr), m_size(0), m_capacity(0) {
    reserve(initial_capacity);
}

ByteBuffer::ByteBuffer(const void* data, size_t size) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (data && size > 0) {
        m_data = std::malloc(size);
        if (m_data) {
            std::memcpy(m_data, data, size);
            m_size = size;
            m_capacity = size;
        }
    }
}

ByteBuffer::ByteBuffer(const std::string& str) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (!str.empty()) {
        m_data = std::malloc(str.size());
        if (m_data) {
            std::memcpy(m_data, str.data(), str.size());
            m_size = str.size();
            m_capacity = str.size();
        }
    }
}

ByteBuffer::ByteBuffer(const char* str) : m_data(nullptr), m_size(0), m_capacity(0) {
    if (str) {
        size_t len = std::strlen(str);
        if (len > 0) {
            m_data = std::malloc(len);
            if (m_data) {
                std::memcpy(m_data, str, len);
                m_size = len;
                m_capacity = len;
            }
        }
    }
}

ByteBuffer::ByteBuffer(ByteBuffer&& other) noexcept 
    : m_data(other.m_data), m_size(other.m_size), m_capacity(other.m_capacity) {
    other.m_data = nullptr;
    other.m_size = 0;
    other.m_capacity = 0;
}

ByteBuffer::~ByteBuffer() {
    free_data();
}

ByteBuffer& ByteBuffer::operator=(ByteBuffer&& other) noexcept {
    if (this != &other) {
        free_data();
        m_data = other.m_data;
        m_size = other.m_size;
        m_capacity = other.m_capacity;
        other.m_data = nullptr;
        other.m_size = 0;
        other.m_capacity = 0;
    }
    return *this;
}

// ========================================================================
// Capacity Management
// ========================================================================

void ByteBuffer::reserve(size_t capacity) {
    if (capacity > m_capacity) {
        // Save old pointer before realloc - std::realloc preserves data if it fails
        void* old_data = m_data;
        void* new_data = std::realloc(old_data, capacity);
        if (new_data) {
            m_data = new_data;
            m_capacity = capacity;
        }
        // If realloc fails, the original memory is preserved and remains valid
        // We simply don't increase capacity - buffer remains usable at current size
    }
}

void ByteBuffer::resize(size_t size) {
    if (size > m_capacity) {
        reserve(size);
    }
    if (size > m_size) {
        // Zero-initialize new space
        std::memset(static_cast<uint8_t*>(m_data) + m_size, 0, size - m_size);
    }
    m_size = size;
}

void ByteBuffer::clear() {
    m_size = 0;
}

void ByteBuffer::swap(ByteBuffer& other) noexcept {
    std::swap(m_data, other.m_data);
    std::swap(m_size, other.m_size);
    std::swap(m_capacity, other.m_capacity);
}

// ========================================================================
// Data Operations
// ========================================================================

void ByteBuffer::append(const void* data, size_t size) {
    if (!data || size == 0) return;
    
    ensure_capacity(m_size + size);
    std::memcpy(static_cast<uint8_t*>(m_data) + m_size, data, size);
    m_size += size;
}

void ByteBuffer::append(const std::string& str) {
    append(str.data(), str.size());
}

void ByteBuffer::append(std::string_view view) {
    append(view.data(), view.size());
}

void ByteBuffer::append(uint8_t byte) {
    ensure_capacity(m_size + 1);
    static_cast<uint8_t*>(m_data)[m_size] = byte;
    m_size++;
}

void ByteBuffer::append(char c) {
    append(static_cast<uint8_t>(c));
}

void ByteBuffer::append(const ByteBuffer& other) {
    if (!other.empty()) {
        append(other.data(), other.size());
    }
}

// ========================================================================
// File Operations
// ========================================================================

bool ByteBuffer::write_to_file(const std::string& path) const {
    if (empty()) {
        return false;
    }
    
    std::ofstream file(path, std::ios::binary);
    if (!file) {
        return false;
    }
    
    file.write(static_cast<const char*>(m_data), static_cast<std::streamsize>(m_size));
    if (!file) {
        return false;
    }
    
    return true;
}

bool ByteBuffer::read_from_file(const std::string& path) {
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return false;
    }
    
    // Get file size
    auto file_size = static_cast<size_t>(file.tellg());
    file.seekg(0, std::ios::beg);
    
    // Reserve space and read
    clear();
    reserve(file_size);
    
    file.read(static_cast<char*>(m_data), static_cast<std::streamsize>(file_size));
    if (!file) {
        clear();
        return false;
    }
    
    m_size = file_size;
    return true;
}

// ========================================================================
// Query Operations
// ========================================================================

size_t ByteBuffer::find(const void* pattern, size_t pattern_size, size_t start) const {
    if (!pattern || pattern_size == 0 || start >= m_size) {
        return std::string::npos;
    }
    
    if (pattern_size > m_size) {
        return std::string::npos;
    }
    
    const uint8_t* data = static_cast<const uint8_t*>(m_data);
    const uint8_t* pat = static_cast<const uint8_t*>(pattern);
    
    for (size_t i = start; i <= m_size - pattern_size; ++i) {
        if (std::memcmp(data + i, pat, pattern_size) == 0) {
            return i;
        }
    }
    return std::string::npos;
}

size_t ByteBuffer::find(const std::string& str, size_t start) const {
    return find(str.data(), str.size(), start);
}

bool ByteBuffer::starts_with(const void* data, size_t size) const {
    if (!data || size == 0 || size > m_size) {
        return false;
    }
    return std::memcmp(m_data, data, size) == 0;
}

bool ByteBuffer::starts_with(const std::string& str) const {
    return starts_with(str.data(), str.size());
}

bool ByteBuffer::ends_with(const void* data, size_t size) const {
    if (!data || size == 0 || size > m_size) {
        return false;
    }
    return std::memcmp(static_cast<const uint8_t*>(m_data) + m_size - size, data, size) == 0;
}

bool ByteBuffer::ends_with(const std::string& str) const {
    return ends_with(str.data(), str.size());
}

// ========================================================================
// Byte Access
// ========================================================================

uint8_t& ByteBuffer::operator[](size_t index) {
    return static_cast<uint8_t*>(m_data)[index];
}

uint8_t ByteBuffer::operator[](size_t index) const {
    return static_cast<const uint8_t*>(m_data)[index];
}

// ========================================================================
// Private Methods
// ========================================================================

void ByteBuffer::ensure_capacity(size_t needed) {
    if (needed <= m_capacity) {
        return;
    }
    
    size_t new_capacity = static_cast<size_t>(m_capacity * GROWTH_FACTOR);
    if (new_capacity < needed) {
        new_capacity = needed;
    }
    
    // Save old pointer before realloc - std::realloc preserves data if it fails
    void* old_data = m_data;
    void* new_data = std::realloc(old_data, new_capacity);
    if (new_data) {
        m_data = new_data;
        m_capacity = new_capacity;
    }
    // If realloc fails, original memory is preserved and buffer remains usable
}

void ByteBuffer::free_data() {
    if (m_data) {
        std::free(m_data);
        m_data = nullptr;
        m_size = 0;
        m_capacity = 0;
    }
}

// ========================================================================
// libcurl Callbacks
// ========================================================================

size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    
    if (!userp || !contents || realsize == 0) {
        return 0;
    }
    
    auto* buffer = static_cast<ByteBuffer*>(userp);
    
    try {
        buffer->append(contents, realsize);
        return realsize;
    } catch (const std::exception&) {
        return 0;
    }
}

size_t curl_header_callback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t realsize = size * nmemb;
    
    if (!userp || !contents || realsize == 0) {
        return 0;
    }
    
    auto* buffer = static_cast<ByteBuffer*>(userp);
    
    try {
        buffer->append(contents, realsize);
        return realsize;
    } catch (const std::exception&) {
        return 0;
    }
}

} // namespace lucid