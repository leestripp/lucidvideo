// byte_buffer_tests.cpp - Unit tests for ByteBuffer class
#include <gtest/gtest.h>
#include <lucid/buffer/byte_buffer.hpp>
#include <cstring>
#include <fstream>
#include <filesystem>

using namespace lucid;

class ByteBufferTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {}
    
    // Helper to create temp file
    static std::string make_temp_file(const void* data, size_t size) {
        std::string path = "/tmp/byte_buffer_test_" + std::to_string(rand()) + ".bin";
        std::ofstream file(path, std::ios::binary);
        file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
        file.close();
        return path;
    }
};

// ========================================================================
// Constructor Tests
// ========================================================================

TEST_F(ByteBufferTest, DefaultConstructorCreatesEmptyBuffer) {
    ByteBuffer buffer;
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_EQ(buffer.capacity(), 0u);
}

TEST_F(ByteBufferTest, ConstructorWithCapacity) {
    ByteBuffer buffer(1024);
    EXPECT_TRUE(buffer.empty());
    EXPECT_GE(buffer.capacity(), 1024u);
}

TEST_F(ByteBufferTest, ConstructorFromData) {
    const char* data = "Hello, World!";
    size_t size = strlen(data);
    ByteBuffer buffer(data, size);
    
    EXPECT_EQ(buffer.size(), size);
    EXPECT_EQ(memcmp(buffer.data(), data, size), 0);
}

TEST_F(ByteBufferTest, ConstructorFromString) {
    std::string str = "Test string";
    ByteBuffer buffer(str);
    
    EXPECT_EQ(buffer.size(), str.size());
    EXPECT_EQ(buffer.as_string_view(), str);
}

TEST_F(ByteBufferTest, MoveConstructor) {
    ByteBuffer original(std::string("test data"));
    ByteBuffer moved(std::move(original));
    
    EXPECT_EQ(moved.size(), 9u);
    EXPECT_TRUE(original.empty());
}

TEST_F(ByteBufferTest, MoveAssignment) {
    ByteBuffer buffer1;
    ByteBuffer buffer2(std::string("test data"));
    
    buffer1 = std::move(buffer2);
    
    EXPECT_EQ(buffer1.size(), 9u);
    EXPECT_TRUE(buffer2.empty());
}

// ========================================================================
// Append Tests
// ========================================================================

TEST_F(ByteBufferTest, AppendBinaryData) {
    ByteBuffer buffer;
    const uint8_t data[] = {0x00, 0x01, 0x02, 0xFF};
    buffer.append(data, 4);
    
    EXPECT_EQ(buffer.size(), 4u);
    EXPECT_EQ(memcmp(buffer.bytes(), data, 4), 0);
}

TEST_F(ByteBufferTest, AppendString) {
    ByteBuffer buffer;
    buffer.append(std::string("Hello"));
    buffer.append(std::string(" World"));
    
    EXPECT_EQ(buffer.as_string_view(), "Hello World");
}

TEST_F(ByteBufferTest, AppendSingleByte) {
    ByteBuffer buffer;
    buffer.append(static_cast<uint8_t>(42));
    buffer.append(static_cast<char>('A'));
    
    EXPECT_EQ(buffer.size(), 2u);
    EXPECT_EQ(buffer[0], 42);
    EXPECT_EQ(buffer[1], 'A');
}

TEST_F(ByteBufferTest, AppendMultipleTimes) {
    ByteBuffer buffer;
    
    for (int i = 0; i < 100; i++) {
        buffer.append(static_cast<uint8_t>(i));
    }
    
    EXPECT_EQ(buffer.size(), 100u);
    for (int i = 0; i < 100; i++) {
        EXPECT_EQ(buffer[i], static_cast<uint8_t>(i));
    }
}

TEST_F(ByteBufferTest, AppendFromBuffer) {
    ByteBuffer source("source data");
    ByteBuffer dest;
    dest.append(source);
    
    EXPECT_EQ(dest.size(), source.size());
    EXPECT_EQ(dest.as_string_view(), "source data");
}

// ========================================================================
// Clear and Resize Tests
// ========================================================================

TEST_F(ByteBufferTest, ClearKeepsCapacity) {
    ByteBuffer buffer("test data");
    size_t capacity = buffer.capacity();
    
    buffer.clear();
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.capacity(), capacity);
}

