#include "PluginSettings.h"

#include <windows.h>
#include <cstdlib>
#include <fstream>

namespace MarkdownTablePlugin
{
bool PluginSettings::wordWrapAutoFitWarningSuppressed(const std::wstring &path)
{
	if (!warningLoaded_ && !path.empty())
	{
		warningSuppressed_ = ::GetPrivateProfileInt(L"Warnings", L"HideWordWrapAutoFitWarning", 0, path.c_str()) != 0;
		warningLoaded_ = true;
	}
	return warningSuppressed_;
}

void PluginSettings::setWordWrapAutoFitWarningSuppressed(const std::wstring &path, bool suppressed)
{
	warningSuppressed_ = suppressed;
	warningLoaded_ = true;
	if (!path.empty())
		::WritePrivateProfileString(L"Warnings", L"HideWordWrapAutoFitWarning", suppressed ? L"1" : L"0", path.c_str());
}

std::string extractNativeLangFileName(const std::string &text)
{
	const std::string marker = "filename=\"";
	const std::size_t start = text.find(marker);
	if (start == std::string::npos)
		return std::string();

	const std::size_t valueStart = start + marker.size();
	const std::size_t valueEnd = text.find('"', valueStart);
	if (valueEnd == std::string::npos || valueEnd <= valueStart)
		return std::string();

	return text.substr(valueStart, valueEnd - valueStart);
}

std::string nativeLangFileNameFromDisk()
{
#ifdef MARKDOWN_TABLE_PLUGIN_TESTING
	return std::string();
#else
	const char *appData = std::getenv("APPDATA");
	if (!appData || !*appData)
		return std::string();

	const std::string path = std::string(appData) + "\\Notepad++\\nativeLang.xml";
	std::ifstream input(path.c_str(), std::ios::in | std::ios::binary);
	if (!input)
		return std::string();

	std::string text;
	text.reserve(4096);
	char buffer[1024];
	while (input && text.size() < 4096)
	{
		input.read(buffer, sizeof(buffer));
		text.append(buffer, static_cast<std::size_t>(input.gcount()));
	}
	return extractNativeLangFileName(text);
#endif
}
}
