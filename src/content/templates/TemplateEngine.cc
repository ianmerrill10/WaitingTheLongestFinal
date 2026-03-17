/**
 * @file TemplateEngine.cc
 * @brief Implementation of TemplateEngine
 * @see TemplateEngine.h for documentation
 */

#include "content/templates/TemplateEngine.h"
#include "content/templates/TemplateContext.h"
#include "core/debug/ErrorCapture.h"
#include "core/debug/SelfHealing.h"
#include "core/utils/Config.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <fstream>
#include <sstream>
#include <filesystem>

namespace wtl::content::templates {

using namespace ::wtl::core::debug;

// ============================================================================
// SINGLETON ACCESS
// ============================================================================

TemplateEngine& TemplateEngine::getInstance() {
    static TemplateEngine instance;
    return instance;
}

// ============================================================================
// INITIALIZATION
// ============================================================================

void TemplateEngine::initialize(const std::string& templates_dir, bool cache_compiled) {
    std::unique_lock lock(mutex_);

    templates_dir_ = templates_dir;
    cache_compiled_ = cache_compiled;

    // Add default template directory
    template_directories_.clear();
    template_directories_.push_back(templates_dir_);

    // Register built-in helpers
    registerBuiltinHelpers();
    registerBuiltinBlockHelpers();

    initialized_ = true;
}

void TemplateEngine::initializeFromConfig() {
    try {
        auto& config = wtl::core::utils::Config::getInstance();
        std::string templates_dir = config.getString("templates.directory", "content_templates");
        bool cache_compiled = config.getBool("templates.cache_compiled", true);
        initialize(templates_dir, cache_compiled);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::CONFIGURATION,
                         "Failed to initialize TemplateEngine from config: " + std::string(e.what()),
                         {});
        // Use defaults
        initialize();
    }
}

// ============================================================================
// RENDERING
// ============================================================================

std::string TemplateEngine::render(const std::string& template_name, const TemplateContext& context) {
    return render(template_name, context.toJson());
}

std::string TemplateEngine::render(const std::string& template_name, const Json::Value& context) {
    render_count_++;

    // Check compiled cache first
    {
        std::shared_lock lock(mutex_);
        auto it = compiled_cache_.find(template_name);
        if (it != compiled_cache_.end() && it->second.is_valid) {
            cache_hits_++;
            return renderCompiled(it->second, context);
        }
    }

    cache_misses_++;

    // Load template
    auto content = loadTemplate(template_name);
    if (!content) {
        WTL_CAPTURE_ERROR(ErrorCategory::FILE_IO,
                         "Template not found: " + template_name,
                         {{"template_name", template_name}});
        throw std::runtime_error("Template not found: " + template_name);
    }

    // Compile and cache
    auto compiled = compile(*content, template_name);
    if (!compiled.is_valid) {
        WTL_CAPTURE_ERROR(ErrorCategory::VALIDATION,
                         "Template compilation failed: " + compiled.error_message,
                         {{"template_name", template_name}});
        throw std::runtime_error("Template compilation failed: " + compiled.error_message);
    }

    if (cache_compiled_) {
        std::unique_lock lock(mutex_);
        compiled_cache_[template_name] = compiled;
    }

    return renderCompiled(compiled, context);
}

std::string TemplateEngine::renderString(const std::string& template_content, const TemplateContext& context) {
    return renderString(template_content, context.toJson());
}

std::string TemplateEngine::renderString(const std::string& template_content, const Json::Value& context) {
    render_count_++;

    auto compiled = compile(template_content);
    if (!compiled.is_valid) {
        WTL_CAPTURE_ERROR(ErrorCategory::VALIDATION,
                         "Template compilation failed: " + compiled.error_message,
                         {});
        throw std::runtime_error("Template compilation failed: " + compiled.error_message);
    }

    return renderCompiled(compiled, context);
}

// ============================================================================
// COMPILATION
// ============================================================================

