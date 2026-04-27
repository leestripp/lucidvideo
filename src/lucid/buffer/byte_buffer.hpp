// byte_buffer.hpp - Binary/Char/Byte buffer for libcurl and general use
#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <string_view>

namespace lucid {

/**
 * ByteBuffer - A robust buffer class for handling binary/char/byte data
 * 
 * Design goals:
 * - Work seamlessly with libcurl CURLOPT_WRITEFUNCTION callbacks
 * - Support binary data (models, files) and text data
 * - Provide clean, safe API with no buffer overflows
 * - Enable efficient zero-copy operations where possible
 * 
 * Usage with libcurl:
 * ```cpp
 * ByteBuffer buffer;
 * curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
 * curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
 * ```
 */
class ByteBuffer {
public:
    // ========================================================================
    // Constructors
    // ========================================================================
    
    /// Create an empty buffer
    ByteBuffer();
    
    /// Create buffer with pre-allocated capacity
    explicit ByteBuffer(size_t initial_capacity);
    
    /// Create buffer from binary data
    ByteBuffer(const void* data, size_t size);
    
    /// Create buffer from string (copies data)
    explicit ByteBuffer(const std::string& str);
    
    /// Create buffer from const char* (null-terminated c-string)
    explicit ByteBuffer(const char* str);
    
    /// Move constructor
    ByteBuffer(ByteBuffer&& other) noexcept;
    
    /// Destructor
    ~ByteBuffer();
    
    // ========================================================================
    // Assignment operators (move only, no copy)
    // ========================================================================
    
    ByteBuffer& operator=(ByteBuffer&& other) noexcept;
    
    // Delete copy operations
    ByteBuffer(const ByteBuffer&) = delete;
    ByteBuffer& operator=(const ByteBuffer&) = delete;
    
    // ========================================================================
    // Accessors
    // ========================================================================
    
    /// Get pointer to raw data (mutable)
    void* data() { return m_data; }
    
    /// Get pointer to raw data (const)
    const void* data() const { return m_data; }
    
    /// Get current size in bytes
    size_t size() const { return m_size; }
    
    /// Get allocated capacity
    size_t capacity() const { return m_capacity; }
    
    /// Check if buffer is empty
    bool empty() const { return m_size == 0; }
    
    // ========================================================================
    // Typed Access
    // ========================================================================
    
    /// Access as bytes (mutable)
    uint8_t* bytes() { return static_cast<uint8_t*>(m_data); }
    
    /// Access as bytes (const)
    const uint8_t* bytes() const { return static_cast<const uint8_t*>(m_data); }
    
    /// Access as chars (mutable, null-terminated)
    char* chars() { return static_cast<char*>(m_data); }
    
    /// Access as chars (const, null-terminated)
    const char* chars() const { return static_cast<const char*>(m_data); }
    
    /// Access as string_view (convenience)
    std::string_view as_string_view() const { 
        return std::string_view(static_cast<const char*>(m_data), m_size); 
    }
    
    /// Access as std::string
    std::string as_string() const { 
        return std::string(static_cast<const char*>(m_data), m_size); 
    }
    
    /// Access single byte (mutable)
    uint8_t& operator[](size_t index);
    
    /// Access single byte (const)
    uint8_t operator[](size_t index) const;
    
    // ========================================================================
    // Capacity Management
    // ========================================================================
    
    /// Reserve capacity (won't shrink existing capacity)
    void reserve(size_t capacity);
    
    /// Resize buffer to exact size (truncates or zero-fills)
    void resize(size_t size);
    
    /// Clear buffer contents (size=0, keeps capacity)
    void clear();
    
    /// Swap contents with another buffer (O(1))
    void swap(ByteBuffer& other) noexcept;
    
    // ========================================================================
    // Data Operations
    // ========================================================================
    
    /// Append binary data
    void append(const void* data, size_t size);
    
    /// Append string data
    void append(const std::string& str);
    
    /// Append string_view data
    void append(std::string_view view);
    
    /// Append single byte
    void append(uint8_t byte);
    
    /// Append single char
    void append(char c);
    
    /// Append another buffer's contents
    void append(const ByteBuffer& other);
    
    // ========================================================================
    // File Operations
    // ========================================================================
    
    /// Write buffer contents to file (binary mode)
    /// Returns true on success, false on error
    bool write_to_file(const std::string& path) const;
    
    /// Read file contents into buffer (binary mode)
    /// Returns true on success, false on error
    bool read_from_file(const std::string& path);
    
    // ========================================================================
    // Query Operations
    // ========================================================================
    
    /// Find byte sequence, returns position or npos
    size_t find(const void* pattern, size_t pattern_size, size_t start = 0) const;
    
    /// Find string, returns position or npos
    size_t find(const std::string& str, size_t start = 0) const;
    
    /// Check if buffer starts with pattern
    bool starts_with(const void* data, size_t size) const;
    
    /// Check if buffer starts with string
    bool starts_with(const std::string& str) const;
    
    /// Check if buffer ends with pattern
    bool ends_with(const void* data, size_t size) const;
    
    /// Check if buffer ends with string
    bool ends_with(const std::string& str) const;

private:
    void* m_data = nullptr;
    size_t m_size = 0;
    size_t m_capacity = 0;
    
    /// Ensure capacity for at least 'needed' bytes
    void ensure_capacity(size_t needed);
    
    /// Free allocated memory
    void free_data();
    
    /// Default initial capacity
    static constexpr size_t DEFAULT_CAPACITY = 256;
    
    /// Growth factor when reallocating
    static constexpr double GROWTH_FACTOR = 1.5;
};

// ========================================================================
// libcurl Callback Helper
// ========================================================================

/// libcurl write callback - appends received data to ByteBuffer
/// This is the recommended callback for CURLOPT_WRITEFUNCTION
/// Usage:
///   curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_callback);
///   curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buffer);
extern "C" size_t curl_write_callback(void* contents, size_t size, size_t nmemb, void* userp);

/// libcurl header callback - appends headers to ByteBuffer
/// Usage:
///   curl_easy_setopt(curl, CURLOPT_HEADERFUNCTION, curl_header_callback);
///   curl_easy_setopt(curl, CURLOPT_HEADERDATA, &header_buffer);
extern "C" size_t curl_header_callback(void* contents, size_t size, size_t nmemb, void* userp);

} // namespace lucid