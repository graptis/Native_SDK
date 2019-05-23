/*!
\brief This file contains Logging functionality.
\file PVRCore/Log.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/
#ifndef _PVR_LOG_H
#define _PVR_LOG_H
#include <string>
#include <assert.h>
#pragma once
#include <cstdlib>
#include <cstdarg>
#include <cstring>
#include <cstdio>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#endif
#include <Windows.h>
#define vsnprintf _vsnprintf
#endif

#if defined(__linux__)
#include <sys/stat.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <signal.h>
#endif

#if defined(__ANDROID__)
#include <android/log.h>
static const android_LogPriority messageTypes[] = {
	ANDROID_LOG_VERBOSE,
	ANDROID_LOG_DEBUG,
	ANDROID_LOG_INFO,
	ANDROID_LOG_WARN,
	ANDROID_LOG_ERROR,
	ANDROID_LOG_FATAL,
};
#elif defined(__QNXNTO__)
#include <sys/slog.h>
static const int messageTypes[] = { _SLOG_DEBUG1, _SLOG_DEBUG1, _SLOG_INFO, _SLOG_WARNING, _SLOG_ERROR, _SLOG_CRITICAL };
#else
static const char* messageTypes[] = { "VERBOSE: ", "DEBUG: ", "INFORMATION: ", "WARNING: ", "ERROR: ", "CRITICAL: ", "PERFORMANCE: " };
#endif

#include "PVRCore/Base/Types.h"

//!\cond NO_DOXYGEN
#if defined(TARGET_OS_IPHONE) || defined(TARGET_IPHONE_SIMULATOR) || defined(__ANDROID__)
#define PVR_PLATFORM_IS_MOBILE 1
#else
#define PVR_PLATFORM_IS_DESKTOP 1
#endif
//!\endcond

/// <summary>Checks whether a debugger can be found for the current running process (on Windows and Linux only).
/// The prescene of a debugger can be used to provide additional helpful functionality for debugging application issues one of which could be to break in the
/// debugger when an exception is thrown. Being able to have the debugger break on such a thrown exception provides by far the most seamless and constructive environment for
/// fixing an issue causing the exception to be thrown due to the full state and stack trace being present at the point in which the issue has occurred rather
/// than relying on error logic handling.</summary>
/// <returns>True if a debugger can be found for the current running process else False.</returns>
inline bool isDebuggerPresent()
{
	// only check once for whether the debugger is present as this may not be efficient to determine
	static bool isUsingDebugger = false;
	static bool haveCheckedForDebugger = false;
	if (!haveCheckedForDebugger)
	{
#if defined(_MSC_VER)
		if (IsDebuggerPresent())
		{
			isUsingDebugger = true;
		}
#elif defined(__linux__)
		// reference implementation taken from: https://stackoverflow.com/a/24969863
		char buf[1024];

		int status_fd = open("/proc/self/status", O_RDONLY);
		if (status_fd == -1)
		{
			isUsingDebugger = false;
		}
		else
		{
			ssize_t num_read = read(status_fd, buf, sizeof(buf) - 1);
			if (num_read > 0)
			{
				static const char TracerPid[] = "TracerPid:";
				char* tracer_pid;

				buf[num_read] = 0;
				tracer_pid = strstr(buf, TracerPid);
				if (tracer_pid)
				{
					isUsingDebugger = !!atoi(tracer_pid + sizeof(TracerPid) - 1);
				}
			}
		}
#endif
		haveCheckedForDebugger = true;
	}

	return isUsingDebugger;
}

/// <summary>Enumerates possible severities from Critical down to Debug.</summary>
enum class LogLevel
{
	Verbose = 0,
	Debug = 1,
	Information = 2,
	Warning = 3,
	Error = 4,
	Critical = 5,
	Performance = 6,
	None = 100,
};

/// <summary>Represents an object capable of providing Logging functionality. This class is normally instantiated and
/// configured, not inherited from. The components providing the Logging capability are contained in this class
/// through interfaces, and as such can be replaced with custom components.</summary>
class ILogger
{
public:
	ILogger() : _verbosityThreshold(LogLevel::Information) {}
	/// <summary>Set the verbosity threshold below which messages will not be output.</summary>
	/// <param name="minimumLevelToOutput">The minimum level to actually output.</param>
	/// <remarks>Messages with a severity less than this will be silently discarded. For example, if using a "Warning"
	/// level, Critical, Error and Warning will be displayed, while Information, Verbose and Debug will be discarded.
	/// </remarks>
	void setVerbosity(const LogLevel minimumLevelToOutput)
	{
		_verbosityThreshold = minimumLevelToOutput;
	}
	/// <summary>Get the verbosity threshold below which messages will not be output.</summary>
	/// <returns>The minimum level that is currently output.</returns>
	/// <remarks>Messages with a severity less than this will be silently discarded. For example, if using a "Warning"
	/// level, Critical, Error and Warning will be displayed, while Information, Verbose and Debug will be discarded.
	/// </remarks>
	LogLevel getVerbosity() const
	{
		return _verbosityThreshold;
	}

	/// <summary>Functor operator used to allow an instance of this class to be called as a function. Logs a message using
	/// this logger's message handler.</summary>
	/// <param name="severity">The severity of the message. Apart from being output into the message, the severity is
	/// used by the logger to discard log events less than a specified threshold. See setVerbosity(...)</param>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
	void operator()(LogLevel severity, const char* const formatString, ...) const
	{
		if (severity < _verbosityThreshold)
		{
			return;
		}
		va_list argumentList;
		va_start(argumentList, formatString);
		vaOutput(severity, formatString, argumentList);
		va_end(argumentList);
	}

	/// <summary>Functor operator used to allow an instance of this class to be called as a function. Logs a message using
	/// this logger's message handler. Severity is fixed to "Error".</summary>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
	void operator()(const char* const formatString, ...) const
	{
		if (LogLevel::Error < _verbosityThreshold)
		{
			return;
		}
		va_list argumentList;
		va_start(argumentList, formatString);
		vaOutput(LogLevel::Error, formatString, argumentList);
		va_end(argumentList);
	}

	/// <summary>Logs a message using this logger's message handler.</summary>
	/// <param name="severity">The severity of the message. Apart from being output into the message, the severity is
	/// used by the logger to discard log events less than a specified threshold. See setVerbosity(...)</param>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
	void output(LogLevel severity, const char* formatString, ...) const
	{
		if (severity < _verbosityThreshold)
		{
			return;
		}
		va_list argumentList;
		va_start(argumentList, formatString);
		vaOutput(severity, formatString, argumentList);
		va_end(argumentList);
	}
	/// <summary>Logs a message using this logger's message handler.</summary>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
	void output(const char* formatString, ...) const
	{
		if (LogLevel::Error < _verbosityThreshold)
		{
			return;
		}
		va_list argumentList;
		va_start(argumentList, formatString);
		vaOutput(LogLevel::Error, formatString, argumentList);
		va_end(argumentList);
	}

	/// <summary>Varargs version of the "output" function.</summary>
	/// <param name="severity">The severity of the message. Apart from being output into the message, the severity is
	/// used by the logger to discard log events less than a specified threshold. See setVerbosity(...)</param>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="argumentList">Variable arguments list for the format std::string. Printf-style rules</param>
	virtual void vaOutput(LogLevel severity, const char* formatString, va_list argumentList) const = 0;

private:
	LogLevel _verbosityThreshold;
};

/// <summary>Represents an object capable of providing Logging functionality. This class is normally instantiated and
/// configured, not inherited from. The components providing the Logging capability are contained in this class
/// through interfaces, and as such can be replaced with custom components.</summary>
class Logger : public ILogger
{
public:
	Logger()
	{
#if defined(PVR_PLATFORM_IS_DESKTOP) && !defined(TARGET_OS_MAC)
		FILE* truncateme = fopen("log.txt", "w");

		if (truncateme)
		{
			fclose(truncateme);
		}
#endif
	}

	/// <summary>Varargs version of the "output" function.</summary>
	/// <param name="severity">The severity of the message. Apart from being output into the message, the severity is
	/// used by the logger to discard log events less than a specified threshold. See setVerbosity(...)</param>
	/// <param name="formatString">A printf-style format std::string</param>
	/// <param name="argumentList">Variable arguments list for the format std::string. Printf-style rules</param>
	virtual void vaOutput(LogLevel severity, const char* formatString, va_list argumentList) const
	{
#ifndef DEBUG
		if (severity > LogLevel::Debug)
#endif
		{
#if defined(__ANDROID__)
			// Note: There may be issues displaying 64bits values with this function
			// Note: This function will truncate long messages
			__android_log_vprint(messageTypes[(int)severity], "com.powervr.Example", formatString, argumentList);
#elif defined(__QNXNTO__)
			vslogf(1, messageTypes[(int)severity], formatString, argumentList);
#else // Not android Not QNX
			static char buffer[4096];
			va_list tempList;
			memset(buffer, 0, sizeof(buffer));
#if (defined _MSC_VER) // Pre VS2013
			tempList = argumentList;
#else
			va_copy(tempList, argumentList);
#endif
			vsnprintf(buffer, 4095, formatString, argumentList);

#if defined(_WIN32) && !defined(_CONSOLE)
			if (isDebuggerPresent())
			{
				OutputDebugString(messageTypes[static_cast<int>(severity)]);
				OutputDebugString(buffer);
				OutputDebugString("\n");
			}
#else
			vprintf(formatString, tempList);
			printf("\n");
#endif
#if defined(PVR_PLATFORM_IS_DESKTOP) && !defined(TARGET_OS_MAC)
			{
				FILE* file = fopen("log.txt", "a");
				if (file)
				{
					fwrite(messageTypes[static_cast<int>(severity)], 1, strlen(messageTypes[static_cast<int>(severity)]), file);
					fwrite(buffer, 1, strlen(buffer), file);
					fwrite("\n", 1, 1, file);
					fclose(file);
				}
			}
#endif
#endif
		}
	}
};

/// <summary>The default logger object. This is the only way to get that object. Is global.</summary>
namespace impl {
static Logger originalDefaultLogger;
}
/// <summary>Returns the original default logger object</summary>
/// <returns>The original default logger global logger.</returns>
inline Logger& originalDefaultLogger()
{
	return impl::originalDefaultLogger;
}

/// <summary>Returns the default logger object. This is the only way to get that object. Is global.</summary>
/// <returns>The default logger global logger.</returns>
inline Logger& DefaultLogger()
{
	static Logger* logger = &originalDefaultLogger();
	return *logger;
}

/// <summary>Logs a message using the default logger.</summary>
/// <param name="severity">The severity of the message. Apart from being output into the message, the severity is
/// used by the logger to discard log events less than a specified threshold. See setVerbosity(...)</param>
/// <param name="formatString">A printf-style format std::string</param>
/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
inline void Log(LogLevel severity, const char* formatString, ...)
{
	va_list argumentList;
	va_start(argumentList, formatString);
	DefaultLogger().vaOutput(severity, formatString, argumentList);
	va_end(argumentList);
}

/// <summary>Logs a message with severity "ERROR" using the default logger (same as calling Log(LogLevel::Error, ...)</summary>
/// <param name="formatString">Printf-style format string for the varargs that follow.</param>
/// <param name="...">Variable arguments for the format std::string. Printf-style rules</param>
inline void Log(const char* formatString, ...)
{
	va_list argumentList;
	va_start(argumentList, formatString);
	DefaultLogger().vaOutput(LogLevel::Error, formatString, argumentList);
	va_end(argumentList);
}

/// <summary>If supported on the platform, makes the debugger break at this line. Used for Assertions on Visual Studio</summary>
inline void debuggerBreak()
{
	if (isDebuggerPresent())
	{
#if defined(__linux__)
		{
			raise(SIGTRAP);
		}
#elif defined(_MSC_VER)
		__debugbreak();
#endif
	}
}

namespace pvr {
inline PvrError::PvrError(std::string message) : std::runtime_error(message)
{
	debuggerBreak();
}
} // namespace pvr

/// <summary>If condition is false, logs a critical error, debug breaks if possible, and - on debug builds - throws an assertion.
/// If you wish to completely compile it out on release, use the macro debug_assertion.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, asserts</param>
/// <param name="message">The message that will be logged if the asserted condition is false.</param>
inline void assertion(bool condition, const std::string& message)
{
	if (!condition)
	{
		Log(LogLevel::Critical, ("ASSERTION FAILED: " + message).c_str());
		debuggerBreak();
		assert(0);
	}
}
//!\cond NO_DOXYGEN
// clang-format off
#define SLASH(s) /##s
#define COMMENT SLASH(/)
	// clang-format on
	//!\endcond

#ifdef DEBUG
/// <summary>Logs a debug message using the default logger. Compiled out on release</summary>
/// <param name="message">A message</param>
#define DebugLog(message) Log(LogLevel::Debug, message)
#else
/// <summary>Logs a debug message using the default logger. Compiled out on release</summary>
/// <param name="message">A message</param>
#define DebugLog(message) void(0)
#endif

#ifdef DEBUG
/// <summary>An assertion that gets completely compiled out in release  builds.
/// Anything inside a macro will be removed, so you may want to put expensive
/// operations for the assert directly here.</summary>
/// <param name="condition">Pass the condition to assert here. Is completely compiled out in release, so this
/// can be taken advantage of by doing potentially expensive operations inline in the function call.</param>
/// <param name="message">The message that will be logged if the asserted condition is false. Is completely
/// compiled out in release, so thiscan be taken advantage of by doing potentially expensive operations
/// such as building the message string inline here.</param>
#define debug_assertion(condition, message) assertion(condition, message)
#else
/// <summary>An assertion that gets completely compiled out in release  builds.
/// Anything inside a macro will be removed, so you may want to put expensive
/// operations for the assert directly here.</summary>
/// <param name="condition">Pass the condition to assert here. Is completely compiled out in release, so this
/// can be taken advantage of by doing potentially expensive operations inline in the function call.</param>
/// <param name="message">The message that will be logged if the asserted condition is false. Is completely
/// compiled out in release, so thiscan be taken advantage of by doing potentially expensive operations
/// such as building the message string inline here.</param>
#define debug_assertion(condition, message) ((void)0)
#endif
#ifdef DEBUG
/// <summary>In debug builds only, log a warning if the condition is false.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, log warning.</param>
/// <param name="message">The message that will be logged if the asserted condition is false.</param>
#define debug_warning(condition, message) assert_warning(condition, message)
#else
/// <summary>An assertion that gets completely compiled out in debug builds.
/// Anything inside a macro will be removed, so you may want to put expensive
/// operations for the assert directly here.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, asserts</param>
/// <param name="message">The message that will be logged if the asserted condition is false.</param>
#define debug_warning(condition, message) ((void)0)
#endif

/// <summary>If condition is false, logs a warning.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, logs warning.</param>
/// <param name="msg">The message that will be logged if the asserted condition is false.</param>
inline void assert_warning(bool condition, const char* msg)
{
	if (!condition)
	{
		Log(LogLevel::Warning, msg);
	}
}

/// <summary>If condition is false, logs a critical error, debug breaks if possible, and - on debug builds - throws an assertion.
/// If you wish to completely compile it out on release, use the macro debug_assertion.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, asserts</param>
/// <param name="msg">The message that will be logged if the asserted condition is false.</param>
inline void assertion(bool condition, const char* msg)
{
	if (!condition)
	{
		Log(LogLevel::Critical, "ASSERTION FAILED: ", msg);
		debuggerBreak();
		assert(0);
	}
}

/// <summary>If condition is false, logs a critical error, debug breaks if possible, and - on debug builds - throws an assertion.
/// If you wish to completely compile it out on release, use the macro debug_assertion.</summary>
/// <param name="condition">Pass the condition to assert here. If true, nothing happens. If false, asserts.</param>
inline void assertion(bool condition)
{
	assertion(condition, "");
}
#endif
