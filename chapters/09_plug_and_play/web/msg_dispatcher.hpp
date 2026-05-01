#pragma once
#include <cassert>
#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>

// Inbound message protocol contract: "type" MUST be the first key.
// This is a controlled internal protocol -- the browser sender enforces key ordering.
class MsgDispatcher {
public:
	using Handler = std::function<void(std::string_view)>;

	void on(std::string type, Handler h) { handlers[std::move(type)] = std::move(h); }

	void dispatch(std::string_view msg) const {
		auto type = extract_type(msg);
		assert(!type.empty() && "Inbound message missing \"type\" as first key");
		auto it = handlers.find(std::string(type));
		if (it != handlers.end()) it->second(msg);
	}

private:
	// Only valid if "type" is the first key: {"type":"<value>", ...}
	static std::string_view extract_type(std::string_view msg) {
		constexpr std::string_view prefix = R"({"type":")";
		if (!msg.starts_with(prefix)) return {};
		auto end = msg.find('"', prefix.size());
		return end == std::string_view::npos ? std::string_view {} :
		                                       msg.substr(prefix.size(), end - prefix.size());
	}

	std::unordered_map<std::string, Handler> handlers;
};