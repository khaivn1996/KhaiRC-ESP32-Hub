#pragma once
#include <cstddef>
#include <cstdint>
namespace WebUiPage {
const char* html();
const char* script();
const char* vaultHead();
const char* vaultTail();
const char* vaultMissing();
const char* manifest();
const char* icon();
const std::uint8_t* iconPng();
std::size_t iconPngLen();
}
