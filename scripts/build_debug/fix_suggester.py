"""
Build Debug Agent - Fix suggestion generator.

Generates human-readable fix suggestions for each error category.
Uses a project-specific knowledge base mapping symbols to headers
and known namespace conventions.
"""

import re
from .models import BuildError, BuildErrorCategory

# ==============================================================================
# PROJECT-SPECIFIC KNOWLEDGE BASE
# ==============================================================================

# Symbol -> header mapping for this project
SYMBOL_TO_HEADER: dict[str, str] = {
    # Standard library
    "std::string": "<string>",
    "std::vector": "<vector>",
    "std::array": "<array>",
    "std::map": "<map>",
    "std::unordered_map": "<unordered_map>",
    "std::set": "<set>",
    "std::unordered_set": "<unordered_set>",
    "std::optional": "<optional>",
    "std::variant": "<variant>",
    "std::any": "<any>",
    "std::shared_ptr": "<memory>",
    "std::unique_ptr": "<memory>",
    "std::make_shared": "<memory>",
    "std::make_unique": "<memory>",
    "std::mutex": "<mutex>",
    "std::shared_mutex": "<shared_mutex>",
    "std::lock_guard": "<mutex>",
    "std::unique_lock": "<mutex>",
    "std::shared_lock": "<shared_mutex>",
    "std::thread": "<thread>",
    "std::atomic": "<atomic>",
    "std::chrono": "<chrono>",
    "std::function": "<functional>",
    "std::bind": "<functional>",
    "std::filesystem": "<filesystem>",
    "std::regex": "<regex>",
    "std::deque": "<deque>",
    "std::queue": "<queue>",
    "std::pair": "<utility>",
    "std::tuple": "<tuple>",
    "std::move": "<utility>",
    "std::forward": "<utility>",
    "std::runtime_error": "<stdexcept>",
    "std::invalid_argument": "<stdexcept>",
    "std::out_of_range": "<stdexcept>",
    "std::exception": "<exception>",
    "std::cout": "<iostream>",
    "std::cerr": "<iostream>",
    "std::endl": "<iostream>",
    "std::stringstream": "<sstream>",
    "std::ostringstream": "<sstream>",
    "std::istringstream": "<sstream>",
    "std::to_string": "<string>",
    "std::stoi": "<string>",
    "std::stod": "<string>",
    "std::sort": "<algorithm>",
    "std::find": "<algorithm>",
    "std::transform": "<algorithm>",
    "std::accumulate": "<numeric>",
    "std::condition_variable": "<condition_variable>",

    # Third-party
    "Json::Value": "<json/json.h>",
    "Json::Reader": "<json/json.h>",
    "Json::StreamWriterBuilder": "<json/json.h>",
    "Json::CharReaderBuilder": "<json/json.h>",
    "drogon::HttpController": "<drogon/HttpController.h>",
    "drogon::HttpSimpleController": "<drogon/HttpSimpleController.h>",
    "drogon::HttpRequest": "<drogon/drogon.h>",
    "drogon::HttpResponse": "<drogon/drogon.h>",
    "drogon::WebSocketController": "<drogon/WebSocketController.h>",
    "drogon::app": "<drogon/drogon.h>",
    "drogon::HttpAppFramework": "<drogon/drogon.h>",
    "pqxx::work": "<pqxx/pqxx>",
    "pqxx::connection": "<pqxx/pqxx>",
    "pqxx::result": "<pqxx/pqxx>",
    "pqxx::nontransaction": "<pqxx/pqxx>",
    "trantor::Logger": "<trantor/utils/Logger.h>",

    # Project includes (from CODING_STANDARDS.md)
    "ErrorCapture": '"core/debug/ErrorCapture.h"',
    "SelfHealing": '"core/debug/SelfHealing.h"',
    "LogGenerator": '"core/debug/LogGenerator.h"',
    "HealthCheck": '"core/debug/HealthCheck.h"',
    "Config": '"core/utils/Config.h"',
    "ApiResponse": '"core/utils/ApiResponse.h"',
    "WaitTimeCalculator": '"core/utils/WaitTimeCalculator.h"',
    "ConnectionPool": '"core/db/ConnectionPool.h"',
    "EventBus": '"core/EventBus.h"',
    "EventType": '"core/EventType.h"',
    "Event": '"core/Event.h"',
    "Dog": '"core/models/Dog.h"',
    "Shelter": '"core/models/Shelter.h"',
    "State": '"core/models/State.h"',
    "User": '"core/models/User.h"',
    "FosterHome": '"core/models/FosterHome.h"',
    "FosterPlacement": '"core/models/FosterPlacement.h"',
    "DogService": '"core/services/DogService.h"',
    "ShelterService": '"core/services/ShelterService.h"',
    "StateService": '"core/services/StateService.h"',
    "UserService": '"core/services/UserService.h"',
    "SearchService": '"core/services/SearchService.h"',
    "FosterService": '"core/services/FosterService.h"',
    "UrgencyCalculator": '"core/services/UrgencyCalculator.h"',
    "KillShelterMonitor": '"core/services/KillShelterMonitor.h"',
    "EuthanasiaTracker": '"core/services/EuthanasiaTracker.h"',
    "AlertService": '"core/services/AlertService.h"',
    "AuthService": '"core/auth/AuthService.h"',
    "AuthMiddleware": '"core/auth/AuthMiddleware.h"',
    "JwtUtils": '"core/auth/JwtUtils.h"',
    "PasswordUtils": '"core/auth/PasswordUtils.h"',
    "SessionManager": '"core/auth/SessionManager.h"',
    "IModule": '"modules/IModule.h"',
    "ModuleManager": '"modules/ModuleManager.h"',
    "ModuleConfig": '"modules/ModuleConfig.h"',
    "BroadcastService": '"core/websocket/BroadcastService.h"',
    "ConnectionManager": '"core/websocket/ConnectionManager.h"',
    "WaitTimeWorker": '"core/websocket/WaitTimeWorker.h"',
    "TikTokEngine": '"content/tiktok/TikTokEngine.h"',
    "BlogEngine": '"content/blog/BlogEngine.h"',
    "SocialMediaEngine": '"content/social/SocialMediaEngine.h"',
    "VideoGenerator": '"content/media/VideoGenerator.h"',
    "ImageProcessor": '"content/media/ImageProcessor.h"',
    "TemplateEngine": '"content/templates/TemplateEngine.h"',
    "AnalyticsService": '"analytics/AnalyticsService.h"',
    "MetricsAggregator": '"analytics/MetricsAggregator.h"',
    "ImpactTracker": '"analytics/ImpactTracker.h"',
    "NotificationService": '"notifications/NotificationService.h"',
    "AggregatorManager": '"aggregators/AggregatorManager.h"',
    "WorkerManager": '"workers/WorkerManager.h"',
    "Scheduler": '"workers/scheduler/Scheduler.h"',
    "AdminService": '"admin/AdminService.h"',
    "HttpClient": '"clients/HttpClient.h"',
    "RateLimiter": '"clients/RateLimiter.h"',
}

