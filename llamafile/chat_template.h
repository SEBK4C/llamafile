// -*- mode:c++;indent-tabs-mode:nil;c-basic-offset:4;coding:utf-8 -*-
// vi: set et ft=cpp ts=4 sts=4 sw=4 fenc=utf-8 :vi
//
// Copyright 2024 Mozilla Foundation
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#pragma once

#include <string>
#include <vector>
#include "llama.cpp/include/llama.h"

namespace lf {
namespace chat {

// Maximum safe size for chat template rendering (1MB)
// This prevents crashes from extremely large prompts
constexpr size_t MAX_CHAT_TEMPLATE_SIZE = 1024 * 1024;

// Initial buffer size for chat template rendering
constexpr size_t INITIAL_CHAT_BUFFER_SIZE = 16384;

// Result of safe chat template rendering
struct ChatTemplateResult {
    std::string content;
    bool success;
    std::string error_message;

    ChatTemplateResult() : success(false) {}
    ChatTemplateResult(std::string&& str) : content(std::move(str)), success(true) {}
    ChatTemplateResult(const std::string& err, bool) : success(false), error_message(err) {}
};

/**
 * Safely render a chat template with bounds checking.
 *
 * Inspired by OpenAI Harmony's approach to safe prompt rendering,
 * this function provides:
 * - Automatic buffer sizing with safety limits
 * - Protection against excessive memory allocation
 * - Clear error reporting
 *
 * @param tmpl Custom chat template (nullptr for model default)
 * @param messages Array of chat messages
 * @param n_messages Number of messages
 * @param add_assistant Whether to add assistant prompt
 * @return ChatTemplateResult with rendered content or error
 */
ChatTemplateResult render_chat_template_safe(
    const char* tmpl,
    const llama_chat_message* messages,
    size_t n_messages,
    bool add_assistant
);

/**
 * Convenience wrapper for C++ vectors
 */
inline ChatTemplateResult render_chat_template_safe(
    const char* tmpl,
    const std::vector<llama_chat_message>& messages,
    bool add_assistant
) {
    return render_chat_template_safe(
        tmpl,
        messages.data(),
        messages.size(),
        add_assistant
    );
}

/**
 * Render chat template with model-specific template
 */
ChatTemplateResult render_chat_template_safe(
    llama_model* model,
    const llama_chat_message* messages,
    size_t n_messages,
    bool add_assistant
);

/**
 * Convenience wrapper for model-based rendering
 */
inline ChatTemplateResult render_chat_template_safe(
    llama_model* model,
    const std::vector<llama_chat_message>& messages,
    bool add_assistant
) {
    return render_chat_template_safe(
        model,
        messages.data(),
        messages.size(),
        add_assistant
    );
}

} // namespace chat
} // namespace lf
