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

#include "chat_template.h"
#include <cstdio>
#include <cstring>

namespace lf {
namespace chat {

ChatTemplateResult render_chat_template_safe(
    const char* tmpl,
    const llama_chat_message* messages,
    size_t n_messages,
    bool add_assistant
) {
    if (!messages || n_messages == 0) {
        return ChatTemplateResult("No messages provided", false);
    }

    // First pass: determine required size
    char initial_buffer[INITIAL_CHAT_BUFFER_SIZE];
    int32_t required_size = llama_chat_apply_template(
        tmpl,
        messages,
        n_messages,
        add_assistant,
        initial_buffer,
        sizeof(initial_buffer)
    );

    if (required_size < 0) {
        return ChatTemplateResult("Failed to apply chat template", false);
    }

    // Check if result fits in initial buffer
    if (required_size < static_cast<int32_t>(sizeof(initial_buffer))) {
        return ChatTemplateResult(std::string(initial_buffer, required_size));
    }

    // Check against maximum safe size to prevent crashes
    if (static_cast<size_t>(required_size) > MAX_CHAT_TEMPLATE_SIZE) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                 "Chat template too large: %d bytes (max: %zu bytes). "
                 "Consider reducing message history or message length.",
                 required_size, MAX_CHAT_TEMPLATE_SIZE);
        return ChatTemplateResult(error_msg, false);
    }

    // Allocate exact size needed
    std::string result;
    try {
        result.resize(required_size);
    } catch (const std::bad_alloc&) {
        return ChatTemplateResult("Failed to allocate memory for chat template", false);
    }

    // Second pass: render into allocated buffer
    int32_t actual_size = llama_chat_apply_template(
        tmpl,
        messages,
        n_messages,
        add_assistant,
        &result[0],
        required_size
    );

    if (actual_size < 0) {
        return ChatTemplateResult("Failed to render chat template", false);
    }

    // Verify size consistency
    if (actual_size != required_size) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg),
                 "Chat template size mismatch: expected %d, got %d",
                 required_size, actual_size);
        return ChatTemplateResult(error_msg, false);
    }

    return ChatTemplateResult(std::move(result));
}

ChatTemplateResult render_chat_template_safe(
    llama_model* model,
    const llama_chat_message* messages,
    size_t n_messages,
    bool add_assistant
) {
    if (!model) {
        return ChatTemplateResult("Model is null", false);
    }

    // Use model's chat template (pass nullptr to llama_chat_apply_template)
    return render_chat_template_safe(
        static_cast<const char*>(nullptr),
        messages,
        n_messages,
        add_assistant
    );
}

} // namespace chat
} // namespace lf