TEST_F(ByteBufferTest, ResizeTruncates) {
    ByteBuffer buffer("Hello World");
    buffer.resize(5);
    
    EXPECT_EQ(buffer.size(), 5u);
    EXPECT_EQ(buffer.as_string_view(), "Hello");
}

TEST_F(ByteBufferTest, ResizeExtendsWithZeros) {
    ByteBuffer buffer("Hi");
    buffer.resize(5);
    
    EXPECT_EQ(buffer.size(), 5u);
    EXPECT_EQ(buffer[0], 'H');
    EXPECT_EQ(buffer[1], 'i');
    EXPECT_EQ(buffer[2], 0);
    EXPECT_EQ(buffer[3], 0);
    EXPECT_EQ(buffer[4], 0);
}

TEST_F(ByteBufferTest, ReserveGrowsCapacity) {
    ByteBuffer buffer;
    buffer.reserve(1024);
    
    EXPECT_GE(buffer.capacity(), 1024u);
    EXPECT_TRUE(buffer.empty());
}

// ========================================================================
// Access Tests
// ========================================================================

TEST_F(ByteBufferTest, ByteAccess) {
    ByteBuffer buffer;
    const uint8_t data[] = {10, 20, 30};
    buffer.append(data, 3);
    
    EXPECT_EQ(buffer.bytes()[0], 10);
    EXPECT_EQ(buffer.bytes()[1], 20);
    EXPECT_EQ(buffer.bytes()[2], 30);
}

TEST_F(ByteBufferTest, CharAccess) {
    ByteBuffer buffer("ABC");
    
    EXPECT_EQ(buffer.chars()[0], 'A');
    EXPECT_EQ(buffer.chars()[1], 'B');
    EXPECT_EQ(buffer.chars()[2], 'C');
}

TEST_F(ByteBufferTest, IndexOperator) {
    ByteBuffer buffer("XYZ");
    
    EXPECT_EQ(buffer[0], 'X');
    EXPECT_EQ(buffer[1], 'Y');
    EXPECT_EQ(buffer[2], 'Z');
}

TEST_F(ByteBufferTest, StringViewAccess) {
    std::string original = "Hello, World!";
    ByteBuffer buffer(original);
    
    EXPECT_EQ(buffer.as_string_view(), original);
}

// ========================================================================
// File I/O Tests
// ========================================================================

TEST_F(ByteBufferTest, WriteAndReadFile) {
    const char* data = "Binary test data with null\0 byte";
    size_t size = strlen(data) + 1; // Include null byte
    
    // Write
    {
        ByteBuffer buffer(data, size);
        std::string path = "/tmp/byte_buffer_io_test.bin";
        EXPECT_TRUE(buffer.write_to_file(path));
        
        // Verify file exists
        std::ifstream file(path, std::ios::binary);
        EXPECT_TRUE(file.good());
    }
    
    // Read back
    {
        ByteBuffer buffer;
        std::string path = "/tmp/byte_buffer_io_test.bin";
        EXPECT_TRUE(buffer.read_from_file(path));
        EXPECT_EQ(buffer.size(), size);
        EXPECT_EQ(memcmp(buffer.data(), data, size), 0);
        
        // Cleanup
        std::filesystem::remove(path);
    }
}

TEST_F(ByteBufferTest, WriteEmptyBufferFails) {
    ByteBuffer buffer;
    EXPECT_FALSE(buffer.write_to_file("/tmp/empty_buffer_test.bin"));
}

// ========================================================================
// Search Tests
// ========================================================================

TEST_F(ByteBufferTest, FindPattern) {
    ByteBuffer buffer("Hello World");
    
    // Find "World" using string overload
    size_t pos = buffer.find(std::string("World"));
    EXPECT_EQ(pos, 6u);
    
    // Find "l" using string overload
    pos = buffer.find(std::string("l"));
    EXPECT_EQ(pos, 2u);
    
    // Find second "l"
    pos = buffer.find(std::string("l"), pos + 1);
    EXPECT_EQ(pos, 3u);
    
    // Not found
    pos = buffer.find(std::string("XYZ"));
    EXPECT_EQ(pos, std::string::npos);
}

