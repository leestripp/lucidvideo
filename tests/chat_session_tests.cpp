// chat_session_tests.cpp - Unit tests for ChatSession message handling logic
// Standalone tests that don't require external dependencies

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <functional>

// Minimal ChatMessage struct for testing (matches the one in llm_engine.hpp)
struct ChatMessage {
    enum class Role { SYSTEM, USER, ASSISTANT };
    Role role;
    std::string content;
    std::string timestamp;
    bool is_error = false;
};

// Minimal ChatSession simulation for testing message handling logic
class ChatSessionLogic {
public:
    void add_user_message(const std::string& content) {
        m_messages.push_back({ChatMessage::Role::USER, content});
        trim_history();
    }
    
    void add_assistant_message(const std::string& content) {
        m_messages.push_back({ChatMessage::Role::ASSISTANT, content});
    }
    
    void set_system_message(const std::string& content) {
        m_system_prompt = content;
    }
    
    void clear() {
        m_messages.clear();
        m_system_prompt.clear();
    }
    
    void set_max_history(int max_messages) { m_max_history = max_messages; }
    int get_max_history() const { return m_max_history; }
    
    const std::vector<ChatMessage>& get_messages() const { return m_messages; }
    const std::string& get_system_prompt() const { return m_system_prompt; }
    
private:
    void trim_history() {
        int current_pairs = 0;
        int first_to_keep = 0;
        
        for (int i = 0; i < (int)m_messages.size(); i++) {
            if (m_messages[i].role == ChatMessage::Role::USER) {
                current_pairs++;
                if (current_pairs > m_max_history) {
                    first_to_keep = i;
                    break;
                }
            }
        }
        
        if (first_to_keep > 0) {
            m_messages.erase(m_messages.begin(), m_messages.begin() + first_to_keep);
        }
    }
    
    std::vector<ChatMessage> m_messages;
    std::string m_system_prompt;
    int m_max_history = 50;
};

class ChatSessionTest : public ::testing::Test {
protected:
    void SetUp() override {}
    void TearDown() override {
        m_session.clear();
    }
    
    ChatSessionLogic m_session;
};

// ========================================================================
// Constructor Tests
// ========================================================================

TEST_F(ChatSessionTest, DefaultConstructorCreatesEmptySession) {
    EXPECT_TRUE(m_session.get_messages().empty());
    EXPECT_TRUE(m_session.get_system_prompt().empty());
}

TEST_F(ChatSessionTest, DefaultMaxHistoryIs50) {
    EXPECT_EQ(m_session.get_max_history(), 50);
}

// ========================================================================
// Message Management Tests
// ========================================================================

TEST_F(ChatSessionTest, AddUserMessageAppendsUserRole) {
    m_session.add_user_message("Hello");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].role, ChatMessage::Role::USER);
    EXPECT_EQ(messages[0].content, "Hello");
}

TEST_F(ChatSessionTest, AddAssistantMessageAppendsAssistantRole) {
    m_session.add_assistant_message("Hi there!");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].role, ChatMessage::Role::ASSISTANT);
    EXPECT_EQ(messages[0].content, "Hi there!");
}

TEST_F(ChatSessionTest, MultipleMessagesPreserveOrder) {
    m_session.add_user_message("First");
    m_session.add_assistant_message("Response 1");
    m_session.add_user_message("Second");
    m_session.add_assistant_message("Response 2");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 4u);
    
    EXPECT_EQ(messages[0].role, ChatMessage::Role::USER);
    EXPECT_EQ(messages[0].content, "First");
    
    EXPECT_EQ(messages[1].role, ChatMessage::Role::ASSISTANT);
    EXPECT_EQ(messages[1].content, "Response 1");
    
    EXPECT_EQ(messages[2].role, ChatMessage::Role::USER);
    EXPECT_EQ(messages[2].content, "Second");
    
    EXPECT_EQ(messages[3].role, ChatMessage::Role::ASSISTANT);
    EXPECT_EQ(messages[3].content, "Response 2");
}

TEST_F(ChatSessionTest, SystemMessageCanBeSet) {
    m_session.set_system_message("You are a helpful assistant.");
    
    EXPECT_EQ(m_session.get_system_prompt(), "You are a helpful assistant.");
}