CompiledTemplate TemplateEngine::compile(const std::string& template_content, const std::string& name) {
    CompiledTemplate result;
    result.name = name;
    result.source = template_content;
    result.compiled_at = std::chrono::system_clock::now();

    try {
        result.tokens = tokenize(template_content);
        result.is_valid = true;
    } catch (const std::exception& e) {
        result.is_valid = false;
        result.error_message = e.what();
    }

    return result;
}

std::string TemplateEngine::renderCompiled(const CompiledTemplate& compiled, const Json::Value& context) {
    if (!compiled.is_valid) {
        throw std::runtime_error("Cannot render invalid template: " + compiled.error_message);
    }

    try {
        return renderTokens(compiled.tokens, context);
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::BUSINESS_LOGIC,
                         "Template rendering failed: " + std::string(e.what()),
                         {{"template_name", compiled.name}});
        throw;
    }
}

bool TemplateEngine::precompile(const std::string& template_name) {
    auto content = loadTemplate(template_name);
    if (!content) {
        return false;
    }

    auto compiled = compile(*content, template_name);
    if (!compiled.is_valid) {
        return false;
    }

    std::unique_lock lock(mutex_);
    compiled_cache_[template_name] = compiled;
    return true;
}

int TemplateEngine::precompileAll(const std::string& directory) {
    int count = 0;
    std::string search_dir = directory.empty() ? templates_dir_ : directory;

    try {
        for (const auto& entry : std::filesystem::recursive_directory_iterator(search_dir)) {
            if (entry.is_regular_file()) {
                std::string ext = entry.path().extension().string();
                if (ext == ".json" || ext == ".html" || ext == ".txt") {
                    std::string rel_path = std::filesystem::relative(entry.path(), search_dir).string();
                    // Normalize path separators
                    std::replace(rel_path.begin(), rel_path.end(), '\\', '/');
                    // Remove extension for template name
                    size_t dot_pos = rel_path.rfind('.');
                    if (dot_pos != std::string::npos) {
                        rel_path = rel_path.substr(0, dot_pos);
                    }
                    if (precompile(rel_path)) {
                        count++;
                    }
                }
            }
        }
    } catch (const std::exception& e) {
        WTL_CAPTURE_ERROR(ErrorCategory::FILE_IO,
                         "Error scanning template directory: " + std::string(e.what()),
                         {{"directory", search_dir}});
    }

    return count;
}

// ============================================================================
// TEMPLATE LOADING
// ============================================================================

std::optional<std::string> TemplateEngine::loadTemplate(const std::string& name) {
    // Check cache first
    {
        std::shared_lock lock(mutex_);
        auto it = template_cache_.find(name);
        if (it != template_cache_.end()) {
            return it->second;
        }
    }

    // Try custom loader
    if (custom_loader_) {
        auto content = custom_loader_(name);
        if (content) {
            std::unique_lock lock(mutex_);
            template_cache_[name] = *content;
            return content;
        }
    }

    // Try file system
    return loadFromFile(name);
}

void TemplateEngine::setTemplateLoader(TemplateLoader loader) {
    std::unique_lock lock(mutex_);
    custom_loader_ = std::move(loader);
}

void TemplateEngine::addTemplateDirectory(const std::string& directory) {
    std::unique_lock lock(mutex_);
    template_directories_.push_back(directory);
}

void TemplateEngine::clearCache() {
    std::unique_lock lock(mutex_);
    template_cache_.clear();
    compiled_cache_.clear();
}

bool TemplateEngine::reloadTemplate(const std::string& name) {
    {
        std::unique_lock lock(mutex_);
        template_cache_.erase(name);
        compiled_cache_.erase(name);
    }

    return precompile(name);
}

// ============================================================================
// HELPERS
// ============================================================================

void TemplateEngine::registerHelper(const std::string& name, HelperFunction helper) {
    std::lock_guard lock(helpers_mutex_);
    helpers_[name] = std::move(helper);
}

void TemplateEngine::registerBlockHelper(const std::string& name, BlockHelperFunction helper) {
    std::lock_guard lock(helpers_mutex_);
    block_helpers_[name] = std::move(helper);
}

bool TemplateEngine::hasHelper(const std::string& name) const {
    std::lock_guard lock(helpers_mutex_);
    return helpers_.find(name) != helpers_.end() ||
           block_helpers_.find(name) != block_helpers_.end();
}

