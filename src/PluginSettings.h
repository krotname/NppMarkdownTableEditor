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
	// Automatic modes stay disabled until the user enables them and the state is saved.
	bool autoFitEnabled(const std::wstring &path) const;
	bool autoAlignEnabled(const std::wstring &path) const;
	bool setAutoFormatEnabled(const std::wstring &path, bool autoFit, bool autoAlign);
private:
	bool warningSuppressed_ = false;
	bool warningLoaded_ = false;
};

std::string nativeLangFileNameFromDisk();
}
