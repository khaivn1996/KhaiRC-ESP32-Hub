#pragma once
namespace OtaSafety {
extern bool safeEnabled;
inline bool canAcceptUpdate() { return safeEnabled; }
}