std::vector<std::string> TemplateEngine::getHelperNames() const {
    std::lock_guard lock(helpers_mutex_);
    std::vector<std::string> names;
    names.reserve(helpers_.size() + block_helpers_.size());

    for (const auto& [name, _] : helpers_) {
        names.push_back(name);
    }
    for (const auto& [name, _] : block_helpers_) {
        names.push_back("#" + name);
    }

    return names;
}

// ============================================================================
// PARTIALS
// ============================================================================

void TemplateEngine::registerPartial(const std::string& name, const std::string& content) {
    std::unique_lock lock(mutex_);
    partials_[name] = content;
}

bool TemplateEngine::hasPartial(const std::string& name) const {
    std::shared_lock lock(mutex_);
    return partials_.find(name) != partials_.end();
}

// ============================================================================
// VALIDATION
// ============================================================================

std::pair<bool, std::string> TemplateEngine::validate(const std::string& template_content) {
    try {
        auto tokens = tokenize(template_content);

        // Check for balanced blocks
        std::vector<std::string> block_stack;
        for (const auto& token : tokens) {
            if (token.type == TokenType::BLOCK_START) {
                block_stack.push_back(token.content);
            } else if (token.type == TokenType::BLOCK_END) {
                if (block_stack.empty()) {
                    return {false, "Unexpected closing block: {{/" + token.content + "}}"};
                }
                if (block_stack.back() != token.content) {
                    return {false, "Mismatched block: expected {{/" + block_stack.back() +
                                   "}} but found {{/" + token.content + "}}"};
                }
                block_stack.pop_back();
            }
        }

        if (!block_stack.empty()) {
            return {false, "Unclosed block: {{#" + block_stack.back() + "}}"};
        }

        return {true, ""};
    } catch (const std::exception& e) {
        return {false, e.what()};
    }
}

std::vector<std::string> TemplateEngine::getRequiredVariables(const std::string& template_content) {
    std::vector<std::string> variables;
    std::unordered_map<std::string, bool> seen;

    try {
        auto tokens = tokenize(template_content);

        for (const auto& token : tokens) {
            if (token.type == TokenType::VARIABLE || token.type == TokenType::VARIABLE_RAW) {
                if (token.content != "." && token.content != "this" &&
                    seen.find(token.content) == seen.end()) {
                    variables.push_back(token.content);
                    seen[token.content] = true;
                }
            }
        }
    } catch (...) {
        // Ignore parsing errors for variable extraction
    }

    return variables;
}

// ============================================================================
// STATISTICS
// ============================================================================

Json::Value TemplateEngine::getStats() const {
    Json::Value stats;
    stats["render_count"] = static_cast<Json::UInt64>(render_count_.load());
    stats["cache_hits"] = static_cast<Json::UInt64>(cache_hits_.load());
    stats["cache_misses"] = static_cast<Json::UInt64>(cache_misses_.load());

    {
        std::shared_lock lock(mutex_);
        stats["cached_templates"] = static_cast<Json::UInt>(template_cache_.size());
        stats["compiled_templates"] = static_cast<Json::UInt>(compiled_cache_.size());
    }

    {
        std::lock_guard lock(helpers_mutex_);
        stats["registered_helpers"] = static_cast<Json::UInt>(helpers_.size());
        stats["registered_block_helpers"] = static_cast<Json::UInt>(block_helpers_.size());
    }

    if (render_count_ > 0) {
        double hit_rate = static_cast<double>(cache_hits_) / static_cast<double>(render_count_) * 100.0;
        stats["cache_hit_rate_percent"] = hit_rate;
    }

    return stats;
}

// ============================================================================
// PRIVATE METHODS - TOKENIZATION
// ============================================================================

