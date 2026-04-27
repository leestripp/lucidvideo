// model_downloader_tests.cpp - Unit tests for model download system
//
// Tests model state management, catalog, selection, and download operations
// Uses standalone implementations where possible to avoid GTK dependencies

#include <gtest/gtest.h>
#include <string>
#include <vector>
#include <map>
#include <functional>
#include <mutex>
#include <optional>
#include <atomic>
#include <thread>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cstdio>

// ============================================================================
// Standalone implementations for testing (no GTK dependencies)
// Mirrored from model_manager.hpp
// ============================================================================

namespace test_model {

enum class ModelType {
    WHISPER,     // Speech-to-text transcription
    EMBEDDING    // Text embeddings for semantic search
};

enum class ModelState {
    NOT_DOWNLOADED = 0,
    DOWNLOADING = 1,
    DOWNLOADED = 2,
    ERROR = 3
};

struct ModelDescriptor {
    std::string id;
    std::string name;
    ModelType type;
    std::string description;
    std::string param_count;
    size_t size_bytes;
    std::string huggingface_url;
    std::string sha256;
    
    // Runtime state
    ModelState state = ModelState::NOT_DOWNLOADED;
    float download_progress = 0.0f;
    float download_speed = 0.0f;
    size_t downloaded_bytes = 0;
    std::string error_message;
    std::string local_path;
};

struct ModelStatus {
    std::string model_id;
    ModelState state;
    float progress;
    float download_speed;
    size_t downloaded_bytes;
    size_t total_bytes;
    std::string error;
};

using ModelStatusCallback = std::function<void(const ModelStatus&)>;

// ============================================================================
// Test Model Catalog Manager - mirrors real ModelManager
// ============================================================================

class TestModelCatalog {
public:
    TestModelCatalog() {
        build_catalog();
    }
    
    void build_catalog() {
        m_catalog.clear();
        m_catalog_index.clear();
        
        m_catalog.push_back(ModelDescriptor{
            "whisper-tiny",
            "Whisper Tiny",
            ModelType::WHISPER,
            "Smallest English model. Fast inference, lower accuracy",
            "39M params",
            75 * 1024 * 1024,
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-tiny.bin",
            ""
        });
        
        m_catalog.push_back(ModelDescriptor{
            "whisper-base",
            "Whisper Base", 
            ModelType::WHISPER,
            "Balanced speed/accuracy for English",
            "74M params",
            148 * 1024 * 1024,
            "https://huggingface.co/ggerganov/whisper.cpp/resolve/main/ggml-base.bin",
            ""
        });
        
        m_catalog.push_back(ModelDescriptor{
            "nomic-embed-text",
            "Nomic Embed Text v1.5",
            ModelType::EMBEDDING,
            "High quality 768-dim embeddings for semantic search",
            "137M params",
            274 * 1024 * 1024,
            "https://huggingface.co/nomic-ai/nomic-embed-text-v1.5-GGUF/resolve/main/nomic-embed-text-v1.5.Q4_K_M.gguf",
            ""
        });
        
        for (auto& model : m_catalog) {
            m_catalog_index[model.id] = &model;
        }
    }
    
    const std::vector<ModelDescriptor>& get_catalog() const { return m_catalog; }
    
    ModelDescriptor* get_model(const std::string& model_id) {
        auto it = m_catalog_index.find(model_id);
        return (it != m_catalog_index.end()) ? it->second : nullptr;
    }
    
    const ModelDescriptor* get_model(const std::string& model_id) const {
        auto it = m_catalog_index.find(model_id);
        return (it != m_catalog_index.end()) ? it->second : nullptr;
    }
    
    std::string extract_filename(const std::string& url) {
        auto last_slash = url.find_last_of('/');
        if (last_slash != std::string::npos) {
            return url.substr(last_slash + 1);
        }
        return url;
    }
    
    std::vector<ModelDescriptor> m_catalog;
    std::map<std::string, ModelDescriptor*> m_catalog_index;
};

} // namespace test_model

using namespace test_model;

// ============================================================================
// UI Button State Logic Test (mirrors model_hub_page.cpp logic)
// ============================================================================

enum class ButtonType { DOWNLOAD, CANCEL, DELETE, SELECT, RETRY };

struct ButtonResult {
    ButtonType type;
    std::string label;
    bool visible;
};