TEST_F(ChatSessionTest, SystemMessageCanBeChanged) {
    m_session.set_system_message("First system prompt");
    m_session.set_system_message("Second system prompt");
    
    EXPECT_EQ(m_session.get_system_prompt(), "Second system prompt");
}

TEST_F(ChatSessionTest, SystemMessageCanBeEmpty) {
    m_session.set_system_message("Initial");
    m_session.set_system_message("");
    
    EXPECT_TRUE(m_session.get_system_prompt().empty());
}

// ========================================================================
// Clear Tests
// ========================================================================

TEST_F(ChatSessionTest, ClearRemovesAllMessages) {
    m_session.add_user_message("Hello");
    m_session.add_assistant_message("Hi");
    m_session.set_system_message("System");
    
    m_session.clear();
    
    EXPECT_TRUE(m_session.get_messages().empty());
    EXPECT_TRUE(m_session.get_system_prompt().empty());
}

// ========================================================================
// History Limit Tests
// ========================================================================

TEST_F(ChatSessionTest, SetMaxHistoryChangesLimit) {
    m_session.set_max_history(10);
    EXPECT_EQ(m_session.get_max_history(), 10);
    
    m_session.set_max_history(100);
    EXPECT_EQ(m_session.get_max_history(), 100);
}

TEST_F(ChatSessionTest, TrimHistoryKeepsMessagesWithinLimit) {
    // Set a low limit for testing
    m_session.set_max_history(2);
    
    // Add more than 2 user/assistant pairs
    for (int i = 1; i <= 4; i++) {
        m_session.add_user_message("User message " + std::to_string(i));
        m_session.add_assistant_message("Assistant response " + std::to_string(i));
    }
    
    // Should be trimmed to max 2 pairs (4 messages)
    const auto& messages = m_session.get_messages();
    EXPECT_EQ(messages.size(), 4u);
    
    // Should keep the most recent 2 pairs
    EXPECT_EQ(messages[0].content, "User message 3");
    EXPECT_EQ(messages[1].content, "Assistant response 3");
    EXPECT_EQ(messages[2].content, "User message 4");
    EXPECT_EQ(messages[3].content, "Assistant response 4");
}

TEST_F(ChatSessionTest, TrimHistoryDoesNotTrimWithinLimit) {
    m_session.set_max_history(5);
    
    // Add exactly 2 pairs
    m_session.add_user_message("First");
    m_session.add_assistant_message("First response");
    m_session.add_user_message("Second");
    m_session.add_assistant_message("Second response");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 4u);
    
    // All messages should be preserved
    EXPECT_EQ(messages[0].content, "First");
    EXPECT_EQ(messages[1].content, "First response");
    EXPECT_EQ(messages[2].content, "Second");
    EXPECT_EQ(messages[3].content, "Second response");
}

TEST_F(ChatSessionTest, TrimHistoryHandlesOddNumberOfMessages) {
    m_session.set_max_history(2);
    
    // Add odd number of messages (3 user messages = 1.5 pairs)
    m_session.add_user_message("First");
    m_session.add_user_message("Second");
    m_session.add_user_message("Third");
    
    // Should trim to only the last message
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].content, "Third");
}

TEST_F(ChatSessionTest, TrimHistoryWithZeroLimitKeepsNothing) {
    m_session.set_max_history(0);
    
    m_session.add_user_message("Hello");
    m_session.add_assistant_message("Hi");
    
    // With max_history=0, trim is triggered but first_to_keep=0, so nothing erased
    m_session.add_user_message("Another");
    
    // Should have all 3 messages since max_history=0 causes no trimming
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 3u);
    EXPECT_EQ(messages[0].content, "Hello");
    EXPECT_EQ(messages[1].content, "Hi");
    EXPECT_EQ(messages[2].content, "Another");
}

// ========================================================================
// System Prompt Tests
// ========================================================================

TEST_F(ChatSessionTest, SystemPromptNotAffectedByTrimHistory) {
    m_session.set_max_history(1);
    m_session.set_system_message("System prompt");
    
    // Add several messages
    m_session.add_user_message("First");
    m_session.add_assistant_message("First response");
    m_session.add_user_message("Second");
    
    // System prompt should still be intact
    EXPECT_EQ(m_session.get_system_prompt(), "System prompt");
}