std::vector<Token> TemplateEngine::tokenize(const std::string& content) {
    std::vector<Token> tokens;
    size_t pos = 0;
    int line = 1;
    int col = 1;
    size_t len = content.length();

    while (pos < len) {
        // Check for tag start
        if (pos + 1 < len && content[pos] == '{' && content[pos + 1] == '{') {
            // Check for triple mustache (unescaped)
            bool is_raw = (pos + 2 < len && content[pos + 2] == '{');
            size_t tag_start = pos;
            int start_line = line;
            int start_col = col;

            // Find closing
            std::string closer = is_raw ? "}}}" : "}}";
            size_t tag_end = content.find(closer, pos + (is_raw ? 3 : 2));

            if (tag_end == std::string::npos) {
                throw std::runtime_error("Unclosed tag starting at line " + std::to_string(line) +
                                        ", column " + std::to_string(col));
            }

            // Extract tag content
            size_t inner_start = pos + (is_raw ? 3 : 2);
            std::string tag_content = content.substr(inner_start, tag_end - inner_start);

            // Trim whitespace
            size_t first = tag_content.find_first_not_of(" \t\n\r");
            size_t last = tag_content.find_last_not_of(" \t\n\r");
            if (first != std::string::npos) {
                tag_content = tag_content.substr(first, last - first + 1);
            }

            // Determine token type
            Token token;
            token.line_number = start_line;
            token.column = start_col;

            if (tag_content.empty()) {
                throw std::runtime_error("Empty tag at line " + std::to_string(line));
            }

            char first_char = tag_content[0];

            if (first_char == '!') {
                // Comment
                token.type = TokenType::COMMENT;
                token.content = tag_content.substr(1);
            } else if (first_char == '#') {
                // Block start
                token.type = TokenType::BLOCK_START;
                std::string rest = tag_content.substr(1);
                // Parse block name and arguments
                std::istringstream iss(rest);
                iss >> token.content;
                std::string arg;
                while (iss >> arg) {
                    token.args.push_back(arg);
                }
            } else if (first_char == '/') {
                // Block end
                token.type = TokenType::BLOCK_END;
                token.content = tag_content.substr(1);
                // Trim
                size_t f = token.content.find_first_not_of(" \t");
                if (f != std::string::npos) {
                    token.content = token.content.substr(f);
                }
            } else if (first_char == '^') {
                // Inverse block
                token.type = TokenType::INVERSE;
                token.content = tag_content.substr(1);
            } else if (first_char == '>') {
                // Partial
                token.type = TokenType::PARTIAL;
                token.content = tag_content.substr(1);
                // Trim
                size_t f = token.content.find_first_not_of(" \t");
                if (f != std::string::npos) {
                    token.content = token.content.substr(f);
                }
            } else if (tag_content == "else") {
                // Else clause
                token.type = TokenType::INVERSE;
                token.content = "else";
            } else {
                // Variable or helper
                if (is_raw) {
                    token.type = TokenType::VARIABLE_RAW;
                } else {
                    // Check if it's a helper call (has spaces = arguments)
                    size_t space_pos = tag_content.find(' ');
                    if (space_pos != std::string::npos) {
                        // Could be a helper
                        std::string potential_name = tag_content.substr(0, space_pos);
                        std::lock_guard lock(helpers_mutex_);
                        if (helpers_.find(potential_name) != helpers_.end()) {
                            token.type = TokenType::HELPER;
                            token.content = potential_name;
                            std::string args_str = tag_content.substr(space_pos + 1);
                            std::istringstream iss(args_str);
                            std::string arg;
                            while (iss >> arg) {
                                token.args.push_back(arg);
                            }
                        } else {
                            token.type = TokenType::VARIABLE;
                            token.content = tag_content;
                        }
                    } else {
                        token.type = TokenType::VARIABLE;
                        token.content = tag_content;
                    }
                }

                if (token.type == TokenType::VARIABLE || token.type == TokenType::VARIABLE_RAW) {
                    token.content = tag_content;
                }
            }

            tokens.push_back(token);

            // Move position past the tag
            pos = tag_end + closer.length();

            // Update line/col
            for (size_t i = tag_start; i < pos && i < len; ++i) {
                if (content[i] == '\n') {
                    line++;
                    col = 1;
                } else {
                    col++;
                }
            }
        } else {
            // Plain text - find next tag or end
            size_t next_tag = content.find("{{", pos);
            if (next_tag == std::string::npos) {
                next_tag = len;
            }

            if (next_tag > pos) {
                Token token;
                token.type = TokenType::TEXT;
                token.content = content.substr(pos, next_tag - pos);
                token.line_number = line;
                token.column = col;
                tokens.push_back(token);

                // Update line/col
                for (size_t i = pos; i < next_tag; ++i) {
                    if (content[i] == '\n') {
                        line++;
                        col = 1;
                    } else {
                        col++;
                    }
                }
            }

            pos = next_tag;
        }
    }

    return tokens;
}