TEST_F(ByteBufferTest, StartsWith) {
    ByteBuffer buffer("Hello World");
    
    EXPECT_TRUE(buffer.starts_with("Hello"));
    EXPECT_TRUE(buffer.starts_with("Hell"));
    EXPECT_TRUE(buffer.starts_with("H"));
    EXPECT_FALSE(buffer.starts_with("World"));
    EXPECT_FALSE(buffer.starts_with("Hello World!"));
}

TEST_F(ByteBufferTest, EndsWith) {
    ByteBuffer buffer("Hello World");
    
    EXPECT_TRUE(buffer.ends_with("World"));
    EXPECT_TRUE(buffer.ends_with("rld"));
    EXPECT_TRUE(buffer.ends_with("d"));
    EXPECT_FALSE(buffer.ends_with("Hello"));
    EXPECT_FALSE(buffer.ends_with("Hello World!"));
}

// ========================================================================
// Swap Tests
// ========================================================================

TEST_F(ByteBufferTest, Swap) {
    ByteBuffer buffer1("First");
    ByteBuffer buffer2("Second");
    
    size_t size1 = buffer1.size();
    size_t size2 = buffer2.size();
    
    buffer1.swap(buffer2);
    
    EXPECT_EQ(buffer1.size(), size2);
    EXPECT_EQ(buffer2.size(), size1);
    EXPECT_EQ(buffer1.as_string_view(), "Second");
    EXPECT_EQ(buffer2.as_string_view(), "First");
}

// ========================================================================
// Edge Cases
// ========================================================================

TEST_F(ByteBufferTest, AppendEmptyData) {
    ByteBuffer buffer("Hello");
    buffer.append(static_cast<const void*>(nullptr), 0);
    EXPECT_EQ(buffer.size(), 5u);
}

TEST_F(ByteBufferTest, LargeBuffer) {
    ByteBuffer buffer;
    size_t target_size = 10 * 1024 * 1024; // 10 MB
    
    for (size_t i = 0; i < target_size; i++) {
        buffer.append(static_cast<uint8_t>(i & 0xFF));
    }
    
    EXPECT_EQ(buffer.size(), target_size);
    EXPECT_EQ(buffer[0], 0);
    EXPECT_EQ(buffer[1], 1);
    EXPECT_EQ(buffer[target_size - 1], static_cast<uint8_t>((target_size - 1) & 0xFF));
}

TEST_F(ByteBufferTest, BinaryDataPreservesZeros) {
    const uint8_t data[] = {0x00, 0x01, 0x00, 0x02, 0x00};
    ByteBuffer buffer(data, 5);
    
    EXPECT_EQ(buffer.size(), 5u);
    for (size_t i = 0; i < 5; i++) {
        EXPECT_EQ(buffer[i], data[i]);
    }
}

TEST_F(ByteBufferTest, StringWithNullByte) {
    const char data[] = "Hello\0World";
    ByteBuffer buffer(data, 11);
    
    EXPECT_EQ(buffer.size(), 11u);
    EXPECT_EQ(buffer.as_string_view().substr(0, 5), "Hello");
    EXPECT_EQ(buffer.as_string_view().substr(6), "World");
}

// ========================================================================
// libcurl Callback Tests
// ========================================================================

TEST_F(ByteBufferTest, CurlWriteCallback) {
    ByteBuffer buffer;
    const char data[] = "HTTP/1.1 200 OK\r\nContent-Length: 100";
    
    size_t result = curl_write_callback(
        const_cast<char*>(data),  // curl passes non-const
        1,                         // size (single bytes)
        strlen(data),              // nmemb
        &buffer
    );
    
    EXPECT_EQ(result, strlen(data));
    EXPECT_EQ(buffer.size(), strlen(data));
    EXPECT_EQ(buffer.as_string_view(), data);
}

TEST_F(ByteBufferTest, CurlWriteCallbackMultipleCalls) {
    ByteBuffer buffer;
    
    const char* chunk1 = "Hello ";
    const char* chunk2 = "World!";
    
    curl_write_callback(const_cast<char*>(chunk1), 1, strlen(chunk1), &buffer);
    curl_write_callback(const_cast<char*>(chunk2), 1, strlen(chunk2), &buffer);
    
    EXPECT_EQ(buffer.as_string_view(), "Hello World!");
}

