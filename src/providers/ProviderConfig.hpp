#pragma once
#include <string>
namespace decomind {
class ProviderConfig {
public:
    static std::string model(std::string const& provider, std::string const& configured) {
        // Recognize the old OpenAI default when switching an existing install to Groq.
        if (configured.empty() || (provider == "groq" && configured == "gpt-4.1-mini"))
            return provider == "groq" ? "openai/gpt-oss-120b" : "gpt-4.1-mini";
        return configured;
    }
    static std::string baseUrl(std::string const& provider) {
        return provider == "groq" ? "https://api.groq.com/openai/v1" : "https://api.openai.com/v1";
    }
};
}