// ============================================================================
// PRIVATE METHODS - RENDERING
// ============================================================================

std::string TemplateEngine::renderTokens(const std::vector<Token>& tokens, const Json::Value& context) {
    std::string result;
    result.reserve(tokens.size() * 64); // Pre-allocate estimate

    auto it = tokens.begin();
    auto end = tokens.end();

    while (it != end) {
        result += processToken(*it, it, end, context);
        ++it;
    }

    return result;
}

std::string TemplateEngine::processToken(
    const Token& token,
    std::vector<Token>::const_iterator& tokens_iter,
    std::vector<Token>::const_iterator tokens_end,
    const Json::Value& context) {

    switch (token.type) {
        case TokenType::TEXT:
            return token.content;

        case TokenType::COMMENT:
            return "";

        case TokenType::VARIABLE: {
            Json::Value value = resolveVariable(token.content, context);
            return htmlEscape(jsonToString(value));
        }

        case TokenType::VARIABLE_RAW: {
            Json::Value value = resolveVariable(token.content, context);
            return jsonToString(value);
        }

        case TokenType::HELPER: {
            std::lock_guard lock(helpers_mutex_);
            auto it = helpers_.find(token.content);
            if (it != helpers_.end()) {
                Json::Value args = parseHelperArgs(
                    [&]() {
                        std::string s;
                        for (const auto& arg : token.args) {
                            if (!s.empty()) s += " ";
                            s += arg;
                        }
                        return s;
                    }(),
                    context
                );
                return it->second(args, context);
            }
            return "{{" + token.content + "}}";
        }

        case TokenType::PARTIAL: {
            std::shared_lock lock(mutex_);
            auto it = partials_.find(token.content);
            if (it != partials_.end()) {
                // Recursively render partial
                auto compiled = compile(it->second, "partial:" + token.content);
                return renderCompiled(compiled, context);
            }
            return "";
        }

        case TokenType::BLOCK_START: {
            // Find matching block end
            std::vector<Token> block_content;
            std::vector<Token> else_content;
            int depth = 1;
            auto block_iter = tokens_iter;
            ++block_iter;

            bool in_else = false;

            while (block_iter != tokens_end && depth > 0) {
                if (block_iter->type == TokenType::BLOCK_START) {
                    depth++;
                    if (!in_else) {
                        block_content.push_back(*block_iter);
                    } else {
                        else_content.push_back(*block_iter);
                    }
                } else if (block_iter->type == TokenType::BLOCK_END) {
                    depth--;
                    if (depth > 0) {
                        if (!in_else) {
                            block_content.push_back(*block_iter);
                        } else {
                            else_content.push_back(*block_iter);
                        }
                    }
                } else if (block_iter->type == TokenType::INVERSE && depth == 1 &&
                          (block_iter->content == "else" || block_iter->content.empty())) {
                    in_else = true;
                } else {
                    if (!in_else) {
                        block_content.push_back(*block_iter);
                    } else {
                        else_content.push_back(*block_iter);
                    }
                }
                ++block_iter;
            }

            // Move iterator past block end
            tokens_iter = block_iter;
            --tokens_iter; // Will be incremented by caller

            // Handle built-in block types
            const std::string& block_name = token.content;

            if (block_name == "if") {
                // Conditional
                Json::Value condition = token.args.empty() ?
                    Json::Value(false) :
                    resolveVariable(token.args[0], context);

                if (isTruthy(condition)) {
                    return renderTokens(block_content, context);
                } else {
                    return renderTokens(else_content, context);
                }
            } else if (block_name == "unless") {
                // Inverse conditional
                Json::Value condition = token.args.empty() ?
                    Json::Value(false) :
                    resolveVariable(token.args[0], context);

                if (!isTruthy(condition)) {
                    return renderTokens(block_content, context);
                } else {
                    return renderTokens(else_content, context);
                }
            } else if (block_name == "each") {
                // Iteration
                if (token.args.empty()) {
                    return "";
                }

                Json::Value array = resolveVariable(token.args[0], context);
                if (!array.isArray() || array.empty()) {
                    return renderTokens(else_content, context);
                }

                std::string result;
                for (Json::ArrayIndex i = 0; i < array.size(); ++i) {
                    // Create loop context
                    Json::Value loop_context = context;
                    loop_context["@index"] = i;
                    loop_context["@first"] = (i == 0);
                    loop_context["@last"] = (i == array.size() - 1);
                    loop_context["."] = array[i];
                    loop_context["this"] = array[i];

                    // If array item is object, merge its properties
                    if (array[i].isObject()) {
                        for (const auto& key : array[i].getMemberNames()) {
                            loop_context[key] = array[i][key];
                        }
                    }

                    result += renderTokens(block_content, loop_context);
                }
                return result;
            } else if (block_name == "with") {
                // Context scoping
                if (token.args.empty()) {
                    return "";
                }

                Json::Value new_context = resolveVariable(token.args[0], context);
                if (!isTruthy(new_context)) {
                    return renderTokens(else_content, context);
                }

                // Merge new context with parent
                Json::Value merged = context;
                if (new_context.isObject()) {
                    for (const auto& key : new_context.getMemberNames()) {
                        merged[key] = new_context[key];
                    }
                }
                merged["."] = new_context;
                merged["this"] = new_context;

                return renderTokens(block_content, merged);
            } else {
                // Check for custom block helper
                std::lock_guard lock(helpers_mutex_);
                auto helper_it = block_helpers_.find(block_name);
                if (helper_it != block_helpers_.end()) {
                    // Build inner content string
                    std::string inner;
                    for (const auto& t : block_content) {
                        if (t.type == TokenType::TEXT) {
                            inner += t.content;
                        }
                    }

                    Json::Value args(Json::arrayValue);
                    for (const auto& arg : token.args) {
                        Json::Value resolved = resolveVariable(arg, context);
                        args.append(resolved);
                    }

                    auto render_fn = [this, &block_content](const Json::Value& ctx) {
                        return renderTokens(block_content, ctx);
                    };

                    return helper_it->second(inner, args, context, render_fn);
                }

                // Unknown block - just render content
                return renderTokens(block_content, context);
            }
        }

        case TokenType::BLOCK_END:
            // Should not reach here - handled in BLOCK_START
            return "";

        case TokenType::INVERSE:
            // Should not reach here - handled in BLOCK_START
            return "";

        default:
            return "";
    }
}

