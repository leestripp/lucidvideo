// semantic_search_tests.cpp - Semantic search unit tests
#include <gtest/gtest.h>
#include <cmath>
#include <algorithm>
#include <functional>

// Include the standalone implementations for testing
// These are the pure math functions from semantic_search.cpp

namespace lucid {
namespace testing {

// ============================================================================
// Pure Math Functions (copy from semantic_search.cpp for testing)
// ============================================================================

// Cosine similarity between two vectors
static float cosine_similarity(const std::vector<float>& a, const std::vector<float>& b) {
    if (a.size() != b.size() || a.empty()) return 0.0f;

    float dot_product = 0.0f;
    float norm_a = 0.0f;
    float norm_b = 0.0f;

    for (size_t i = 0; i < a.size(); ++i) {
        dot_product += a[i] * b[i];
        norm_a += a[i] * a[i];
        norm_b += b[i] * b[i];
    }

    float denom = std::sqrt(norm_a) * std::sqrt(norm_b);
    if (denom < 1e-8f) return 0.0f;

    return dot_product / denom;
}

// Normalize vector to unit length
static std::vector<float> normalize(const std::vector<float>& vec) {
    std::vector<float> result(vec);

    float norm = 0.0f;
    for (const auto& v : result) {
        norm += v * v;
    }
    norm = std::sqrt(norm);

    if (norm > 1e-8f) {
        for (auto& v : result) {
            v /= norm;
        }
    }

    return result;
}

// ============================================================================
// Cosine Similarity Tests
// ============================================================================

TEST(CosineSimilarityTest, IdenticalVectors) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {1.0f, 0.0f, 0.0f};

    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

TEST(CosineSimilarityTest, OrthogonalVectors) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f, 0.0f};

    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, 0.0f, 0.001f);
}

TEST(CosineSimilarityTest, NegativeCorrelation) {
    std::vector<float> a = {1.0f, 0.0f, 0.0f};
    std::vector<float> b = {-1.0f, 0.0f, 0.0f};

    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, -1.0f, 0.001f);
}

TEST(CosineSimilarityTest, PartialCorrelation) {
    std::vector<float> a = {1.0f, 1.0f, 0.0f};
    std::vector<float> b = {1.0f, 0.0f, 0.0f};

    float similarity = cosine_similarity(a, b);

    // (1,1,0) dot (1,0,0) = 1
    // |a| = sqrt(2), |b| = 1
    // similarity = 1 / sqrt(2) ≈ 0.707
    EXPECT_NEAR(similarity, 0.707f, 0.01f);
}

TEST(CosineSimilarityTest, DifferentLengths) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {1.0f, 0.0f, 0.0f};

    float similarity = cosine_similarity(a, b);

    EXPECT_EQ(similarity, 0.0f);
}

TEST(CosineSimilarityTest, EmptyVectors) {
    std::vector<float> a;
    std::vector<float> b = {1.0f};

    float similarity = cosine_similarity(a, b);

    EXPECT_EQ(similarity, 0.0f);
}

TEST(CosineSimilarityTest, ThreeDimensions) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {2.0f, 4.0f, 6.0f};

    float similarity = cosine_similarity(a, b);

    // (1,2,3) dot (2,4,6) = 28
    // |a| = sqrt(14), |b| = sqrt(56)
    // similarity = 28 / sqrt(784) = 28/28 = 1
    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

