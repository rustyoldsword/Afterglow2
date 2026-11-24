#pragma once

namespace AG::SexLabAlpha
{
	// Which SexLab is loaded. P+ has sslSystemConfig.GetSettingFlt, Classic 1.6x has the type
	// without it. Valid from kNewGame/kPostLoadGame on (the script types are loaded then).
	enum class Kind
	{
		None,
		Classic,
		Plus
	};
	Kind Detect();

	// "Hide SexLab P+'s own cum overlays": SexLab keeps its spells, layer counts and events
	// (Fill Her Up, SL Survival... still see the cum), only its overlay alpha goes to 0.
	// No SexLab file is replaced, so it works the same on 2.18.1, 2.19 and Dwemer's Edition.
	// Sync() applies the menu setting; the replaced value is remembered and restored when off.
	void Sync();
}