Json::Value TemplateEngine::resolveVariable(const std::string& path, const Json::Value& context) {
    if (path == "." || path == "this") {
        if (context.isMember(".")) {
            return context["."];
        }
        return context;
    }

    // Handle path traversal (e.g., "dog.name")
    Json::Value current = context;
    std::istringstream iss(path);
    std::string segment;

    while (std::getline(iss, segment, '.')) {
        if (current.isObject() && current.isMember(segment)) {
            current = current[segment];
        } else if (current.isArray()) {
            // Try to parse as index
            try {
                size_t idx = std::stoul(segment);
                if (idx < current.size()) {
                    current = current[static_cast<Json::ArrayIndex>(idx)];
                } else {
                    return Json::Value();
                }
            } catch (...) {
                return Json::Value();
            }
        } else {
            return Json::Value();
        }
    }

    return current;
}

Json::Value TemplateEngine::parseHelperArgs(const std::string& args_str, const Json::Value& context) {
    Json::Value args(Json::arrayValue);

    std::istringstream iss(args_str);
    std::string token;

    while (iss >> token) {
        // Check if it's a string literal
        if ((token.front() == '"' && token.back() == '"') ||
            (token.front() == '\'' && token.back() == '\'')) {
            args.append(token.substr(1, token.length() - 2));
        }
        // Check if it's a number
        else {
            try {
                if (token.find('.') != std::string::npos) {
                    args.append(std::stod(token));
                } else {
                    args.append(std::stoi(token));
                }
            } catch (...) {
                // It's a variable reference
                args.append(resolveVariable(token, context));
            }
        }
    }

    return args;
}

