#pragma once
#include <charconv>
#include <string>
#include <string_view>

// Simple key extraction for flat JSON objects.
// Allocates a small needle string -- only used on the web thread, not RT.
namespace MsgParser {

inline auto make_needle(std::string_view key) {
	std::string n = "\"";
	n += key;
	n += "\":";
	return n;
}

inline int extract_int(std::string_view msg, std::string_view key) {
	auto needle = make_needle(key);
	auto pos    = msg.find(needle);
	if (pos == std::string_view::npos) return -1;
	pos += needle.size();
	int val = -1;
	std::from_chars(msg.data() + pos, msg.data() + msg.size(), val);
	return val;
}

inline float extract_float(std::string_view msg, std::string_view key) {
	auto needle = make_needle(key);
	auto pos    = msg.find(needle);
	if (pos == std::string_view::npos) return 0.0f;
	pos += needle.size();
	float val = 0.0f;
	std::from_chars(msg.data() + pos, msg.data() + msg.size(), val);
	return val;
}

} // namespace MsgParser