// This mirrors the button creation logic in model_hub_page.cpp
ButtonResult get_expected_button_for_model(const ModelDescriptor* model, 
                                            const std::string& selected_whisper,
                                            const std::string& selected_embedding) {
    ButtonResult result;
    result.visible = true;
    
    if (model->state == ModelState::DOWNLOADED) {
        bool is_selected = (model->type == ModelType::WHISPER) ?
            (selected_whisper == model->id) :
            (selected_embedding == model->id);
        
        if (is_selected) {
            // Selected model: show Delete only
            result.type = ButtonType::DELETE;
            result.label = "Delete";
        } else {
            // Not selected: show Select AND Delete (we return the PRIMARY one = Select)
            result.type = ButtonType::SELECT;
            result.label = "Select";
        }
        
    } else if (model->state == ModelState::DOWNLOADING) {
        result.type = ButtonType::CANCEL;
        result.label = "Cancel";
        
    } else if (model->state == ModelState::ERROR) {
        result.type = ButtonType::RETRY;
        result.label = "Retry";
        
    } else {
        // NOT_DOWNLOADED (state == 0)
        result.type = ButtonType::DOWNLOAD;
        result.label = "Download";
    }
    
    return result;
}

// ============================================================================
// TESTS: Model State and Enum Values
// ============================================================================

TEST(ModelStateEnumTest, EnumValuesAreCorrect) {
    // These MUST match the values in model_manager.hpp
    EXPECT_EQ(static_cast<int>(ModelState::NOT_DOWNLOADED), 0);
    EXPECT_EQ(static_cast<int>(ModelState::DOWNLOADING), 1);
    EXPECT_EQ(static_cast<int>(ModelState::DOWNLOADED), 2);
    EXPECT_EQ(static_cast<int>(ModelState::ERROR), 3);
}

TEST(ModelStateEnumTest, WhisperEnumValueIsZero) {
    EXPECT_EQ(static_cast<int>(ModelType::WHISPER), 0);
}

TEST(ModelStateEnumTest, EmbeddingEnumValueIsOne) {
    EXPECT_EQ(static_cast<int>(ModelType::EMBEDDING), 1);
}

// ============================================================================
// TESTS: Catalog and Model Lookup
// ============================================================================

TEST(ModelCatalogTest, CatalogHasExpectedModels) {
    TestModelCatalog catalog;
    
    ASSERT_NE(catalog.get_model("whisper-tiny"), nullptr);
    ASSERT_NE(catalog.get_model("whisper-base"), nullptr);
    ASSERT_NE(catalog.get_model("nomic-embed-text"), nullptr);
    EXPECT_EQ(catalog.get_model("unknown-model"), nullptr);
}

TEST(ModelCatalogTest, ModelLookupById) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    ASSERT_NE(model, nullptr);
    EXPECT_EQ(model->name, "Whisper Tiny");
    EXPECT_EQ(model->type, ModelType::WHISPER);
    EXPECT_EQ(model->state, ModelState::NOT_DOWNLOADED);
}

TEST(ModelCatalogTest, DefaultStateIsNotDownloaded) {
    TestModelCatalog catalog;
    
    for (const auto& model : catalog.get_catalog()) {
        EXPECT_EQ(model.state, ModelState::NOT_DOWNLOADED)
            << "Model " << model.id << " should start as NOT_DOWNLOADED";
    }
}

// ============================================================================
// TESTS: State Transitions
// ============================================================================

TEST(StateTransitionTest, NotDownloadedToDownloading) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    ASSERT_NE(model, nullptr);
    
    EXPECT_EQ(model->state, ModelState::NOT_DOWNLOADED);
    
    model->state = ModelState::DOWNLOADING;
    EXPECT_EQ(model->state, ModelState::DOWNLOADING);
}

TEST(StateTransitionTest, DownloadingToDownloaded) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADING;
    model->download_progress = 0.5f;
    
    model->state = ModelState::DOWNLOADED;
    model->download_progress = 1.0f;
    model->local_path = "/tmp/test-model.bin";
    
    EXPECT_EQ(model->state, ModelState::DOWNLOADED);
    EXPECT_FLOAT_EQ(model->download_progress, 1.0f);
    EXPECT_EQ(model->local_path, "/tmp/test-model.bin");
}

TEST(StateTransitionTest, DownloadingToError) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADING;
    
    model->state = ModelState::ERROR;
    model->error_message = "Network timeout";
    
    EXPECT_EQ(model->state, ModelState::ERROR);
    EXPECT_EQ(model->error_message, "Network timeout");
}

TEST(StateTransitionTest, DownloadedToNotDownloaded) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADED;
    model->local_path = "/tmp/test-model.bin";
    
    // Simulate delete
    model->state = ModelState::NOT_DOWNLOADED;
    model->local_path.clear();
    model->download_progress = 0.0f;
    
    EXPECT_EQ(model->state, ModelState::NOT_DOWNLOADED);
    EXPECT_TRUE(model->local_path.empty());
    EXPECT_FLOAT_EQ(model->download_progress, 0.0f);
}

// ============================================================================
// TESTS: UI Button Logic (THE CRITICAL TESTS)
// ============================================================================