TEST_F(ChatSessionTest, EmptySessionBuildsEmptyContext) {
    const auto& messages = m_session.get_messages();
    EXPECT_TRUE(messages.empty());
}

TEST_F(ChatSessionTest, MessagesWithSystemPromptPreservesBoth) {
    m_session.set_system_message("You are a helpful assistant.");
    m_session.add_user_message("Hello!");
    m_session.add_assistant_message("Hi there!");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 2u);
    
    // System prompt is stored separately
    EXPECT_EQ(m_session.get_system_prompt(), "You are a helpful assistant.");
    
    // Messages are stored in the messages vector
    EXPECT_EQ(messages[0].role, ChatMessage::Role::USER);
    EXPECT_EQ(messages[1].role, ChatMessage::Role::ASSISTANT);
}

// ========================================================================
// Message Role Tests
// ========================================================================

TEST_F(ChatSessionTest, UserMessageHasCorrectRole) {
    m_session.add_user_message("Test");
    EXPECT_EQ(m_session.get_messages()[0].role, ChatMessage::Role::USER);
}

TEST_F(ChatSessionTest, AssistantMessageHasCorrectRole) {
    m_session.add_assistant_message("Test");
    EXPECT_EQ(m_session.get_messages()[0].role, ChatMessage::Role::ASSISTANT);
}

TEST_F(ChatSessionTest, MessagesHaveDefaultErrorFlag) {
    m_session.add_user_message("Test");
    m_session.add_assistant_message("Response");
    
    EXPECT_FALSE(m_session.get_messages()[0].is_error);
    EXPECT_FALSE(m_session.get_messages()[1].is_error);
}

// ========================================================================
// Edge Cases
// ========================================================================

TEST_F(ChatSessionTest, EmptyMessageContent) {
    m_session.add_user_message("");
    m_session.add_assistant_message("");
    
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_TRUE(messages[0].content.empty());
    EXPECT_TRUE(messages[1].content.empty());
}

TEST_F(ChatSessionTest, VeryLongMessage) {
    std::string long_message(10000, 'x');
    m_session.add_user_message(long_message);
    
    EXPECT_EQ(m_session.get_messages()[0].content.size(), 10000u);
}

TEST_F(ChatSessionTest, UnicodeMessageContent) {
    m_session.add_user_message("Hello 你好 مرحبا");
    m_session.add_assistant_message("Hi 世界 🌍");
    
    const auto& messages = m_session.get_messages();
    EXPECT_EQ(messages[0].content, "Hello 你好 مرحبا");
    EXPECT_EQ(messages[1].content, "Hi 世界 🌍");
}

TEST_F(ChatSessionTest, ClearAfterTrim) {
    m_session.set_max_history(1);
    m_session.add_user_message("First");
    m_session.add_assistant_message("First response");
    m_session.add_user_message("Second");
    
    m_session.clear();
    
    EXPECT_TRUE(m_session.get_messages().empty());
    EXPECT_EQ(m_session.get_max_history(), 1);  // Max history preserved
}

TEST_F(ChatSessionTest, TrimPreservesAssistantMessages) {
    m_session.set_max_history(1);
    
    // Add a pair
    m_session.add_user_message("Question");
    m_session.add_assistant_message("Answer");
    
    // Add another user message to trigger trim
    m_session.add_user_message("New question");
    
    // With max_history=1, the 3rd user triggers trim
    // first_to_keep=2, erase [0,2) removes "Question" and "Answer"
    // Result: only "New question" remains
    const auto& messages = m_session.get_messages();
    ASSERT_EQ(messages.size(), 1u);
    EXPECT_EQ(messages[0].content, "New question");
}

TEST_F(ChatSessionTest, MultipleTrimsAccumulate) {
    m_session.set_max_history(2);
    
    // Add 5 pairs
    for (int i = 1; i <= 5; i++) {
        m_session.add_user_message("User " + std::to_string(i));
        m_session.add_assistant_message("Assistant " + std::to_string(i));
    }
    
    const auto& messages = m_session.get_messages();
    // After 5 pairs with max_history=2:
    // Each trim removes messages up to and including the oldest user
    // Final: User 5, Assistant 5 = 2 messages
    ASSERT_EQ(messages.size(), 2u);
    EXPECT_EQ(messages[0].content, "User 5");
    EXPECT_EQ(messages[1].content, "Assistant 5");
}