# Common namespace mistakes in this project
NAMESPACE_FIXES: dict[str, str] = {
    "core::": "wtl::core::",
    "services::": "wtl::core::services::",
    "models::": "wtl::core::models::",
    "auth::": "wtl::core::auth::",
    "debug::": "wtl::core::debug::",
    "websocket::": "wtl::core::websocket::",
    "controllers::": "wtl::core::controllers::",
    "modules::": "wtl::modules::",
    "content::": "wtl::content::",
    "tiktok::": "wtl::content::tiktok::",
    "blog::": "wtl::content::blog::",
    "social::": "wtl::content::social::",
    "media::": "wtl::content::media::",
    "templates::": "wtl::content::templates::",
    "analytics::": "wtl::analytics::",
    "notifications::": "wtl::notifications::",
    "aggregators::": "wtl::aggregators::",
    "workers::": "wtl::workers::",
    "clients::": "wtl::clients::",
    "admin::": "wtl::admin::",
}

# Regex to extract identifiers from error messages
IDENTIFIER_RE = re.compile(r"['\"`]([A-Za-z_][\w:]*)['\"`]")
TYPE_CONVERSION_RE = re.compile(r"from ['\"`]([^'`\"]+)['\"`] to ['\"`]([^'`\"]+)['\"`]")
MEMBER_RE = re.compile(r"['\"`](?:class |struct )?(\w+)['\"`] has no member named ['\"`](\w+)['\"`]")