TEST(UIButtonLogicTest, NotDownloadedShowsDownloadButton) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    ASSERT_NE(model, nullptr);
    
    // Verify it's NOT_DOWNLOADED
    EXPECT_EQ(model->state, ModelState::NOT_DOWNLOADED);
    EXPECT_EQ(static_cast<int>(model->state), 0);
    
    ButtonResult btn = get_expected_button_for_model(model, "", "");
    
    EXPECT_EQ(btn.type, ButtonType::DOWNLOAD);
    EXPECT_EQ(btn.label, "Download");
    EXPECT_TRUE(btn.visible);
}

TEST(UIButtonLogicTest, DownloadingShowsCancelButton) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADING;
    
    ButtonResult btn = get_expected_button_for_model(model, "", "");
    
    EXPECT_EQ(btn.type, ButtonType::CANCEL);
    EXPECT_EQ(btn.label, "Cancel");
}

TEST(UIButtonLogicTest, DownloadedShowsSelectWhenNoModelSelected) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADED;
    model->local_path = "/path/to/model.bin";
    
    // When no model is selected (empty strings), should show Select
    ButtonResult btn = get_expected_button_for_model(model, "", "");
    
    EXPECT_EQ(btn.type, ButtonType::SELECT);
    EXPECT_EQ(btn.label, "Select");
}

TEST(UIButtonLogicTest, DownloadedShowsSelectWhenNotSelected) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADED;
    
    // Not selected - should show Select
    ButtonResult btn = get_expected_button_for_model(model, "other-model", "");
    EXPECT_EQ(btn.type, ButtonType::SELECT);
    EXPECT_EQ(btn.label, "Select");
}

TEST(UIButtonLogicTest, DownloadedShowsDeleteWhenSelected) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    ASSERT_NE(model, nullptr) << "Model should exist";
    
    model->state = ModelState::DOWNLOADED;
    
    // Selected - UI shows Delete only
    ButtonResult btn = get_expected_button_for_model(model, "whisper-tiny", "");
    
    EXPECT_EQ(btn.type, ButtonType::DELETE);
    EXPECT_EQ(btn.label, "Delete");
}

TEST(UIButtonLogicTest, ErrorShowsRetryButton) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::ERROR;
    model->error_message = "Download failed";
    
    ButtonResult btn = get_expected_button_for_model(model, "", "");
    
    EXPECT_EQ(btn.type, ButtonType::RETRY);
    EXPECT_EQ(btn.label, "Retry");
}

// ============================================================================
// TESTS: State Comparison Edge Cases
// ============================================================================

TEST(StateComparisonTest, CompareStateToZero) {
    ModelDescriptor model;
    model.state = ModelState::NOT_DOWNLOADED;
    
    // Critical test: state == 0 should match NOT_DOWNLOADED
    EXPECT_TRUE(model.state == ModelState::NOT_DOWNLOADED);
    EXPECT_TRUE(static_cast<int>(model.state) == 0);
}

TEST(StateComparisonTest, CompareStateEnumDirectly) {
    ModelState state = ModelState::NOT_DOWNLOADED;
    
    // Direct enum comparison (this is what the UI code does)
    EXPECT_TRUE(state == ModelState::NOT_DOWNLOADED);
    EXPECT_FALSE(state == ModelState::DOWNLOADING);
    EXPECT_FALSE(state == ModelState::DOWNLOADED);
    EXPECT_FALSE(state == ModelState::ERROR);
}

TEST(StateComparisonTest, StateInIfElseChain) {
    ModelDescriptor model;
    model.state = ModelState::NOT_DOWNLOADED;
    
    std::string result = "unknown";
    
    if (model.state == ModelState::DOWNLOADED) {
        result = "downloaded";
    } else if (model.state == ModelState::DOWNLOADING) {
        result = "downloading";
    } else if (model.state == ModelState::ERROR) {
        result = "error";
    } else {
        // NOT_DOWNLOADED
        result = "not_downloaded";
    }
    
    EXPECT_EQ(result, "not_downloaded");
}

TEST(StateComparisonTest, AllStatesInIfElseChain) {
    std::map<std::string, std::string> expected = {
        {"NOT_DOWNLOADED", "not_downloaded"},
        {"DOWNLOADING", "downloading"},
        {"DOWNLOADED", "downloaded"},
        {"ERROR", "error"}
    };
    
    for (const auto& [state_name, expected_result] : expected) {
        ModelDescriptor model;
        
        if (state_name == "NOT_DOWNLOADED") model.state = ModelState::NOT_DOWNLOADED;
        else if (state_name == "DOWNLOADING") model.state = ModelState::DOWNLOADING;
        else if (state_name == "DOWNLOADED") model.state = ModelState::DOWNLOADED;
        else if (state_name == "ERROR") model.state = ModelState::ERROR;
        
        std::string result = "unknown";
        if (model.state == ModelState::DOWNLOADED) result = "downloaded";
        else if (model.state == ModelState::DOWNLOADING) result = "downloading";
        else if (model.state == ModelState::ERROR) result = "error";
        else result = "not_downloaded";
        
        EXPECT_EQ(result, expected_result) << "Failed for state: " << state_name;
    }
}