TEST_F(ByteBufferTest, CurlWriteCallbackHandlesNull) {
    ByteBuffer buffer;
    
    size_t result = curl_write_callback(nullptr, 1, 10, &buffer);
    EXPECT_EQ(result, 0);
    EXPECT_TRUE(buffer.empty());
}

// ========================================================================
// Additional Tests: C-string Constructor
// ========================================================================

TEST_F(ByteBufferTest, ConstructorFromCString) {
    ByteBuffer buffer("Hello from c-string");
    
    EXPECT_EQ(buffer.size(), 19u); // "Hello from c-string" is 19 chars
    EXPECT_EQ(buffer.as_string_view(), "Hello from c-string");
}

TEST_F(ByteBufferTest, ConstructorFromNullCString) {
    ByteBuffer buffer(static_cast<const char*>(nullptr));
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0u);
}

TEST_F(ByteBufferTest, ConstructorFromEmptyCString) {
    ByteBuffer buffer("");
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0u);
}

// ========================================================================
// Additional Tests: as_string() Method
// ========================================================================

TEST_F(ByteBufferTest, AsString) {
    ByteBuffer buffer("Test string content");
    
    std::string str = buffer.as_string();
    EXPECT_EQ(str, "Test string content");
    EXPECT_EQ(str.size(), buffer.size());
}

// ========================================================================
// Additional Tests: String_view Append
// ========================================================================

TEST_F(ByteBufferTest, AppendStringView) {
    ByteBuffer buffer;
    std::string_view view = "Hello World";
    
    buffer.append(view);
    
    EXPECT_EQ(buffer.size(), view.size());
    EXPECT_EQ(buffer.as_string_view(), view);
}

// ========================================================================
// Additional Tests: Binary File Integrity
// ========================================================================

TEST_F(ByteBufferTest, FileRoundTripBinary) {
    // Create binary data with all possible byte values
    std::vector<uint8_t> original_data;
    for (int i = 0; i < 256; i++) {
        original_data.push_back(static_cast<uint8_t>(i));
    }
    
    // Write to file
    std::string path = "/tmp/byte_buffer_binary_test.bin";
    {
        ByteBuffer buffer(original_data.data(), original_data.size());
        EXPECT_TRUE(buffer.write_to_file(path));
    }
    
    // Read back
    ByteBuffer read_buffer;
    EXPECT_TRUE(read_buffer.read_from_file(path));
    EXPECT_EQ(read_buffer.size(), 256u);
    
    // Verify all bytes
    for (size_t i = 0; i < 256; i++) {
        EXPECT_EQ(read_buffer[i], original_data[i]) << "Mismatch at byte " << i;
    }
    
    // Cleanup
    std::filesystem::remove(path);
}

// ========================================================================
// Additional Tests: Zero Copy Behavior
// ========================================================================

TEST_F(ByteBufferTest, MovePreservesData) {
    ByteBuffer original("Important data that must survive move");
    size_t original_size = original.size();
    const void* original_data = original.data();
    
    ByteBuffer moved(std::move(original));
    
    // Data should be preserved (same memory location)
    EXPECT_EQ(moved.size(), original_size);
    EXPECT_EQ(moved.data(), original_data);
    
    // Original should be empty
    EXPECT_TRUE(original.empty());
    EXPECT_EQ(original.size(), 0u);
    EXPECT_EQ(original.capacity(), 0u);
}

TEST_F(ByteBufferTest, MoveAssignmentPreservesData) {
    ByteBuffer buffer1("Data to preserve");
    size_t size1 = buffer1.size();
    const void* data1 = buffer1.data();
    
    ByteBuffer buffer2;
    buffer2 = std::move(buffer1);
    
    EXPECT_EQ(buffer2.size(), size1);
    EXPECT_EQ(buffer2.data(), data1);
    EXPECT_TRUE(buffer1.empty());
}

// ========================================================================
// Additional Tests: Capacity Growth
// ========================================================================

TEST_F(ByteBufferTest, AutomaticGrowth) {
    ByteBuffer buffer(10); // Start small
    
    // Append more than initial capacity
    for (int i = 0; i < 100; i++) {
        buffer.append(static_cast<uint8_t>(i));
    }
    
    EXPECT_EQ(buffer.size(), 100u);
    EXPECT_GE(buffer.capacity(), 100u);
}

