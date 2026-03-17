/**
 * @file TemplateEngine.h
 * @brief Template engine for rendering content templates with Mustache/Handlebars-like syntax
 *
 * PURPOSE:
 * Provides a powerful template rendering engine for generating dynamic content
 * for social media posts, emails, SMS, and other content types. Supports
 * variable interpolation, conditionals, loops, and custom helpers.
 *
 * USAGE:
 * auto& engine = TemplateEngine::getInstance();
 * engine.registerHelper("formatWaitTime", [](const Json::Value& args) { ... });
 *
 * TemplateContext ctx;
 * ctx.set("dog_name", "Max");
 * ctx.setWaitTime({2, 3, 15, 8, 45, 32});
 *
 * std::string result = engine.render("tiktok/urgent_countdown", ctx);
 *
 * SYNTAX SUPPORTED:
 * - {{variable}} - Variable interpolation
 * - {{#if condition}}...{{/if}} - Conditionals
 * - {{#unless condition}}...{{/unless}} - Inverse conditionals
 * - {{#each array}}...{{/each}} - Array iteration (use {{.}} for current item)
 * - {{#with object}}...{{/with}} - Context scoping
 * - {{helper arg1 arg2}} - Helper function calls
 * - {{! comment }} - Comments (not rendered)
 * - {{{unescaped}}} - Unescaped output (no HTML encoding)
 *
 * DEPENDENCIES:
 * - jsoncpp (JSON handling)
 * - ErrorCapture (error logging)
 * - SelfHealing (retry logic)
 *
 * MODIFICATION GUIDE:
 * - Add new block helpers in registerBlockHelper()
 * - Extend syntax in parseToken() and processToken()
 * - Custom template loaders can be added via setTemplateLoader()
 *
 * @author Agent 16 - WaitingTheLongest Build System
 * @date 2024-01-28
 */

#pragma once

// Standard library includes
#include <atomic>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <regex>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <variant>
#include <vector>

// Third-party includes
#include <json/json.h>

namespace wtl::content::templates {

// Forward declarations
class TemplateContext;

// ============================================================================
// TYPE DEFINITIONS
// ============================================================================

/**
 * @brief Helper function signature
 * Takes arguments as Json::Value and returns rendered string
 */
using HelperFunction = std::function<std::string(const Json::Value& args, const Json::Value& context)>;

/**
 * @brief Block helper function signature
 * Takes inner content, arguments, and context; returns rendered string
 */
using BlockHelperFunction = std::function<std::string(
    const std::string& inner_content,
    const Json::Value& args,
    const Json::Value& context,
    std::function<std::string(const Json::Value&)> render_fn
)>;

/**
 * @brief Template loader function signature
 * Takes template name, returns template content
 */
using TemplateLoader = std::function<std::optional<std::string>(const std::string& name)>;

// ============================================================================
// TOKEN TYPES
// ============================================================================

/**
 * @enum TokenType
 * @brief Types of tokens parsed from templates
 */
enum class TokenType {
    TEXT,           ///< Plain text (no processing)
    VARIABLE,       ///< {{variable}}
    VARIABLE_RAW,   ///< {{{variable}}} - unescaped
    HELPER,         ///< {{helper arg1 arg2}}
    BLOCK_START,    ///< {{#block}}
    BLOCK_END,      ///< {{/block}}
    INVERSE,        ///< {{^block}} or {{else}}
    COMMENT,        ///< {{! comment }}
    PARTIAL         ///< {{> partial}}
};

/**
 * @struct Token
 * @brief Represents a parsed token from a template
 */
struct Token {
    TokenType type;
    std::string content;           // For TEXT: the text; for others: the tag name
    std::vector<std::string> args; // Arguments for helpers/blocks
    int line_number;               // For error reporting
    int column;                    // For error reporting
};

// ============================================================================
// COMPILED TEMPLATE
// ============================================================================

/**
 * @struct CompiledTemplate
 * @brief Pre-compiled template for faster rendering
 */
struct CompiledTemplate {
    std::string name;
    std::string source;
    std::vector<Token> tokens;
    std::chrono::system_clock::time_point compiled_at;
    bool is_valid;
    std::string error_message;
};

// ============================================================================
// TEMPLATE ENGINE CLASS
// ============================================================================

/**
 * @class TemplateEngine
 * @brief Singleton template rendering engine
 *
 * Thread-safe template engine that supports Mustache/Handlebars-like syntax
 * with variable interpolation, conditionals, loops, and custom helpers.
 */
class TemplateEngine {
public:
    // =========================================================================
    // SINGLETON ACCESS
    // =========================================================================

    /**
     * @brief Get the singleton instance
     * @return Reference to the TemplateEngine singleton
     */
    static TemplateEngine& getInstance();

