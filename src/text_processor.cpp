#include "aiws/text_processor.hpp"

#include <stdexcept>

namespace aiws {

namespace {

bool is_ascii_alnum(unsigned char ch) {
    return (ch >= 'A' && ch <= 'Z') ||
           (ch >= 'a' && ch <= 'z') ||
           (ch >= '0' && ch <= '9');
}

}  // namespace

std::vector<TokenInfo> TextProcessor::tokenize(const std::string& text) {
    std::vector<TokenInfo> tokens;

    std::size_t paragraph = 0;
    std::size_t i = 0;

    while (i < text.size()) {
        bool paragraph_break = false;
        bool have_newline = false;
        bool only_spaces_tabs_since_newline = true;

        // Process separators before the next token.
        while (i < text.size() &&
               !is_ascii_alnum(static_cast<unsigned char>(text[i]))) {

            unsigned char ch =
                static_cast<unsigned char>(text[i]);

            // Treat LF and CRLF consistently as newline events.
            if (ch == '\n' || ch == '\r') {
                if (have_newline &&
                    only_spaces_tabs_since_newline) {
                    paragraph_break = true;
                }

                have_newline = true;
                only_spaces_tabs_since_newline = true;

                // Consume CRLF as one newline.
                if (ch == '\r' &&
                    i + 1 < text.size() &&
                    text[i + 1] == '\n') {
                    i += 2;
                } else {
                    ++i;
                }
            } else {
                // Spaces and tabs may appear between the two
                // newlines of a blank line.
                if (ch != ' ' && ch != '\t') {
                    have_newline = false;
                    only_spaces_tabs_since_newline = true;
                }

                ++i;
            }
        }

        if (i >= text.size()) {
            break;
        }

        // Do not count blank lines before the first real paragraph.
        if (paragraph_break && !tokens.empty()) {
            ++paragraph;
        }

        std::size_t begin = i;
        std::string token;

        while (i < text.size()) {
            unsigned char ch =
                static_cast<unsigned char>(text[i]);

            if (!is_ascii_alnum(ch)) {
                break;
            }

            if (ch >= 'A' && ch <= 'Z') {
                token += static_cast<char>(
                    ch - 'A' + 'a');
            } else {
                token += static_cast<char>(ch);
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