// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (c) 2026 krotname

#include "PluginShortcutTests.h"

#include <iostream>

int main()
{
	const int failures = runPluginShortcutTests();
	if (failures != 0)
	{
		std::cerr << failures << " plugin shortcut test(s) failed\n";
		return 1;
	}

	std::cout << "Plugin shortcut tests passed\n";
	return 0;
}
