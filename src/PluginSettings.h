#pragma once

#include <string>

namespace MarkdownTablePlugin
{
// INI persistence is independent of how Notepad++ resolves the config directory.
class PluginSettings
{
public:
	bool wordWrapAutoFitWarningSuppressed(const std::wstring &path);
	void setWordWrapAutoFitWarningSuppressed(const std::wstring &path, bool suppressed);
private:
	bool warningSuppressed_ = false;
	bool warningLoaded_ = false;
};

std::string nativeLangFileNameFromDisk();
}
