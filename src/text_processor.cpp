#include "aiws/text_processor.hpp"

#include <cctype>
#include <stdexcept>

namespace aiws {

std::vector<TokenInfo> TextProcessor::tokenize(const std::string& text) {
    std::vector<TokenInfo> tokens;

    std::size_t paragraph = 0;
    std::size_t i = 0;

    while (i < text.size()) {

        // Check for paragraph boundaries while processing separators.
        if (text[i] == '\n' || text[i] == '\r') {
            std::size_t j = i;

            // Consume first newline.
            if (text[j] == '\r' &&
                j + 1 < text.size() &&
                text[j + 1] == '\n') {
                j += 2;
            } else {
                ++j;
            }

            // Spaces/tabs are allowed between the two newlines.
            while (j < text.size() &&
                   (text[j] == ' ' || text[j] == '\t')) {
                ++j;
            }

            // Check for second newline.
            bool blank_line = false;

            if (j < text.size()) {
                if (text[j] == '\n') {
                    blank_line = true;
                } else if (text[j] == '\r') {
                    blank_line = true;
                }
            }

            if (blank_line) {
                ++paragraph;
            }
        }

        unsigned char ch = static_cast<unsigned char>(text[i]);

        // Skip separators.
        if (!((ch >= 'A' && ch <= 'Z') ||
              (ch >= 'a' && ch <= 'z') ||
              (ch >= '0' && ch <= '9'))) {
            ++i;
            continue;
        }

        std::size_t begin = i;
        std::string token;

        // Build one normalized token.
        while (i < text.size()) {
            unsigned char current =
                static_cast<unsigned char>(text[i]);

            if (current >= 'A' && current <= 'Z') {
                token += static_cast<char>(current - 'A' + 'a');
            } else if ((current >= 'a' && current <= 'z') ||
                       (current >= '0' && current <= '9')) {
                token += static_cast<char>(current);
            } else {
                break;
            }

            ++i;
        }

        std::size_t end = i;

        tokens.push_back(
            TokenInfo{token, begin, end, paragraph});
    }

    return tokens;
}


std::vector<std::string> TextProcessor::terms(
    const std::string& text) {

    std::vector<TokenInfo> token_info = tokenize(text);
    std::vector<std::string> result;

    result.reserve(token_info.size());

    for (const TokenInfo& info : token_info) {
        result.push_back(info.token);
    }

    return result;
}


std::string TextProcessor::normalize(
    const std::string& text) {

    std::vector<TokenInfo> tokens = tokenize(text);

    return join(tokens, 0, tokens.size());
}


std::string TextProcessor::join(
    const std::vector<TokenInfo>& tokens,
    std::size_t begin,
    std::size_t end) {

    if (begin > end || end > tokens.size()) {
        throw std::out_of_range("invalid token range");
    }

    std::string result;

    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty()) {
            result += ' ';
        }

        result += tokens[i].token;
    }

    return result;
}


std::string TextProcessor::join(
    const std::vector<std::string>& tokens,
    std::size_t begin,
    std::size_t end) {

    if (begin > end || end > tokens.size()) {
        throw std::out_of_range("invalid token range");
    }

    std::string result;

    for (std::size_t i = begin; i < end; ++i) {
        if (!result.empty()) {
            result += ' ';
        }

        result += tokens[i];
    }

    return result;
}

}  // namespace aiws