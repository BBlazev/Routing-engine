#ifndef CORE_PATHS_HPP
#define CORE_PATHS_HPP

#include <filesystem>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>


namespace paths {

namespace detail {

inline std::filesystem::path& root_storage() {
	static std::filesystem::path root;
	return root;
}

inline std::filesystem::path& exe_dir_storage() {
	static std::filesystem::path dir;
	return dir;
}

inline bool walk_up(std::filesystem::path start, const char* marker,
	std::filesystem::path& out) {
	std::error_code ec;
	start = std::filesystem::absolute(start, ec);
	if (ec) return false;

	for (int i = 0; i < 8 && !start.empty(); ++i) {
		if (std::filesystem::exists(start / marker, ec)) {
			out = start;
			return true;
		}
		if (!start.has_parent_path() || start.parent_path() == start) break;
		start = start.parent_path();
	}
	return false;
}

inline std::vector<std::filesystem::path> candidates(std::string_view subdir,
	std::string_view relative) {
	const std::filesystem::path rel{ relative };
	const std::filesystem::path sub{ subdir };

	std::vector<std::filesystem::path> out;

	if (!root_storage().empty()) out.push_back(root_storage() / sub / rel);

	if (!exe_dir_storage().empty()) {
		out.push_back(exe_dir_storage() / sub / rel);
		out.push_back(exe_dir_storage() / rel);
	}

	std::error_code ec;
	const std::filesystem::path cwd = std::filesystem::current_path(ec);
	if (!ec) {
		out.push_back(cwd / sub / rel);
		out.push_back(cwd / rel);
	}

	return out;
}

inline std::filesystem::path resolve(std::string_view subdir, std::string_view relative) {
	const auto list = candidates(subdir, relative);

	std::error_code ec;
	for (const auto& candidate : list) {
		if (std::filesystem::exists(candidate, ec)) return candidate;
	}

	return list.empty() ? std::filesystem::path{ relative } : list.front();
}

} // namespace detail

inline void init(const char* argv0 = nullptr, const char* marker = "assets") {
	std::error_code ec;

	if (argv0 != nullptr) {
		std::filesystem::path exe = std::filesystem::absolute(argv0, ec);
		if (!ec && exe.has_parent_path()) detail::exe_dir_storage() = exe.parent_path();
	}

	std::filesystem::path found;

	if (!detail::exe_dir_storage().empty() &&
		detail::walk_up(detail::exe_dir_storage(), marker, found)) {
		detail::root_storage() = found;
		return;
	}

	if (detail::walk_up(std::filesystem::current_path(ec), marker, found)) {
		detail::root_storage() = found;
		return;
	}

	throw std::runtime_error(
		std::string{ "Could not locate project root: no '" } + marker +
		"' directory found walking up from the executable or working directory.");
}

inline const std::filesystem::path& root() {
	if (detail::root_storage().empty())
		throw std::runtime_error("paths::init() was never called");
	return detail::root_storage();
}

inline const std::filesystem::path& exe_dir() { return detail::exe_dir_storage(); }

inline std::filesystem::path asset(std::string_view relative) {
	return detail::resolve("assets", relative);
}

inline std::filesystem::path shader(std::string_view relative) {
	return detail::resolve("shaders", relative);
}


inline std::string describe_search(std::string_view subdir, std::string_view relative) {
	std::string out;
	for (const auto& candidate : detail::candidates(subdir, relative)) {
		out += "\n    ";
		out += candidate.string();
	}
	return out;
}

} // namespace paths

#endif // CORE_PATHS_HPP