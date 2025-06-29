#pragma once
#include <string_view>
#include <fmt/format.h>

enum class ELogLevel
{
	Debug,
	Info,
	Warn,
	Error,
};

struct ILogger
{
	virtual ~ILogger() {}

	virtual void LogMessage(ELogLevel logLevel, std::string_view msg) = 0;

	void VLogMessage(ELogLevel logLevel, std::string_view format, const fmt::format_args &args)
	{
		std::string text = fmt::vformat(format, args);
		LogMessage(logLevel, text);
	}

	template <typename... Args>
    void LogDebug(std::string_view format, const Args &...args) { VLogMessage(ELogLevel::Debug, format, fmt::make_format_args(args...)); }

	template <typename... Args>
    void LogInfo(std::string_view format, const Args &...args) { VLogMessage(ELogLevel::Info, format, fmt::make_format_args(args...)); }

	template <typename... Args>
    void LogWarn(std::string_view format, const Args &...args) { VLogMessage(ELogLevel::Warn, format, fmt::make_format_args(args...)); }

	template <typename... Args>
    void LogError(std::string_view format, const Args &...args) { VLogMessage(ELogLevel::Error, format, fmt::make_format_args(args...)); }
};
