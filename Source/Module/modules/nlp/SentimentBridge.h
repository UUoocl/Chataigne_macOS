/*
  ==============================================================================

    SentimentBridge.h
    macOS NaturalLanguage.framework sentiment analysis C++ bridge

  ==============================================================================
*/

#pragma once

#include <string>

struct SentimentResult
{
    double score = 0.0;
    std::string language;
    bool valid = false;
};

namespace SentimentBridge
{
    SentimentResult analyze(const std::string& text, const std::string& languageCode);
}
