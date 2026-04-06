#pragma once
#include <charconv>
#include <string>
#include <string_view>

// Builds flat JSON objects with no heap allocation for the common case.
// Usage: JsonMsg("meter").field("rms_l", 0.1f).field("rms_r", 0.1f).str()
class JsonMsg {
public:
	explicit JsonMsg(std::string_view type) {
		buf.reserve(128);
		buf = R"({"type":")";
		buf += type;
		buf += '"';
	}

	JsonMsg &field(std::string_view key, float val) {
		append_key(key);
		char tmp[32];
		auto [ptr, _] = std::to_chars(tmp, tmp + sizeof(tmp), val, std::chars_format::fixed, 4);
		buf.append(tmp, ptr);
		return *this;
	}

	JsonMsg &field(std::string_view key, int val) {
		append_key(key);
		buf += std::to_string(val);
		return *this;
	}

	JsonMsg &field(std::string_view key, std::string_view val) {
		append_key(key);
		buf += '"';
		buf += val;
		buf += '"';
		return *this;
	}

	std::string str() {
		buf += '}';
		return std::move(buf);
	}

private:
	void append_key(std::string_view key) {
		buf += ",\"";
		buf += key;
		buf += "\":";
	}

	std::string buf;
};