std::string TemplateEngine::htmlEscape(const std::string& str) {
    std::string result;
    result.reserve(str.length() * 1.1); // Small buffer for escapes

    for (char c : str) {
        switch (c) {
            case '&':  result += "&amp;"; break;
            case '<':  result += "&lt;"; break;
            case '>':  result += "&gt;"; break;
            case '"':  result += "&quot;"; break;
            case '\'': result += "&#39;"; break;
            default:   result += c; break;
        }
    }

    return result;
}

std::string TemplateEngine::jsonToString(const Json::Value& value) {
    if (value.isNull()) {
        return "";
    } else if (value.isString()) {
        return value.asString();
    } else if (value.isBool()) {
        return value.asBool() ? "true" : "false";
    } else if (value.isInt()) {
        return std::to_string(value.asInt());
    } else if (value.isInt64()) {
        return std::to_string(value.asInt64());
    } else if (value.isUInt()) {
        return std::to_string(value.asUInt());
    } else if (value.isUInt64()) {
        return std::to_string(value.asUInt64());
    } else if (value.isDouble()) {
        std::ostringstream oss;
        oss << value.asDouble();
        return oss.str();
    } else if (value.isArray()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, value);
    } else if (value.isObject()) {
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, value);
    }

    return "";
}

bool TemplateEngine::isTruthy(const Json::Value& value) {
    if (value.isNull()) {
        return false;
    } else if (value.isBool()) {
        return value.asBool();
    } else if (value.isString()) {
        return !value.asString().empty();
    } else if (value.isNumeric()) {
        return value.asDouble() != 0.0;
    } else if (value.isArray()) {
        return !value.empty();
    } else if (value.isObject()) {
        return !value.empty();
    }
    return true;
}

std::optional<std::string> TemplateEngine::loadFromFile(const std::string& name) {
    // Try different extensions
    std::vector<std::string> extensions = {".json", ".html", ".txt", ""};

    for (const auto& dir : template_directories_) {
        for (const auto& ext : extensions) {
            std::filesystem::path file_path = std::filesystem::path(dir) / (name + ext);

            if (std::filesystem::exists(file_path)) {
                std::ifstream file(file_path);
                if (file.is_open()) {
                    std::stringstream buffer;
                    buffer << file.rdbuf();
                    std::string content = buffer.str();

                    // If it's a JSON file, extract the template content
                    if (ext == ".json" || file_path.extension() == ".json") {
                        try {
                            Json::Value root;
                            Json::CharReaderBuilder builder;
                            std::istringstream iss(content);
                            std::string errors;
                            if (Json::parseFromStream(builder, iss, &root, &errors)) {
                                // Look for common template fields
                                if (root.isMember("caption")) {
                                    content = root["caption"].asString();
                                } else if (root.isMember("template")) {
                                    content = root["template"].asString();
                                } else if (root.isMember("content")) {
                                    content = root["content"].asString();
                                } else if (root.isMember("body")) {
                                    content = root["body"].asString();
                                }
                                // Otherwise use raw JSON
                            }
                        } catch (...) {
                            // Use raw content if JSON parsing fails
                        }
                    }

                    std::unique_lock lock(mutex_);
                    template_cache_[name] = content;
                    return content;
                }
            }
        }
    }

    return std::nullopt;
}

// ============================================================================
// PRIVATE METHODS - BUILT-IN HELPERS
// ============================================================================

