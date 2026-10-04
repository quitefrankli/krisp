#pragma once

#include <functional>
#include <map>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

class GameEngine;

// Register and execute on the game thread. Handlers receive the remaining
// argument text unchanged and return feedback for the command prompt.
class CommandRegistry
{
public:
	using Handler = std::function<std::string(GameEngine&, std::string_view)>;
	struct Entry { std::string name; std::string description; };

	void add(std::string name, std::string description, Handler handler)
	{
		if (name.empty() || name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789_-") != std::string::npos || !handler)
			throw std::invalid_argument("Commands require a lowercase name and a handler");
		if (!commands.emplace(std::move(name), Command{std::move(description), std::move(handler)}).second)
			throw std::invalid_argument("Command name is already registered");
	}

	std::vector<Entry> matches(std::string_view prefix) const
	{
		if (prefix.starts_with('/')) prefix.remove_prefix(1);
		std::vector<Entry> result;
		for (const auto& [name, command] : commands)
			if (name.starts_with(prefix)) result.push_back({name, command.description});
		return result;
	}

	std::string execute(GameEngine& engine, std::string_view text) const
	{
		if (!text.starts_with('/')) return "Commands must start with /";
		text.remove_prefix(1);
		const auto separator = text.find_first_of(" \t\r\n");
		const auto found = commands.find(text.substr(0, separator));
		if (found == commands.end()) return "Unknown command. Use /help to list commands.";
		std::string_view arguments;
		if (separator != std::string_view::npos)
		{
			arguments = text.substr(separator);
			const auto first = arguments.find_first_not_of(" \t\r\n");
			arguments = first == std::string_view::npos ? std::string_view{} : arguments.substr(first);
		}
		// Copy so a handler can register another command safely.
		const auto handler = found->second.handler;
		try { return handler(engine, arguments); }
		catch (const std::exception& error) { return std::string("Command failed: ") + error.what(); }
	}

private:
	struct Command { std::string description; Handler handler; };
	std::map<std::string, Command, std::less<>> commands;
};