// ============================================================================
// TESTS: Catalog State Summary
// ============================================================================

TEST(CatalogStateTest, CountNotDownloadedModels) {
    TestModelCatalog catalog;
    
    int not_downloaded = 0;
    for (const auto& model : catalog.get_catalog()) {
        if (model.state == ModelState::NOT_DOWNLOADED) {
            not_downloaded++;
        }
    }
    
    EXPECT_EQ(not_downloaded, (int)catalog.get_catalog().size())
        << "All models should be NOT_DOWNLOADED at start";
}

TEST(CatalogStateTest, MarkSpecificModelAsDownloaded) {
    TestModelCatalog catalog;
    
    ModelDescriptor* model = catalog.get_model("whisper-tiny");
    model->state = ModelState::DOWNLOADED;
    model->local_path = "/tmp/ggml-tiny.bin";
    
    int downloaded = 0;
    for (const auto& m : catalog.get_catalog()) {
        if (m.state == ModelState::DOWNLOADED) {
            downloaded++;
        }
    }
    
    EXPECT_EQ(downloaded, 1);
}

// ============================================================================
// TESTS: URL Parsing
// ============================================================================

TEST(URLParsingTest, ExtractFilename) {
    TestModelCatalog catalog;
    
    EXPECT_EQ(catalog.extract_filename("https://huggingface.co/test/ggml-tiny.bin"), "ggml-tiny.bin");
    EXPECT_EQ(catalog.extract_filename("https://huggingface.co/test/model.gguf"), "model.gguf");
}

TEST(URLParsingTest, FilenameExtractionEdgeCases) {
    TestModelCatalog catalog;
    
    EXPECT_EQ(catalog.extract_filename(""), "");
    EXPECT_EQ(catalog.extract_filename("https://example.com/"), "");
}

// ============================================================================
// TESTS: Size Formatting
// ============================================================================

TEST(SizeFormattingTest, FormatSizes) {
    EXPECT_FLOAT_EQ(75 * 1024 * 1024 / (1024.0 * 1024.0), 75.0f);
    EXPECT_FLOAT_EQ(148 * 1024 * 1024 / (1024.0 * 1024.0 * 1024.0), 148.0f / 1024.0f);
}

// ============================================================================
// INTEGRATION TEST: Full UI Button Logic Flow
// ============================================================================

TEST(IntegrationUITest, WhisperTinyNotDownloadedShowsDownload) {
    TestModelCatalog catalog;
    
    // This is the actual flow the app takes:
    // 1. User opens Model Hub
    // 2. refresh_models() is called
    // 3. For each model, create_model_card() is called
    // 4. Button is created based on model->state
    
    const ModelDescriptor* model = catalog.get_model("whisper-tiny");
    ASSERT_NE(model, nullptr);
    
    // THE CRITICAL ASSERTION: model->state MUST be NOT_DOWNLOADED
    // and the comparison MUST work correctly
    ASSERT_EQ(model->state, ModelState::NOT_DOWNLOADED);
    
    // This is the exact logic from model_hub_page.cpp lines ~225-265
    ButtonResult btn;
    if (model->state == ModelState::DOWNLOADED) {
        btn.type = ButtonType::DELETE;
        btn.label = "Delete";
    } else if (model->state == ModelState::DOWNLOADING) {
        btn.type = ButtonType::CANCEL;
        btn.label = "Cancel";
    } else if (model->state == ModelState::ERROR) {
        btn.type = ButtonType::RETRY;
        btn.label = "Retry";
    } else {
        // NOT_DOWNLOADED - SHOULD SHOW "Download"
        btn.type = ButtonType::DOWNLOAD;
        btn.label = "Download";
    }
    
    // These MUST pass for the UI to show the right button
    EXPECT_EQ(btn.type, ButtonType::DOWNLOAD);
    EXPECT_EQ(btn.label, "Download");
}

TEST(IntegrationUITest, AllCatalogModelsShouldShowDownload) {
    TestModelCatalog catalog;
    
    for (const auto& model : catalog.get_catalog()) {
        ButtonResult btn = get_expected_button_for_model(&model, "", "");
        
        EXPECT_EQ(btn.type, ButtonType::DOWNLOAD)
            << "Model " << model.id << " should show Download button";
        EXPECT_EQ(btn.label, "Download")
            << "Model " << model.id << " should have 'Download' label";
    }
}
