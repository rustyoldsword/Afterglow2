#include "Settings.h"

namespace AG
{
	namespace
	{
		constexpr auto kPath = "Data/SKSE/Plugins/Afterglow.ini";

		const std::array<std::string_view, kZoneCount> kZoneKeys{ "iMaxVaginal", "iMaxAnal", "iMaxBreast", "iMaxFace", "iMaxHands", "iMaxFeet", "iMaxLegs" };

		std::string Trim(std::string_view a_s)
		{
			const auto b = a_s.find_first_not_of(" \t\r\n");
			if (b == std::string_view::npos) {
				return {};
			}
			const auto e = a_s.find_last_not_of(" \t\r\n");
			return std::string(a_s.substr(b, e - b + 1));
		}

		std::vector<std::string> SplitList(std::string_view a_s)
		{
			std::vector<std::string> out;
			std::size_t              pos = 0;
			while (pos <= a_s.size()) {
				const auto next = a_s.find(',', pos);
				auto       item = Trim(a_s.substr(pos, next == std::string_view::npos ? std::string_view::npos : next - pos));
				if (!item.empty()) {
					out.push_back(std::move(item));
				}
				if (next == std::string_view::npos) {
					break;
				}
				pos = next + 1;
			}
			return out;
		}

		std::vector<std::string> DefaultMegaRaces()
		{
			return { "DragonRace", "UndeadDragonRace", "OrcRace", "OrcRaceVampire", "TrollRace", "TrollFrostRace", "CartHorseRace", "HorseRace",
				"MammothRace", "ElkRace", "BearBlackRace", "BearBrownRace", "BearSnowRace", "SabreCatRace", "SabreCatSnowyRace", "HorkerRace",
				"GiantRace", "WerewolfBeastRace", "AtronachFrostRace", "AtronachStormRace", "DwarvenCenturionRace" };
		}
	}

	Settings& Settings::Get()
	{
		static Settings s;
		return s;
	}

	// Plain key=value reader: unknown keys are ignored, bad values keep the default.
	void Settings::Load()
	{
		megaRaces = DefaultMegaRaces();
		std::ifstream in(kPath);
		if (!in) {
			logger::info("settings: {} not found, using defaults", kPath);
			return;
		}
		std::string line;
		while (std::getline(in, line)) {
			const auto t = Trim(line);
			if (t.empty() || t[0] == ';' || t[0] == '#' || t[0] == '[') {
				continue;
			}
			const auto eq = t.find('=');
			if (eq == std::string::npos) {
				continue;
			}
			const auto key = Trim(std::string_view(t).substr(0, eq));
			auto       value = Trim(std::string_view(t).substr(eq + 1));
			if (const auto sc = value.find(';'); sc != std::string::npos) {
				value = Trim(std::string_view(value).substr(0, sc));
			}
			try {
				if (key == "fOpacity") {
					opacity = std::clamp(std::stof(value), 0.0f, 1.0f);
				} else if (key == "fDryHours") {
					dryHours = std::max(0.0f, std::stof(value));
				} else if (key == "fFadeSeconds") {
					fadeSeconds = std::clamp(std::stof(value), 0.0f, 10.0f);
				} else if (key == "fGlossiness") {
					glossiness = std::clamp(std::stof(value), 0.0f, 1000.0f);
				} else if (key == "fSpecular") {
					specular = std::clamp(std::stof(value), 0.0f, 20.0f);
				} else if (key == "iStartSlot") {
					startSlot = std::clamp(std::stoi(value), 0, 127);
				} else if (key == "bMaleTargets") {
					maleTargets = std::stoi(value) != 0;
				} else if (key == "bFollowSexLabCleanup") {
					followSexLabCleanup = std::stoi(value) != 0;
				} else if (key == "bWashOnBathing") {
					washOnBathing = std::stoi(value) != 0;
				} else if (key == "bWashOnSwimming") {
					washOnSwimming = std::stoi(value) != 0;
				} else if (key == "bHideSexLabOverlays") {
					hideSexLabOverlays = std::stoi(value) != 0;
				} else if (key == "fSexLabAlphaBefore") {
					sexLabAlphaBefore = std::stof(value);
				} else if (key == "iFallbackZone") {
					fallbackZone = std::clamp(std::stoi(value), 0, 2);
				} else if (key == "sMegaRaces") {
					megaRaces = SplitList(value);
				} else if (key == "iClassicCumSpells") {
					classicCumSpells = std::clamp(std::stoi(value), -1, 1);
				} else if (key == "bDebugLog") {
					debugLog = std::stoi(value) != 0;
				} else {
					for (std::size_t z = 0; z < kZoneCount; ++z) {
						if (key == kZoneKeys[z]) {
							maxLayers[z] = std::clamp(std::stoi(value), 0, 12);
						}
					}
				}
			} catch (...) {
				logger::warn("settings: bad value for {}: '{}'", key, value);
			}
		}
		logger::info("settings loaded: opacity {:.2f}, gloss {:.0f}, specular {:.1f}, dry {:.1f} h, fade {:.1f} s, start slot {}, male targets {}, hide P+ overlays {}, wash on bathing {}, on swimming {}",
			opacity, glossiness, specular, dryHours, fadeSeconds, startSlot, maleTargets, hideSexLabOverlays, washOnBathing, washOnSwimming);
	}

	void Settings::Save() const
	{
		std::ofstream out(kPath, std::ios::trunc);
		if (!out) {
			logger::warn("settings: cannot write {}", kPath);
			return;
		}
		std::string races;
		for (const auto& r : megaRaces) {
			races += (races.empty() ? "" : ", ") + r;
		}
		out << "; Afterglow 2.0 - written by the SKSE Menu Framework page\n[General]\n";
		out << std::format("fOpacity = {:.2f}\nfGlossiness = {:.0f}\nfSpecular = {:.1f}\nfDryHours = {:.1f}\nfFadeSeconds = {:.1f}\niStartSlot = {}\n",
			opacity, glossiness, specular, dryHours, fadeSeconds, startSlot);
		out << std::format("bMaleTargets = {}\nbFollowSexLabCleanup = {}\nbWashOnBathing = {}\nbWashOnSwimming = {}\n", int(maleTargets), int(followSexLabCleanup), int(washOnBathing), int(washOnSwimming));
		out << std::format("bHideSexLabOverlays = {}\nfSexLabAlphaBefore = {:.2f}\niFallbackZone = {}\niClassicCumSpells = {}\nbDebugLog = {}\n", int(hideSexLabOverlays), sexLabAlphaBefore, fallbackZone, classicCumSpells, int(debugLog));
		out << "\n[Layers]\n";
		for (std::size_t z = 0; z < kZoneCount; ++z) {
			out << std::format("{} = {}\n", kZoneKeys[z], maxLayers[z]);
		}
		out << "\n[Races]\nsMegaRaces = " << races << "\n";
	}
}