    // Delete copy/move constructors
    TemplateEngine(const TemplateEngine&) = delete;
    TemplateEngine& operator=(const TemplateEngine&) = delete;
    TemplateEngine(TemplateEngine&&) = delete;
    TemplateEngine& operator=(TemplateEngine&&) = delete;

    // =========================================================================
    // INITIALIZATION
    // =========================================================================

    /**
     * @brief Initialize the template engine
     * @param templates_dir Base directory for template files
     * @param cache_compiled Whether to cache compiled templates
     */
    void initialize(const std::string& templates_dir = "content_templates",
                   bool cache_compiled = true);

    /**
     * @brief Initialize from configuration
     */
    void initializeFromConfig();

    // =========================================================================
    // RENDERING
    // =========================================================================

    /**
     * @brief Render a template by name with context
     *
     * @param template_name The template name (e.g., "tiktok/urgent_countdown")
     * @param context The template context with variables
     * @return Rendered string
     * @throws std::runtime_error if template not found or rendering fails
     *
     * @example
     * TemplateContext ctx;
     * ctx.set("dog_name", "Max");
     * std::string result = engine.render("tiktok/urgent_countdown", ctx);
     */
    std::string render(const std::string& template_name, const TemplateContext& context);

    /**
     * @brief Render a template by name with JSON context
     *
     * @param template_name The template name
     * @param context JSON object with variables
     * @return Rendered string
     */
    std::string render(const std::string& template_name, const Json::Value& context);

    /**
     * @brief Render a template string directly
     *
     * @param template_content The template content string
     * @param context The template context
     * @return Rendered string
     */
    std::string renderString(const std::string& template_content, const TemplateContext& context);

    /**
     * @brief Render a template string with JSON context
     *
     * @param template_content The template content string
     * @param context JSON object with variables
     * @return Rendered string
     */
    std::string renderString(const std::string& template_content, const Json::Value& context);

    // =========================================================================
    // COMPILATION
    // =========================================================================

    /**
     * @brief Compile a template string for faster rendering
     *
     * @param template_content The template content
     * @param name Optional name for the compiled template
     * @return Compiled template object
     */
    CompiledTemplate compile(const std::string& template_content,
                            const std::string& name = "");

    /**
     * @brief Render a pre-compiled template
     *
     * @param compiled The compiled template
     * @param context The template context
     * @return Rendered string
     */
    std::string renderCompiled(const CompiledTemplate& compiled, const Json::Value& context);

    /**
     * @brief Pre-compile and cache a template by name
     *
     * @param template_name The template name
     * @return true if compilation successful
     */
    bool precompile(const std::string& template_name);

    /**
     * @brief Pre-compile all templates in a directory
     *
     * @param directory Directory to scan
     * @return Number of templates compiled
     */
    int precompileAll(const std::string& directory = "");

    // =========================================================================
    // TEMPLATE LOADING
    // =========================================================================

    /**
     * @brief Load a template by name
     *
     * @param name Template name (e.g., "tiktok/urgent_countdown")
     * @return Template content if found
     */
    std::optional<std::string> loadTemplate(const std::string& name);

    /**
     * @brief Set a custom template loader
     *
     * @param loader The loader function
     */
    void setTemplateLoader(TemplateLoader loader);

    /**
     * @brief Add a template directory to search path
     *
     * @param directory Directory path
     */
    void addTemplateDirectory(const std::string& directory);

    /**
     * @brief Clear the template cache
     */
    void clearCache();

    /**
     * @brief Reload a specific template
     *
     * @param name Template name
     * @return true if reloaded successfully
     */
    bool reloadTemplate(const std::string& name);

    // =========================================================================
    // HELPERS
    // =========================================================================

    /**
     * @brief Register a simple helper function
     *
     * @param name Helper name (used in templates as {{name arg1 arg2}})
     * @param helper The helper function
     *
     * @example
     * engine.registerHelper("uppercase", [](const Json::Value& args, const Json::Value& ctx) {
     *     return toUpperCase(args[0].asString());
     * });
     */
    void registerHelper(const std::string& name, HelperFunction helper);

    /**
     * @brief Register a block helper function
     *
     * @param name Block helper name (used as {{#name}}...{{/name}})
     * @param helper The block helper function
     *
     * @example
     * engine.registerBlockHelper("repeat", [](const std::string& inner,
     *                                         const Json::Value& args,
     *                                         const Json::Value& ctx,
     *                                         auto render) {
     *     std::string result;
     *     int count = args[0].asInt();
     *     for (int i = 0; i < count; ++i) {
     *         result += render(ctx);
     *     }
     *     return result;
     * });
     */
    void registerBlockHelper(const std::string& name, BlockHelperFunction helper);

    /**
     * @brief Check if a helper is registered
     *
     * @param name Helper name
     * @return true if registered
     */
    bool hasHelper(const std::string& name) const;