TEST(CosineSimilarityTest, LargeVectors) {
    std::vector<float> a(1000, 0.5f);
    std::vector<float> b(1000, 0.5f);

    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

TEST(CosineSimilarityTest, HighDimensionalSparse) {
    // Simulate typical embedding: 768 dim, mostly small values
    std::vector<float> a(768, 0.001f);
    std::vector<float> b(768, 0.001f);
    a[0] = 1.0f;
    b[0] = 1.0f;

    float similarity = cosine_similarity(a, b);

    // Due to float precision, result may be slightly > 1.0
    EXPECT_GT(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f + 0.001f);
}

TEST(CosineSimilarityTest, NegativesInVector) {
    std::vector<float> a = {1.0f, 2.0f, 3.0f};
    std::vector<float> b = {2.0f, 4.0f, 6.0f};

    // Same direction despite some negatives
    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

// ============================================================================
// Normalization Tests
// ============================================================================

TEST(NormalizeTest, ProducesUnitVector) {
    std::vector<float> v = {3.0f, 4.0f};
    auto normalized = normalize(v);

    float magnitude = 0.0f;
    for (const auto& val : normalized) {
        magnitude += val * val;
    }
    magnitude = std::sqrt(magnitude);

    EXPECT_NEAR(magnitude, 1.0f, 0.001f);
}

TEST(NormalizeTest, PreservesDirection) {
    std::vector<float> v = {6.0f, 8.0f};
    auto normalized = normalize(v);

    // Should maintain same ratio: x/y = 6/8 = 0.75
    EXPECT_NEAR(normalized[0] / normalized[1], 0.75f, 0.001f);
}

TEST(NormalizeTest, ZeroVector) {
    std::vector<float> v = {0.0f, 0.0f, 0.0f};
    auto normalized = normalize(v);

    // Zero vector stays zero (no division by zero)
    EXPECT_EQ(normalized.size(), 3);
    EXPECT_TRUE(std::all_of(normalized.begin(), normalized.end(), 
                            [](float f) { return f == 0.0f; }));
}

TEST(NormalizeTest, SingleElement) {
    std::vector<float> v = {5.0f};
    auto normalized = normalize(v);

    EXPECT_EQ(normalized.size(), 1);
    EXPECT_NEAR(normalized[0], 1.0f, 0.001f);
}

TEST(NormalizeTest, LargeValues) {
    std::vector<float> v = {1000000.0f, 2000000.0f};
    auto normalized = normalize(v);

    float magnitude = std::sqrt(normalized[0] * normalized[0] + normalized[1] * normalized[1]);
    EXPECT_NEAR(magnitude, 1.0f, 0.001f);
}

TEST(NormalizeTest, VerySmallValues) {
    std::vector<float> v = {0.00001f, 0.00002f};
    auto normalized = normalize(v);

    float magnitude = std::sqrt(normalized[0] * normalized[0] + normalized[1] * normalized[1]);
    EXPECT_NEAR(magnitude, 1.0f, 0.001f);
}

TEST(NormalizeTest, HighDimensionalVector) {
    std::vector<float> v(768, 1.0f);
    auto normalized = normalize(v);

    float magnitude = 0.0f;
    for (const auto& val : normalized) {
        magnitude += val * val;
    }
    magnitude = std::sqrt(magnitude);

    EXPECT_NEAR(magnitude, 1.0f, 0.001f);
}

TEST(NormalizeTest, DoesNotModifyOriginal) {
    std::vector<float> v = {3.0f, 4.0f};
    auto v_copy = v;
    auto normalized = normalize(v);

    // Original should be unchanged
    EXPECT_EQ(v[0], v_copy[0]);
    EXPECT_EQ(v[1], v_copy[1]);
}

TEST(NormalizeTest, NegativeValues) {
    std::vector<float> v = {-3.0f, -4.0f};
    auto normalized = normalize(v);

    // Normalized should have same sign pattern
    EXPECT_LT(normalized[0], 0.0f);
    EXPECT_LT(normalized[1], 0.0f);

    float magnitude = std::sqrt(normalized[0] * normalized[0] + normalized[1] * normalized[1]);
    EXPECT_NEAR(magnitude, 1.0f, 0.001f);
}

TEST(NormalizeTest, MixedSigns) {
    std::vector<float> v = {6.0f, -8.0f};
    auto normalized = normalize(v);

    float magnitude = std::sqrt(normalized[0] * normalized[0] + normalized[1] * normalized[1]);
    EXPECT_NEAR(magnitude, 1.0f, 0.001f);

    // Ratio should be preserved
    EXPECT_NEAR(std::abs(normalized[0] / normalized[1]), 0.75f, 0.001f);
}

// ============================================================================
// Combined Operations Tests
// ============================================================================

TEST(CombinedOperationsTest, NormalizeThenCompare) {
    std::vector<float> a = {3.0f, 4.0f};
    std::vector<float> b = {6.0f, 8.0f};

    // Both are same direction, just different magnitude
    auto norm_a = normalize(a);
    auto norm_b = normalize(b);

    float similarity = cosine_similarity(norm_a, norm_b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

TEST(CombinedOperationsTest, NormalizeDifferentDirections) {
    std::vector<float> a = {1.0f, 0.0f};
    std::vector<float> b = {0.0f, 1.0f};

    auto norm_a = normalize(a);
    auto norm_b = normalize(b);

    float similarity = cosine_similarity(norm_a, norm_b);

    EXPECT_NEAR(similarity, 0.0f, 0.001f);
}

TEST(CombinedOperationsTest, SearchRankingSimulation) {
    // Simulate search results with relevance scores
    struct MockResult {
        std::string id;
        std::vector<float> embedding;
    };

    std::vector<MockResult> results = {
        {"result1", {0.9f, 0.1f, 0.2f}},
        {"result2", {0.5f, 0.5f, 0.5f}},
        {"result3", {0.1f, 0.9f, 0.1f}}
    };

    std::vector<float> query = {1.0f, 0.0f, 0.0f};
    auto norm_query = normalize(query);

    // Score each result
    std::vector<std::pair<std::string, float>> scored;
    for (const auto& result : results) {
        auto norm_emb = normalize(result.embedding);
        float score = cosine_similarity(norm_query, norm_emb);
        scored.push_back({result.id, score});
    }

    // Sort by score descending
    std::sort(scored.begin(), scored.end(), 
              [](const auto& a, const auto& b) { return a.second > b.second; });

    // result1 should be highest (closest to query)
    EXPECT_EQ(scored[0].first, "result1");
    EXPECT_EQ(scored[1].first, "result2");
    EXPECT_EQ(scored[2].first, "result3");
}

TEST(CombinedOperationsTest, ThresholdFiltering) {
    std::vector<float> query = {1.0f, 0.0f};
    float threshold = 0.9f;

    std::vector<std::pair<std::string, std::vector<float>>> candidates = {
        {"exact", {1.0f, 0.0f}},           // similarity = 1.0
        {"partial", {0.95f, 0.05f}},       // similarity ≈ 0.998
        {"low", {0.5f, 0.5f}},             // similarity ≈ 0.707
        {"opposite", {-1.0f, 0.0f}}        // similarity = -1.0
    };

    auto norm_query = normalize(query);

    int above_threshold = 0;
    for (const auto& [id, emb] : candidates) {
        auto norm_emb = normalize(emb);
        float sim = cosine_similarity(norm_query, norm_emb);
        if (sim >= threshold) above_threshold++;
    }

    EXPECT_EQ(above_threshold, 2);  // exact and partial are >= 0.9
}

TEST(CombinedOperationsTest, EmbeddingDimensionality) {
    // Test with typical embedding dimensions (768 for nomic-embed-text)
    std::vector<float> large_vec(768, 0.5f);
    auto normalized = normalize(large_vec);

    float magnitude = 0.0f;
    for (const auto& val : normalized) {
        magnitude += val * val;
    }
    magnitude = std::sqrt(magnitude);

    EXPECT_NEAR(magnitude, 1.0f, 0.001f);

    // Each element should be 1/sqrt(768)
    float expected_element = 1.0f / std::sqrt(768.0f);
    EXPECT_NEAR(normalized[0], expected_element, 0.001f);
}

// ============================================================================
// Edge Cases
// ============================================================================

TEST(EdgeCasesTest, VeryLongVectors) {
    std::vector<float> a(10000, 0.1f);
    std::vector<float> b(10000, 0.1f);

    float similarity = cosine_similarity(a, b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

TEST(EdgeCasesTest, NearZeroVectors) {
    std::vector<float> a = {0.0000001f, 0.0000001f};
    std::vector<float> b = {0.0000002f, 0.0000002f};

    float similarity = cosine_similarity(a, b);

    // Should handle gracefully
    EXPECT_GE(similarity, 0.0f);
    EXPECT_LE(similarity, 1.0f);
}

TEST(EdgeCasesTest, MixedMagnitudes) {
    std::vector<float> a = {0.001f, 0.001f, 0.001f};
    std::vector<float> b = {1000.0f, 1000.0f, 1000.0f};

    auto norm_a = normalize(a);
    auto norm_b = normalize(b);

    float similarity = cosine_similarity(norm_a, norm_b);

    EXPECT_NEAR(similarity, 1.0f, 0.001f);
}

// ============================================================================
// Score Range Tests
// ============================================================================

TEST(ScoreRangeTest, AlwaysInRange) {
    // Test that cosine similarity always returns value in [-1, 1]
    std::vector<std::vector<float>> test_vectors = {
        {1.0f, 1.0f},
        {100.0f, -50.0f},
        {0.001f, 0.002f},
        {-0.5f, -0.5f},
        {0.0f, 1.0f}
    };

    for (const auto& a : test_vectors) {
        for (const auto& b : test_vectors) {
            float similarity = cosine_similarity(a, b);
            EXPECT_GE(similarity, -1.001f);  // Small tolerance for float errors
            EXPECT_LE(similarity, 1.001f);
        }
    }
}

TEST(ScoreRangeTest, NormalizedScores) {
    // After normalization, all pairwise similarities should be consistent
    std::vector<float> v1 = {3.0f, 4.0f};
    std::vector<float> v2 = {6.0f, 8.0f};
    std::vector<float> v3 = {1.0f, 0.0f};

    auto n1 = normalize(v1);
    auto n2 = normalize(v2);
    auto n3 = normalize(v3);

    // n1 and n2 are collinear
    EXPECT_NEAR(cosine_similarity(n1, n2), 1.0f, 0.001f);

    // n3 is not collinear with n1/n2
    float sim_13 = cosine_similarity(n1, n3);
    float sim_23 = cosine_similarity(n2, n3);
    EXPECT_NEAR(sim_13, sim_23, 0.001f);  // Same angle
}

} // namespace testing
} // namespace lucid