void TemplateEngine::registerBuiltinHelpers() {
    // uppercase helper
    registerHelper("uppercase", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty() || !args[0].isString()) return "";
        std::string str = args[0].asString();
        std::transform(str.begin(), str.end(), str.begin(), ::toupper);
        return str;
    });

    // lowercase helper
    registerHelper("lowercase", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty() || !args[0].isString()) return "";
        std::string str = args[0].asString();
        std::transform(str.begin(), str.end(), str.begin(), ::tolower);
        return str;
    });

    // capitalize helper
    registerHelper("capitalize", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty() || !args[0].isString()) return "";
        std::string str = args[0].asString();
        if (!str.empty()) {
            str[0] = std::toupper(str[0]);
        }
        return str;
    });

    // truncate helper
    registerHelper("truncate", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.size() < 2) return args.empty() ? "" : args[0].asString();
        std::string str = args[0].asString();
        int length = args[1].asInt();
        if (static_cast<int>(str.length()) > length) {
            return str.substr(0, length - 3) + "...";
        }
        return str;
    });

    // default helper
    registerHelper("default", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty()) return "";
        if (args[0].isNull() || (args[0].isString() && args[0].asString().empty())) {
            return args.size() > 1 ? args[1].asString() : "";
        }
        return args[0].asString();
    });

    // eq helper (equality)
    registerHelper("eq", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.size() < 2) return "false";
        return (args[0] == args[1]) ? "true" : "false";
    });

    // ne helper (not equal)
    registerHelper("ne", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.size() < 2) return "false";
        return (args[0] != args[1]) ? "true" : "false";
    });

    // gt helper (greater than)
    registerHelper("gt", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.size() < 2) return "false";
        if (args[0].isNumeric() && args[1].isNumeric()) {
            return (args[0].asDouble() > args[1].asDouble()) ? "true" : "false";
        }
        return "false";
    });

    // lt helper (less than)
    registerHelper("lt", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.size() < 2) return "false";
        if (args[0].isNumeric() && args[1].isNumeric()) {
            return (args[0].asDouble() < args[1].asDouble()) ? "true" : "false";
        }
        return "false";
    });

    // join helper
    registerHelper("join", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty() || !args[0].isArray()) return "";
        std::string separator = args.size() > 1 ? args[1].asString() : ", ";
        std::string result;
        const Json::Value& arr = args[0];
        for (Json::ArrayIndex i = 0; i < arr.size(); ++i) {
            if (i > 0) result += separator;
            result += arr[i].asString();
        }
        return result;
    });

    // length helper
    registerHelper("length", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty()) return "0";
        if (args[0].isArray()) {
            return std::to_string(args[0].size());
        } else if (args[0].isString()) {
            return std::to_string(args[0].asString().length());
        }
        return "0";
    });

    // now helper (current timestamp)
    registerHelper("now", [](const Json::Value&, const Json::Value&) -> std::string {
        auto now = std::chrono::system_clock::now();
        auto time = std::chrono::system_clock::to_time_t(now);
        std::stringstream ss;
        ss << std::put_time(std::localtime(&time), "%Y-%m-%d %H:%M:%S");
        return ss.str();
    });

    // json helper (serialize to JSON)
    registerHelper("json", [](const Json::Value& args, const Json::Value&) -> std::string {
        if (args.empty()) return "null";
        Json::StreamWriterBuilder builder;
        builder["indentation"] = "";
        return Json::writeString(builder, args[0]);
    });
}

void TemplateEngine::registerBuiltinBlockHelpers() {
    // repeat block helper
    registerBlockHelper("repeat", [](const std::string&,
                                     const Json::Value& args,
                                     const Json::Value& context,
                                     std::function<std::string(const Json::Value&)> render) -> std::string {
        int count = args.empty() ? 0 : args[0].asInt();
        std::string result;
        for (int i = 0; i < count; ++i) {
            Json::Value loop_ctx = context;
            loop_ctx["@index"] = i;
            loop_ctx["@first"] = (i == 0);
            loop_ctx["@last"] = (i == count - 1);
            result += render(loop_ctx);
        }
        return result;
    });

    // log block helper (for debugging)
    registerBlockHelper("log", [](const std::string& inner,
                                  const Json::Value& args,
                                  const Json::Value&,
                                  std::function<std::string(const Json::Value&)>) -> std::string {
        // In production, this could log to the debug system
        // For now, just return empty (comment-like behavior)
        return "";
    });
}

} // namespace wtl::content::templates
