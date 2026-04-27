// main.cpp - Lucid Application Entry Point
// 
// Intelligent video library with AI-powered search

#include "lucid/app.hpp"
#include "lucid/logging.hpp"

int main(int argc, char** argv) {
    // Initialize logging
    lucid::initialize_logging(lucid::LogLevel::DEBUG);
    
    return lucid::LucidApp::create()->run(argc, argv);
}