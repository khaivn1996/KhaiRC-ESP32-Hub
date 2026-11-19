#include "ApiAuth.h"
#include "secrets.h"  // HUB_API_KEY only; never print it.

namespace {
constexpr std::size_t KEY_SIZE = sizeof(HUB_API_KEY) - 1;
static_assert(sizeof(HUB_API_KEY) == 65, "HUB_API_KEY must contain 64 characters");
}  // namespace

namespace ApiAuth {

bool hubKeyMatches(const char* provided, std::size_t length) {
    return provided != nullptr && length == KEY_SIZE &&
           constantTimeEquals(provided, HUB_API_KEY, KEY_SIZE);
}

}  // namespace ApiAuth
