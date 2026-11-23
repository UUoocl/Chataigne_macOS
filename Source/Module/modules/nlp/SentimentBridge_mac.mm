/*
  ==============================================================================

    SentimentBridge_mac.mm
    macOS NaturalLanguage.framework implementation of SentimentBridge

  ==============================================================================
*/

#include "SentimentBridge.h"

#if defined(__APPLE__)

#import <Foundation/Foundation.h>
#import <NaturalLanguage/NaturalLanguage.h>

namespace SentimentBridge
{
    SentimentResult analyze(const std::string& text, const std::string& languageCode)
    {
        SentimentResult res;
        if (text.empty()) return res;

        @autoreleasepool
        {
            if (@available(macOS 10.15, *))
            {
                NSString* nsText = [NSString stringWithUTF8String:text.c_str()];
                if (!nsText || [nsText length] == 0) return res;

                NLLanguage lang = nil;
                if (!languageCode.empty() && languageCode != "auto")
                {
                    lang = [NSString stringWithUTF8String:languageCode.c_str()];
                }
                else
                {
                    NLLanguageRecognizer* recognizer = [[NLLanguageRecognizer alloc] init];
                    [recognizer processString:nsText];
                    lang = recognizer.dominantLanguage;
                }

                if (lang)
                {
                    const char* langUtf8 = [lang UTF8String];
                    if (langUtf8) res.language = langUtf8;
                }

                NLTagger* tagger = [[NLTagger alloc] initWithTagSchemes:@[NLTagSchemeSentimentScore]];
                tagger.string = nsText;
                if (lang)
                    [tagger setLanguage:lang range:NSMakeRange(0, nsText.length)];

                NLTag tag = [tagger tagAtIndex:0 unit:NLTokenUnitParagraph scheme:NLTagSchemeSentimentScore tokenRange:nullptr];
                if (tag)
                {
                    res.score = [tag doubleValue];
                    res.valid = true;
                }
            }
        }
        return res;
    }
}

#else

namespace SentimentBridge
{
    SentimentResult analyze(const std::string&, const std::string&) { return {}; }
}

#endif
