#pragma once

#include "core/Core.h"

namespace AG::Manager
{
	void Start();  // after data loaded: texture cache + ticker thread
	bool Blocked();  // the old Afterglow (SLACS_2_Plus_SCO.esp) is active: draw nothing
	std::string_view BlockReason();

	// From the Papyrus bridge (any thread).
	void OnOrgasm(RE::Actor* a_source, int a_sourceSex, const std::vector<RE::Actor*>& a_partners, const std::vector<std::int32_t>& a_sexes, std::uint32_t a_tagMask);
	void OnApplyCum(RE::Actor* a_target, int a_type, std::uint32_t a_tagMask, int a_targetSex);
	void OnCumZones(RE::Actor* a_target, ZoneSet a_zones, std::uint32_t a_tagMask, int a_targetSex);  // any thread
	void OnSexLabClear(RE::Actor* a_target, int a_type);
	void Wash(RE::Actor* a_target);  // bathing mods, console, menu

	// Menu / console helpers.
	void Test(RE::Actor* a_target, ZoneSet a_zones);
	void ClearAll(RE::Actor* a_target);
	std::size_t TrackedActors();
	std::string Describe(RE::Actor* a_actor);

	// SKSE co-save
	void Save(SKSE::SerializationInterface* a_intfc);
	void Load(SKSE::SerializationInterface* a_intfc);
	void Revert(SKSE::SerializationInterface* a_intfc);
}