    /**
     * @brief Get list of registered helper names
     *
     * @return Vector of helper names
     */
    std::vector<std::string> getHelperNames() const;

    // =========================================================================
    // PARTIALS
    // =========================================================================

    /**
     * @brief Register a partial template
     *
     * @param name Partial name (used as {{> name}})
     * @param content Partial template content
     */
    void registerPartial(const std::string& name, const std::string& content);

    /**
     * @brief Check if a partial is registered
     *
     * @param name Partial name
     * @return true if registered
     */
    bool hasPartial(const std::string& name) const;

    // =========================================================================
    // VALIDATION
    // =========================================================================

    /**
     * @brief Validate a template string
     *
     * @param template_content The template content
     * @return Pair of (is_valid, error_message)
     */
    std::pair<bool, std::string> validate(const std::string& template_content);

    /**
     * @brief Get required variables from a template
     *
     * @param template_content The template content
     * @return Vector of variable names
     */
    std::vector<std::string> getRequiredVariables(const std::string& template_content);

    // =========================================================================
    // STATISTICS
    // =========================================================================

    /**
     * @brief Get engine statistics
     *
     * @return JSON with render counts, cache stats, etc.
     */
    Json::Value getStats() const;

private:
    // =========================================================================
    // PRIVATE MEMBERS
    // =========================================================================

    TemplateEngine() = default;
    ~TemplateEngine() = default;

    /**
     * @brief Tokenize a template string
     * @param content Template content
     * @return Vector of tokens
     */
    std::vector<Token> tokenize(const std::string& content);

    /**
     * @brief Parse a single token from content
     * @param content Content starting at token
     * @param pos Current position (updated)
     * @param line Current line number
     * @param col Current column
     * @return Parsed token
     */
    Token parseToken(const std::string& content, size_t& pos, int& line, int& col);

    /**
     * @brief Render tokens with context
     * @param tokens Vector of tokens
     * @param context JSON context
     * @return Rendered string
     */
    std::string renderTokens(const std::vector<Token>& tokens, const Json::Value& context);

    /**
     * @brief Process a single token
     * @param token The token to process
     * @param tokens_iter Iterator to current position in tokens
     * @param tokens_end End iterator
     * @param context JSON context
     * @return Rendered string for this token
     */
    std::string processToken(
        const Token& token,
        std::vector<Token>::const_iterator& tokens_iter,
        std::vector<Token>::const_iterator tokens_end,
        const Json::Value& context);

    /**
     * @brief Resolve a variable path in context
     * @param path Variable path (e.g., "dog.name" or ".")
     * @param context JSON context
     * @return Resolved value
     */
    Json::Value resolveVariable(const std::string& path, const Json::Value& context);

    /**
     * @brief Parse helper arguments
     * @param args_str Arguments string
     * @return Parsed arguments as JSON array
     */
    Json::Value parseHelperArgs(const std::string& args_str, const Json::Value& context);

    /**
     * @brief HTML escape a string
     * @param str Input string
     * @return Escaped string
     */
    std::string htmlEscape(const std::string& str);

    /**
     * @brief Convert JSON value to string
     * @param value JSON value
     * @return String representation
     */
    std::string jsonToString(const Json::Value& value);

    /**
     * @brief Check if value is truthy for conditionals
     * @param value JSON value
     * @return true if truthy
     */
    bool isTruthy(const Json::Value& value);

    /**
     * @brief Load template from file
     * @param name Template name
     * @return Content if found
     */
    std::optional<std::string> loadFromFile(const std::string& name);

    /**
     * @brief Register built-in helpers
     */
    void registerBuiltinHelpers();

    /**
     * @brief Register built-in block helpers
     */
    void registerBuiltinBlockHelpers();

    // Configuration
    std::string templates_dir_{"content_templates"};
    std::vector<std::string> template_directories_;
    bool cache_compiled_{true};

    // Template storage
    std::unordered_map<std::string, std::string> template_cache_;
    std::unordered_map<std::string, CompiledTemplate> compiled_cache_;

    // Helpers
    std::unordered_map<std::string, HelperFunction> helpers_;
    std::unordered_map<std::string, BlockHelperFunction> block_helpers_;

    // Partials
    std::unordered_map<std::string, std::string> partials_;

    // Custom loader
    TemplateLoader custom_loader_;

    // Statistics
    mutable std::atomic<uint64_t> render_count_{0};
    mutable std::atomic<uint64_t> cache_hits_{0};
    mutable std::atomic<uint64_t> cache_misses_{0};

    // Thread safety
    mutable std::shared_mutex mutex_;
    mutable std::mutex helpers_mutex_;

    // Initialized flag
    bool initialized_{false};
};

} // namespace wtl::content::templates
