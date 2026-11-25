#include "Papyrus.h"

#include "Manager.h"
#include "Settings.h"
#include "SexLabAlpha.h"

// Natives of `Scriptname Afterglow Hidden` (papyrus/Source/Scripts/Afterglow.psc).
namespace AG::Papyrus
{
	namespace
	{
		constexpr auto kClass = "Afterglow";

		void OnOrgasm(RE::StaticFunctionTag*, RE::Actor* a_source, std::int32_t a_sourceSex, std::vector<RE::Actor*> a_partners, std::vector<std::int32_t> a_sexes, std::int32_t a_tagMask)
		{
			Manager::OnOrgasm(a_source, a_sourceSex, a_partners, a_sexes, static_cast<std::uint32_t>(a_tagMask));
		}

		void OnApplyCum(RE::StaticFunctionTag*, RE::Actor* a_target, std::int32_t a_type, std::int32_t a_tagMask, std::int32_t a_targetSex)
		{
			Manager::OnApplyCum(a_target, a_type, static_cast<std::uint32_t>(a_tagMask), a_targetSex);
		}

		void OnSexLabClear(RE::StaticFunctionTag*, RE::Actor* a_target, std::int32_t a_type) { Manager::OnSexLabClear(a_target, a_type); }

		void OnWashed(RE::StaticFunctionTag*, RE::Actor* a_target)
		{
			if (Settings::Get().washOnBathing) {
				Manager::Wash(a_target);
			}
		}

		// Afterglow_PlayerAlias registered the washing events (new game and every load). The alias names
		// no SexLab type, so this line also shows up under OStim alone.
		void OnBathingListener(RE::StaticFunctionTag*, bool a_bathingInSkyrim, bool a_dirtAndBlood)
		{
			logger::info("bathing listener ready: Bathing in Skyrim {}, Dirt and Blood {}, wash {}",
				a_bathingInSkyrim ? "found" : "not found", a_dirtAndBlood ? "found" : "not found",
				Settings::Get().washOnBathing ? "on" : "off");
		}

		void Clear(RE::StaticFunctionTag*, RE::Actor* a_target) { Manager::Wash(a_target); }

		void OnGameLoaded(RE::StaticFunctionTag*) { SexLabAlpha::Sync(); }

		std::int32_t GetVersion(RE::StaticFunctionTag*) { return 20003; }

		bool Register(RE::BSScript::IVirtualMachine* a_vm)
		{
			a_vm->RegisterFunction("OnOrgasm", kClass, OnOrgasm);
			a_vm->RegisterFunction("OnApplyCum", kClass, OnApplyCum);
			a_vm->RegisterFunction("OnSexLabClear", kClass, OnSexLabClear);
			a_vm->RegisterFunction("OnWashed", kClass, OnWashed);
			a_vm->RegisterFunction("OnBathingListener", kClass, OnBathingListener);
			a_vm->RegisterFunction("Clear", kClass, Clear);
			a_vm->RegisterFunction("OnGameLoaded", kClass, OnGameLoaded);
			a_vm->RegisterFunction("GetVersion", kClass, GetVersion);
			logger::info("papyrus natives registered");
			return true;
		}
	}

	void Init()
	{
		SKSE::GetPapyrusInterface()->Register(Register);
	}
}