TEST_F(ByteBufferTest, ReserveDoesNotShrink) {
    ByteBuffer buffer(1000);
    buffer.append(std::string("test"));
    size_t capacity = buffer.capacity();
    
    buffer.reserve(100); // Should not shrink
    
    EXPECT_EQ(buffer.capacity(), capacity);
}

// ========================================================================
// Additional Tests: Search Edge Cases
// ========================================================================

TEST_F(ByteBufferTest, FindEmptyPattern) {
    ByteBuffer buffer("Hello");
    size_t pos = buffer.find(std::string(""));
    
    EXPECT_EQ(pos, std::string::npos);
}

TEST_F(ByteBufferTest, FindPatternAtStart) {
    ByteBuffer buffer("Hello World");
    size_t pos = buffer.find(std::string("Hello"));
    
    EXPECT_EQ(pos, 0u);
}

TEST_F(ByteBufferTest, FindPatternAtEnd) {
    ByteBuffer buffer("Hello World");
    size_t pos = buffer.find(std::string("World"));
    
    EXPECT_EQ(pos, 6u);
}

TEST_F(ByteBufferTest, FindWithStartOffset) {
    ByteBuffer buffer("Hello World");
    
    size_t pos = buffer.find(std::string("l"), 0);
    EXPECT_EQ(pos, 2u); // First 'l' in "Hello"
    
    pos = buffer.find(std::string("l"), pos + 1);
    EXPECT_EQ(pos, 3u); // Second 'l' in "Hello"
    
    pos = buffer.find(std::string("l"), pos + 1);
    EXPECT_EQ(pos, 9u); // 'l' in "World"
    
    pos = buffer.find(std::string("l"), pos + 1);
    EXPECT_EQ(pos, std::string::npos); // Beyond string length
}

// ========================================================================
// Additional Tests: StartsWith/EndsWith Edge Cases
// ========================================================================

TEST_F(ByteBufferTest, StartsWithEmpty) {
    ByteBuffer buffer("Hello");
    // Empty string: implementation returns false (ambiguous)
    EXPECT_FALSE(buffer.starts_with(std::string("")));
    EXPECT_FALSE(buffer.starts_with("", 0));
}

TEST_F(ByteBufferTest, EndsWithEmpty) {
    ByteBuffer buffer("Hello");
    // Empty string: implementation returns false (ambiguous)
    EXPECT_FALSE(buffer.ends_with(std::string("")));
    EXPECT_FALSE(buffer.ends_with("", 0));
}

TEST_F(ByteBufferTest, StartsWithLongerThanBuffer) {
    ByteBuffer buffer("Hi");
    EXPECT_FALSE(buffer.starts_with(std::string("Hello")));
    EXPECT_FALSE(buffer.starts_with("Hello", 5));
}

TEST_F(ByteBufferTest, EndsWithLongerThanBuffer) {
    ByteBuffer buffer("Hi");
    EXPECT_FALSE(buffer.ends_with(std::string("Hello")));
    EXPECT_FALSE(buffer.ends_with("Hello", 5));
}

// ========================================================================
// Additional Tests: Resize Edge Cases
// ========================================================================

TEST_F(ByteBufferTest, ResizeToZero) {
    ByteBuffer buffer("Hello");
    buffer.resize(0);
    
    EXPECT_TRUE(buffer.empty());
    EXPECT_EQ(buffer.size(), 0u);
    EXPECT_GE(buffer.capacity(), 5u);
}

TEST_F(ByteBufferTest, ResizeSameSize) {
    ByteBuffer buffer("Hello");
    size_t capacity = buffer.capacity();
    
    buffer.resize(5);
    
    EXPECT_EQ(buffer.size(), 5u);
    EXPECT_EQ(buffer.as_string_view(), "Hello");
    EXPECT_GE(buffer.capacity(), capacity);
}

// ========================================================================
// Additional Tests: Curl Header Callback
// ========================================================================

TEST_F(ByteBufferTest, CurlHeaderCallback) {
    ByteBuffer buffer;
    const char header[] = "Content-Type: application/json\r\n";
    
    size_t result = curl_header_callback(
        const_cast<char*>(header),
        1,
        strlen(header),
        &buffer
    );
    
    EXPECT_EQ(result, strlen(header));
    EXPECT_EQ(buffer.size(), strlen(header));
    EXPECT_TRUE(buffer.as_string_view().find("Content-Type") != std::string::npos);
}
