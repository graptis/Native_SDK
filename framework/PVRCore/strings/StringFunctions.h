/*!
\brief Contains helper functions for std::string manipulation.
\file PVRCore/strings/StringFunctions.h
\author PowerVR by Imagination, Developer Technology Team
\copyright Copyright (c) Imagination Technologies Limited.
*/
#pragma once
#include <algorithm>
#include <vector>
#include <cstdarg>
#include <cstring>

namespace pvr {
/// <summary>Contains several valuable helpers to assist with common std::string operations: Starts with, ends with, create
/// with printf-style formatting and others.</summary>
namespace strings {
/// <summary>Creates an std::string from a printf style vararg list.</summary>
/// <param name="format">printf-style format std::string</param>
/// <param name="argumentList">printf-style va_list</param>
/// <returns>A formatted string, as if the parameters were passed to printf</returns>
inline std::string vaFormatString(const char* format, va_list argumentList)
{
#if defined(_WIN32)
	int newStringSize = _vscprintf(format, argumentList);
#else
	va_list argumentListCopy;

	va_copy(argumentListCopy, argumentList);
	int newStringSize = vsnprintf(0, 0, format, argumentListCopy);
	va_end(argumentListCopy);
#endif

	// Create a new std::string
	std::string newString;
	newString.resize(static_cast<uint32_t>(newStringSize + 1));
	bool res = true;
	{
#ifdef _WIN32
		if (_vsnprintf_s(const_cast<char*>(newString.c_str()), newStringSize + 1, newStringSize, format, argumentList) != newStringSize)
#else
		int32_t n = vsnprintf(const_cast<char*>(newString.data()), newStringSize + 1, format, argumentList);
		if ((n < 0) || (n >= newStringSize + 1))
#endif
		{
			res = false;
		}
	}

	if (!res)
	{
		newString.resize(0);
	}

	return std::string(newString);
}

/// <summary>Creates an std::wstring from a printf style vararg list.</summary>
/// <param name="format">printf-style wide format std::string</param>
/// <param name="argumentList">printf-style va_list</param>
/// <returns>A formatted string, as if the parameters were passed to printf</returns>
inline std::basic_string<wchar_t> vaFormatString(const wchar_t* const format, va_list argumentList)
{
	bool result = true;

#if defined(_WIN32)
	int newStringSize = _vscwprintf(format, argumentList);
#else
	va_list argumentListCopy;

	va_copy(argumentListCopy, argumentList);
	int newStringSize = vswprintf(0, 0, format, argumentListCopy);
	va_end(argumentListCopy);
#endif

	// Create a new std::string
	std::basic_string<wchar_t> newString;
	newString.resize(static_cast<uint32_t>(newStringSize + 1));

	{
#ifdef _WIN32
		if (_vsnwprintf_s(const_cast<wchar_t*>(newString.c_str()), newStringSize + 1, newStringSize, format, argumentList) != newStringSize)
#else
		if (vswprintf(const_cast<wchar_t*>(newString.c_str()), static_cast<size_t>(newStringSize) + 1, format, argumentList) != newStringSize)
#endif
		{
			result = false;
		}
	}

	if (!result)
	{
		newString.resize(0);
	}

	return std::basic_string<wchar_t>(newString);
}

/// <summary>Creates an std::string with a printf-like command.</summary>
/// <param name="format">printf-style format std::string</param>
/// <param name="...">printf-style variable arguments</param>
/// <returns>A string containing the output as if the params were passed through printf</returns>
inline std::string createFormatted(const char* format, ...)
{
	// Calculate the length of the new std::string
	va_list argumentList;
	va_start(argumentList, format);
	std::string newString = strings::vaFormatString(format, argumentList);
	va_end(argumentList);

	return newString;
}

/// <summary>Creates an std::wstring with a printf-like command.</summary>
/// <param name="format">printf-style wide format std::string</param>
/// <param name="...">printf-style variable arguments</param>
/// <returns>A string containing the output as if the params were passed through printf</returns>
inline std::basic_string<wchar_t> createFormatted(const wchar_t* format, ...)
{
	// Calculate the length of the new std::string
	va_list argumentList;
	va_start(argumentList, format);
	std::basic_string<wchar_t> newString = strings::vaFormatString(format, argumentList);
	va_end(argumentList);
	return newString;
}

/// <summary>Transforms a std::string to lowercase in place.</summary>
/// <param name="str">A std::string to transform to lowercase.</param>
/// <returns>The string passed as a param, transformed to lowercase in-place.
/// If read only, behaviour is undefined.</returns>
inline std::string& toLower(std::string& str)
{
	std::transform(str.begin(), str.end(), str.begin(), tolower);
	return str;
}

/// <summary>Transforms a std::string to lowercase.</summary>
/// <param name="str">A std::string to transform.</param>
/// <returns>A string otherwise equal to the param str, but transformed to lowercase</returns>
inline std::string toLower(const std::string& str)
{
	std::string s = str;
	return toLower(s);
}

/// <summary>Skips any beginning space, tab or new-line characters, advancing the pointer to the first
/// non-whitespace character</summary>
/// <param name="myString">Pointer to a c-style std::string. Will be advanced to the first non-whitespace char (or the null
/// terminator if no other characters exist)</param>
inline void ignoreWhitespace(char** pszString)
{
	while (*pszString[0] == '\t' || *pszString[0] == '\n' || *pszString[0] == '\r' || *pszString[0] == ' ')
	{
		(*pszString)++;
	}
}

/// <summary>Reads next strings to the end of the line and interperts as a token.</summary>
/// <param name="pToken">The std::string</param>
/// <returns>char* The</returns>
inline char* readEOLToken(char* pToken)
{
	char* pReturn = NULL;

	char szDelim[2] = { '\n', 0 }; // try newline
	pReturn = strtok(pToken, szDelim);
	if (pReturn == NULL)
	{
		szDelim[0] = '\r';
		pReturn = strtok(pToken, szDelim); // try linefeed
	}
	return pReturn;
}

/// <summary>Outputs a block of text starting from nLine and ending when the std::string endStr is found.</summary>
/// <param name="outStr">output text</param>
/// <param name="line">Input start line number, outputs end line number</param>
/// <param name="lines">Input text - one array element per line</param>
/// <param name="endStr">End std::string: When this std::string is encountered, the procedure will stop.</param>
/// <param name="limit">A limit to the number of lines concatenated</param>
/// <returns>true if successful, false if endStr was not found before lines finished or limit was reached
/// </returns>
inline bool concatenateLinesUntil(std::string& outStr, int& line, const std::vector<std::string>& lines, uint32_t limit, const char* endStr)
{
	uint32_t i, j;
	size_t nLen;

	nLen = 0;
	for (i = line; i < limit; ++i)
	{
		if (strcmp(lines[i].c_str(), endStr) == 0)
		{
			break;
		}
		nLen += strlen(lines[i].c_str()) + 1;
	}
	if (i == limit)
	{
		return false;
	}

	if (nLen)
	{
		++nLen;

		outStr.reserve(nLen);

		for (j = line; j < i; ++j)
		{
			outStr.append(lines[j]);
			outStr.append("\n");
		}
	}

	line = i;
	return true;
}

/// <summary>Tests if a std::string starts with another std::string.</summary>
/// <param name="str">The std::string whose beginning will be checked</param>
/// <param name="substr">The sequence of characters to check if str starts with</param>
/// <returns>true if the std::string substr is indeed the first characters of str, false otherwise.</returns>
inline bool startsWith(const char* str, const char* substr)
{
	int current = 0;
	while (str[current] && substr[current])
	{
		if (str[current] != substr[current])
		{
			return false;
		}
		++current;
	}
	if (!str[current] && substr[current])
	{
		return false;
	}
	return true;
}
/// <summary>Tests if a std::string starts with another std::string.</summary>
/// <param name="str">The std::string whose beginning will be checked</param>
/// <param name="substr">The sequence of characters to check if str starts with</param>
/// <returns>true if the std::string substr is indeed the first characters of str, false otherwise.</returns>
inline bool startsWith(const std::string& str, const std::string& substr)
{
	return startsWith(str.c_str(), substr.c_str());
}

/// <summary>Tests if a std::string starts with another std::string.</summary>
/// <param name="str">The std::string whose beginning will be checked</param>
/// <param name="substr">The sequence of characters to check if str starts with</param>
/// <returns>true if the std::string substr is indeed the first characters of str, false otherwise.</returns>
inline bool startsWith(const std::string& str, const char* substr)
{
	return startsWith(str.c_str(), substr);
}

/// <summary>Tests if a std::string ends with another std::string.</summary>
/// <param name="str">The std::string whose end will be checked. Not null terminated - length is passed explicitly.
/// </param>
/// <param name="lenStr">The length of str</param>
/// <param name="substr">The sequence of characters to check if str ends with. Not null terminated - length is
/// passed explicitly.</param>
/// <param name="lenSubstr">The length of Substr</param>
/// <returns>true if the std::string substr is indeed the last characters of str, false otherwise.</returns>
inline bool endsWith(const char* str, int32_t lenStr, const char* substr, int32_t lenSubstr)
{
	if (lenSubstr > lenStr || !lenStr--)
	{
		return false;
	}
	if (!lenSubstr--)
	{
		return true;
	}
	while (lenStr >= 0 && lenSubstr >= 0)
	{
		if (str[lenStr--] != substr[lenSubstr--])
		{
			return false;
		}
	}
	if (!lenStr && lenSubstr)
	{
		return false;
	}
	return true;
}

/// <summary>Tests if a std::string ends with another std::string.</summary>
/// <param name="str">The std::string whose end will be checked.</param>
/// <param name="substr">The sequence of characters to check if str ends with.</param>
/// <returns>true if the std::string substr is indeed the last characters of str, false otherwise.</returns>
inline bool endsWith(const std::string& str, const std::string& substr)
{
	return endsWith(str.c_str(), static_cast<int32_t>(str.length()), substr.c_str(), static_cast<int32_t>(substr.length()));
}

/// <summary>Tests if a std::string starts with another std::string.</summary>
/// <param name="str">The std::string whose end will be checked.</param>
/// <param name="substr">The sequence of characters to check if str starts with.</param>
/// <returns>true if the std::string substr is indeed the first characters of str, false otherwise.</returns>
inline bool endsWith(const std::string& str, const char* substr)
{
	return endsWith(str.c_str(), static_cast<int32_t>(str.length()), substr, static_cast<int32_t>(strlen(substr)));
}

/// <summary>Tests if a std::string starts with another std::string.</summary>
/// <param name="str">The std::string whose end will be checked.</param>
/// <param name="substr">The sequence of characters to check if str starts with.</param>
/// <returns>true if the std::string substr is indeed the first characters of str, false otherwise.</returns>
inline bool endsWith(const char* str, const char* substr)
{
	return endsWith(str, static_cast<int32_t>(strlen(str)), substr, static_cast<int32_t>(strlen(substr)));
}

inline void getFileDirectory(const std::string& fileName, std::string& outFileDir)
{
	outFileDir.clear();
	auto pos = fileName.find_last_of("/");
	if (pos == std::string::npos)
	{
		return;
	}
	outFileDir.assign(fileName.substr(0, pos));
}

/// <summary>Separate a filename to name and extension</summary>
/// <param name="fileAndExtension">A filename</param>
/// <param name="filename">The file part of the name (part before the last '.')</param>
/// <param name="extension">The extension part of the name (part after the last '.')</param>
/// <remarks>The period is returned in neither filename nor the extension</remarks>
inline void getFileNameAndExtension(const std::string& fileAndExtension, std::string& filename, std::string& extension)
{
	auto it = std::find(fileAndExtension.rbegin(), fileAndExtension.rend(), '.');
	if (it == fileAndExtension.rend())
	{
		filename = fileAndExtension;
		extension = "";
		return;
	}
	size_t position = fileAndExtension.rend() - it; // rend is the position one-after the start of the std::string.
	filename.assign(fileAndExtension.begin(), fileAndExtension.begin() + position - 1);
	extension.assign(fileAndExtension.begin() + position, fileAndExtension.end());
}
} // namespace strings
} // namespace pvr
