#pragma once

namespace rapidjson {

// Minimal compatibility shim for cereal-bundled RapidJSON subset.
// The upstream API takes a ParseErrorCode enum, but cereal's copy exposes
// const char* via GetParseError(). Provide overloads to cover both cases.
inline const char* GetParseError_En(const char* parseError) {
    return parseError ? parseError : "Unknown parse error";
}

inline const char* GetParseError_En(int /*code*/) {
    return "Parse error";
}

} // namespace rapidjson




