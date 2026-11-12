#pragma once
#include <string>
namespace decomind {
// Session-only until a platform protected-secret adapter is available.
// Never call Mod::setSavedValue for a credential.
class ProviderCredentials {
    static inline std::string s_key, s_provider;
public:
    static void clear() { std::fill(s_key.begin(), s_key.end(), '\0'); s_key.clear(); s_provider.clear(); }
    static void set(std::string provider, std::string key) { clear(); s_provider = std::move(provider); s_key = std::move(key); }
    static std::string get(std::string const& provider) { return provider == s_provider ? s_key : ""; }
};
}