class FixSuggester:
    """Generates human-readable fix suggestions for each error category."""

    def suggest(self, error: BuildError) -> str:
        """Return a fix suggestion string for a single error."""
        handlers = {
            BuildErrorCategory.MISSING_HEADER: self._suggest_missing_header,
            BuildErrorCategory.UNDEFINED_REFERENCE: self._suggest_undefined_reference,
            BuildErrorCategory.TYPE_MISMATCH: self._suggest_type_mismatch,
            BuildErrorCategory.NAMESPACE_ERROR: self._suggest_namespace_error,
            BuildErrorCategory.SYNTAX_ERROR: self._suggest_syntax_error,
            BuildErrorCategory.MISSING_METHOD: self._suggest_missing_method,
            BuildErrorCategory.DUPLICATE_SYMBOL: self._suggest_duplicate_symbol,
            BuildErrorCategory.TEMPLATE_ERROR: self._suggest_template_error,
            BuildErrorCategory.ACCESS_ERROR: self._suggest_access_error,
            BuildErrorCategory.IMPLICIT_CONVERSION: self._suggest_implicit_conversion,
            BuildErrorCategory.CMAKE_ERROR: self._suggest_cmake_error,
            BuildErrorCategory.LINKER_ERROR: self._suggest_linker_error,
        }

        handler = handlers.get(error.category)
        if handler:
            return handler(error)
        return f"Unknown error category. Review {error.file}:{error.line} manually."

    def suggest_batch(self, errors: list[BuildError]) -> list[BuildError]:
        """Add suggestions to all errors, mutating in place."""
        for error in errors:
            error.suggestion = self.suggest(error)
        return errors

    def _suggest_missing_header(self, error: BuildError) -> str:
        # Extract the missing header name from the message
        match = re.search(r"['\"`]?([^\s'`\"]+\.h(?:pp)?)['\"`]?", error.message)
        if match:
            header = match.group(1)
            return (
                f"Add '#include \"{header}\"' to {error.file}.\n"
                f"If the file doesn't exist at that path, check the src/ directory structure.\n"
                f"The include path should be relative to src/ (e.g., 'core/services/DogService.h')."
            )

        # Try to find which symbol triggered the include
        for symbol, header in SYMBOL_TO_HEADER.items():
            if symbol.split("::")[-1] in error.message:
                return f"Add '#include {header}' to {error.file} for '{symbol}'."

        return f"Missing header file. Check include path in {error.file}:{error.line}."

    def _suggest_undefined_reference(self, error: BuildError) -> str:
        symbol = error.metadata.get("symbol", "unknown")

        # Check if it maps to a known source file
        for known_symbol, header in SYMBOL_TO_HEADER.items():
            simple_name = known_symbol.split("::")[-1]
            if simple_name in symbol:
                cc_file = header.strip('"').replace(".h", ".cc")
                return (
                    f"Undefined reference to '{symbol}'.\n"
                    f"Check that '{cc_file}' is listed in CMakeLists.txt.\n"
                    f"The implementation may be missing or the source file not included in the build."
                )

        return (
            f"Undefined reference to '{symbol}'.\n"
            f"Either the implementation (.cc file) is missing, or it's not in CMakeLists.txt.\n"
            f"Search for the declaration and ensure the matching .cc file exists and is compiled."
        )

    def _suggest_type_mismatch(self, error: BuildError) -> str:
        conv_match = TYPE_CONVERSION_RE.search(error.message)
        if conv_match:
            from_type = conv_match.group(1)
            to_type = conv_match.group(2)
            return (
                f"Type mismatch: cannot convert '{from_type}' to '{to_type}'.\n"
                f"Consider: static_cast<{to_type}>(expression)\n"
                f"Or check that the function signature in the .h matches the call site."
            )

        if "no matching function" in error.message:
            return (
                f"No matching function found at {error.file}:{error.line}.\n"
                f"Check argument types match the function declaration.\n"
                f"Common cause: 34 agents may have different assumptions about method signatures."
            )

        return f"Type mismatch at {error.file}:{error.line}. Check types match expected signatures."

    def _suggest_namespace_error(self, error: BuildError) -> str:
        # Extract the undeclared identifier
        identifiers = IDENTIFIER_RE.findall(error.message)

        for ident in identifiers:
            # Check known symbols
            if ident in SYMBOL_TO_HEADER:
                header = SYMBOL_TO_HEADER[ident]
                return (
                    f"'{ident}' not found. Add '#include {header}'.\n"
                    f"This project uses namespace hierarchy: wtl::core::*, wtl::modules::*, etc."
                )

            # Check if it's a namespace issue
            for short, full in NAMESPACE_FIXES.items():
                if ident.startswith(short.rstrip(":")):
                    return (
                        f"'{ident}' not declared. This project uses '{full}' namespace.\n"
                        f"Either qualify as '{full}{ident.split('::')[-1]}' or add "
                        f"'using namespace {full.rstrip('::')}' at the top."
                    )

        if "does not name a type" in error.message:
            return (
                f"Type not found at {error.file}:{error.line}.\n"
                f"Missing #include for the type, or wrong namespace.\n"
                f"Check CODING_STANDARDS.md for the correct namespace hierarchy."
            )

        return (
            f"Identifier not found at {error.file}:{error.line}.\n"
            f"Common causes: missing #include, wrong namespace, or typo.\n"
            f"This project uses wtl:: root namespace with sub-namespaces per module."
        )

    def _suggest_syntax_error(self, error: BuildError) -> str:
        if "expected ';'" in error.message:
            return (
                f"Missing semicolon near {error.file}:{error.line}.\n"
                f"Check the line above as well - the error is often on the preceding line.\n"
                f"Common cause: missing ';' after class/struct definition or closing brace."
            )

        if "expected '}'" in error.message or "expected '{'" in error.message:
            return (
                f"Unmatched braces near {error.file}:{error.line}.\n"
                f"Check for mismatched opening/closing braces in this scope."
            )

        return (
            f"Syntax error at {error.file}:{error.line}.\n"
            f"Check for missing punctuation (;, }}, )), unmatched brackets, or typos."
        )

    def _suggest_missing_method(self, error: BuildError) -> str:
        member_match = MEMBER_RE.search(error.message)
        if member_match:
            class_name = member_match.group(1)
            method_name = member_match.group(2)
            # Try to find the header
            header = SYMBOL_TO_HEADER.get(class_name, f"<{class_name}'s header>")
            return (
                f"Class '{class_name}' has no member '{method_name}'.\n"
                f"Check the header: {header}\n"
                f"Common cause: method declared with different name, or not declared at all.\n"
                f"This could be an agent consistency issue - verify .h matches .cc usage."
            )

        return (
            f"Member not found at {error.file}:{error.line}.\n"
            f"Check that the method is declared in the corresponding .h file."
        )

    def _suggest_duplicate_symbol(self, error: BuildError) -> str:
        symbol = error.metadata.get("symbol", "")
        return (
            f"Symbol '{symbol}' defined in multiple translation units.\n"
            f"Common causes:\n"
            f"  - Function defined in header without 'inline' keyword\n"
            f"  - Same .cc file listed twice in CMakeLists.txt\n"
            f"  - Global variable defined in header (use 'inline' or extern+definition)\n"
            f"Fix: Add 'inline' to functions defined in headers, or move to .cc file."
        )

    def _suggest_template_error(self, error: BuildError) -> str:
        context = error.metadata.get("template_context", "")
        return (
            f"Template instantiation error at {error.file}:{error.line}.\n"
            f"Check template argument types match constraints.\n"
            f"{'Template context:\\n' + context if context else ''}\n"
            f"These errors are complex - review the full chain manually."
        )

    def _suggest_access_error(self, error: BuildError) -> str:
        return (
            f"Accessing private/protected member at {error.file}:{error.line}.\n"
            f"Options:\n"
            f"  - Make the member public in the header\n"
            f"  - Add a public getter/setter method\n"
            f"  - Add a friend declaration\n"
            f"Check if this is an intentional encapsulation or an agent oversight."
        )

    def _suggest_implicit_conversion(self, error: BuildError) -> str:
        return (
            f"Implicit narrowing conversion at {error.file}:{error.line}.\n"
            f"Add explicit cast: static_cast<TargetType>(expression)\n"
            f"In C++17 brace-init, narrowing conversions are not allowed.\n"
            f"Common: int -> size_t, double -> int, int64_t -> int."
        )

    def _suggest_cmake_error(self, error: BuildError) -> str:
        command = error.metadata.get("command", "")
        if command == "find_package":
            return (
                f"CMake can't find package. Install the development library.\n"
                f"For Docker builds, add the package to the Dockerfile's apt-get install list."
            )
        return f"CMake configuration error. Check CMakeLists.txt:{error.line}."

    def _suggest_linker_error(self, error: BuildError) -> str:
        library = error.metadata.get("library", "")
        if library:
            return (
                f"Linker can't find library: -l{library}.\n"
                f"Install the library or add it to CMakeLists.txt target_link_libraries."
            )
        return f"Linker error. Check CMakeLists.txt link configuration."
