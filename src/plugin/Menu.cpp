#include "Menu.h"

#include "Body.h"
#include "Manager.h"
#include "Settings.h"
#include "SexLabAlpha.h"
#include "Skee.h"

#pragma warning(push)
#pragma warning(disable : 4099 5054)
#include <SKSEMenuFramework.h>
#pragma warning(pop)

namespace AG::Menu
{
	namespace
	{
		void Help(const char* a_text)
		{
			if (a_text && ImGuiMCP::IsItemHovered()) {
				ImGuiMCP::SetTooltip("%s", a_text);
			}
		}

		void Toggle(const char* a_label, bool& a_value, const char* a_help = nullptr)
		{
			if (ImGuiMCP::Checkbox(a_label, std::addressof(a_value))) {
				Settings::Get().Save();
			}
			Help(a_help);
		}

		RE::Actor* Target(bool a_selected)
		{
			if (a_selected) {
				if (auto ref = RE::Console::GetSelectedRef(); ref) {
					return ref->As<RE::Actor>();
				}
				return nullptr;
			}
			return RE::PlayerCharacter::GetSingleton();
		}

		void __stdcall Settings_()
		{
			auto& s = Settings::Get();
			if (Manager::Blocked()) {
				ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", Manager::BlockReason().data());
			}
			if (!Skee::Ready()) {
				ImGuiMCP::TextColored(ImGuiMCP::ImVec4(1.0f, 0.45f, 0.35f, 1.0f), "%s", Skee::Status().data());
			}

			ImGuiMCP::SeparatorText("Look");
			if (ImGuiMCP::SliderFloat("Opacity", std::addressof(s.opacity), 0.1f, 1.0f, "%.2f")) {
				s.Save();
			}
			Help("Opacity of new cum layers.");
			if (ImGuiMCP::SliderFloat("Wet shine: tightness", std::addressof(s.glossiness), 0.0f, 300.0f, "%.0f")) {
				s.Save();
			}
			Help("Specular power of new layers. Higher = smaller, sharper highlight. 0 = same as the skin.");
			if (ImGuiMCP::SliderFloat("Wet shine: strength", std::addressof(s.specular), 0.0f, 10.0f, "%.1f")) {
				s.Save();
			}
			Help("Specular strength of new layers. 0 = same as the skin.");
			if (ImGuiMCP::SliderFloat("Dries after (game hours)", std::addressof(s.dryHours), 0.0f, 168.0f, "%.0f")) {
				s.Save();
			}
			Help("0 = stays until washed or cleared by SexLab.");
			if (ImGuiMCP::SliderFloat("Fade-out (seconds)", std::addressof(s.fadeSeconds), 0.0f, 5.0f, "%.1f")) {
				s.Save();
			}

			ImGuiMCP::SeparatorText("Layers per zone (0 = off)");
			static constexpr std::array<const char*, kZoneCount> kLabels{ "Vaginal", "Anal", "Breast", "Face", "Hands", "Feet", "Legs (footjob)" };
			for (std::size_t z = 0; z < kZoneCount; ++z) {
				ImGuiMCP::SetNextItemWidth(200.0f);
				if (ImGuiMCP::SliderInt(kLabels[z], std::addressof(s.maxLayers[z]), 0, 12)) {
					s.Save();
				}
			}

			ImGuiMCP::SeparatorText("Who and when");
			Toggle("Male targets", s.maleTargets, "Men can receive cum layers too (by default only women, futa and female creatures).");
			Toggle("Follow SexLab cleanup", s.followSexLabCleanup, "Off (default): every zone dries on Afterglow's own timer above. On: when SexLab P+ removes its own cum (swimming, its fCumTimer), Afterglow also removes that zone - SexLab only knows vaginal, anal and oral, so breast, hands and feet still use Afterglow's timer.");
			Toggle("Wash with Bathing in Skyrim / Dirt and Blood", s.washOnBathing);
			Toggle("Wash when swimming", s.washOnSwimming, "An actor with Afterglow layers who swims (on the surface or underwater) loses every layer within about 2 seconds. Wading or standing in water does not count - use Bathing in Skyrim for that.");
			bool hide = s.hideSexLabOverlays;
			if (ImGuiMCP::Checkbox("Hide SexLab P+'s own cum overlays", std::addressof(hide))) {
				s.hideSexLabOverlays = hide;
				s.Save();
				SKSE::GetTaskInterface()->AddTask([]() { SexLabAlpha::Sync(); });
			}
			Help("Sets SexLab P+'s fCumAlpha to 0, so only Afterglow draws. SexLab's spells and cum counts keep working. Turning it off restores your old value.");
			static const char* kFallback[] = { "Vaginal", "Anal", "Oral" };
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::Combo("Untagged scene fallback", std::addressof(s.fallbackZone), kFallback, 3)) {
				s.Save();
			}
			Help("A scene without cum tags where SexLab could not tell the type either (mostly creatures).");
			ImGuiMCP::SetNextItemWidth(200.0f);
			if (ImGuiMCP::SliderInt("First overlay slot", std::addressof(s.startSlot), 0, 40)) {
				s.Save();
			}
			Help("Afterglow only uses free slots from here on and never paints over tattoos or other mods' overlays.");
			Toggle("Debug log", s.debugLog, "Every layer in Afterglow.log.");
			spdlog::default_logger()->set_level(s.debugLog ? spdlog::level::debug : spdlog::level::info);
		}

		void TestRow(const char* a_who, bool a_selected)
		{
			ImGuiMCP::PushID(a_who);
			ImGuiMCP::TextDisabled("%s", a_who);
			struct B
			{
				const char* label;
				ZoneSet     zones;
			};
			static constexpr std::array<B, 7> kButtons{ { { "Vaginal", Bit(Zone::Vaginal) }, { "Anal", Bit(Zone::Anal) }, { "Oral", Bit(Zone::Face) | Bit(Zone::Breast) },
				{ "Breast", Bit(Zone::Breast) }, { "Hands", Bit(Zone::Hands) }, { "Feet", Bit(Zone::Feet) | Bit(Zone::Legs) }, { "All", ~ZoneSet{ 0 } } } };
			for (const auto& b : kButtons) {
				ImGuiMCP::SameLine();
				if (ImGuiMCP::Button(b.label)) {
					Manager::Test(Target(a_selected), b.zones);
				}
			}
			ImGuiMCP::SameLine();
			if (ImGuiMCP::Button("Clear")) {
				Manager::ClearAll(Target(a_selected));
			}
			ImGuiMCP::PopID();
		}

		void __stdcall Status()
		{
			ImGuiMCP::SeparatorText("Status");
			ImGuiMCP::TextWrapped("%s", Skee::Status().data());
			ImGuiMCP::TextWrapped("Actors with cum: %zu", Manager::TrackedActors());
			ImGuiMCP::TextWrapped("%s", Manager::Describe(Target(false)).c_str());
			if (auto* sel = Target(true); sel && sel != Target(false)) {
				ImGuiMCP::TextWrapped("%s", Manager::Describe(sel).c_str());
			}
			ImGuiMCP::SeparatorText("Test");
			TestRow("Player", false);
			TestRow("Console selection", true);
		}
	}

	void Register()
	{
		if (!SKSEMenuFramework::IsInstalled()) {
			logger::warn("SKSE Menu Framework not installed: no settings page (Afterglow.ini still works)");
			return;
		}
		SKSEMenuFramework::SetSection("Afterglow");
		SKSEMenuFramework::AddSectionItem("Settings", Settings_);
		SKSEMenuFramework::AddSectionItem("Status and test", Status);
	}
}
