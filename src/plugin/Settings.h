#pragma once

#include "core/Core.h"

namespace AG
{
	// Data\SKSE\Plugins\Afterglow.ini - written by the SKSE Menu Framework page.
	struct Settings
	{
		float opacity{ 1.0f };          // 0..1
		float dryHours{ 24.0f };        // game hours a layer stays; 0 = until washed
		float fadeSeconds{ 1.5f };      // real seconds of the fade-out when a layer is removed
		// wet look of the layers (owner 17.11: more shine). The overlay shader copies the skin's values
		// (vanilla skin: glossiness 30 = broad matte sheen); a liquid needs a tight, brighter highlight.
		float glossiness{ 80.0f };      // specular power; <= 0 keeps the skin's
		float specular{ 4.0f };         // specular strength; <= 0 keeps the skin's
		int   startSlot{ 7 };           // first overlay slot Afterglow may use in every area
		std::array<int, kZoneCount> maxLayers{ 6, 6, 6, 6, 2, 2, 2 };
		bool  maleTargets{ false };
		bool  followSexLabCleanup{ false };  // owner 16.11: Afterglow's own drying timer for every zone; on = P+ "SexLabClearCum" also clears
		bool  washOnBathing{ true };        // Bathing in Skyrim / Dirt and Blood
		bool  washOnSwimming{ true };       // swimming (also underwater) removes every layer; checked every 2 s
		bool  hideSexLabOverlays{ true };   // P+ fCumAlpha = 0 while on
		float sexLabAlphaBefore{ -1.0f };   // the fCumAlpha we replaced (restored when switched off)
		int   fallbackZone{ 0 };            // untagged scene, P+ found no type: 0 vaginal 1 anal 2 oral
		int   classicCumSpells{ -1 };       // SexLab cum spells outside a scene add a layer: -1 on SexLab Classic only, 0 never, 1 always (test)
		std::vector<std::string> megaRaces; // race EditorIDs whose cum uses the m_ textures
		bool  debugLog{ false };

		static Settings& Get();
		void Load();
		void Save() const;
	};
}
