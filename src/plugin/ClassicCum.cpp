#include "ClassicCum.h"

#include "Manager.h"
#include "Settings.h"
#include "SexLabAlpha.h"

namespace AG::ClassicCum
{
	namespace
	{
		std::atomic_bool g_active{ false };
		RE::TESFaction*  g_animating{ nullptr };  // SexLabAnimatingFaction, the same id in Classic and P+

		// SexLab's cum keywords (Classic sslSystemConfig.Cum*Keyword); editor ids, so no form id is assumed
		void Raise(std::uint8_t& a_level, std::uint8_t a_to) { a_level = std::max(a_level, a_to); }

		bool LevelsOf(const RE::EffectSetting* a_effect, CumLevels& a_out)
		{
			bool cum = false;
			if (!a_effect) {
				return false;
			}
			for (std::uint32_t i = 0; i < a_effect->numKeywords; ++i) {
				const auto* kw = a_effect->keywords ? a_effect->keywords[i] : nullptr;
				if (!kw) {
					continue;
				}
				const char*            raw = kw->GetFormEditorID();
				const std::string_view id = raw ? raw : "";
				if (id == "SexLabVaginalCum") {
					Raise(a_out.vaginal, 1);
				} else if (id == "SexLabVaginalStackedCum") {
					Raise(a_out.vaginal, 2);
				} else if (id == "SexLabAnalCum") {
					Raise(a_out.anal, 1);
				} else if (id == "SexLabAnalStackedCum") {
					Raise(a_out.anal, 2);
				} else if (id == "SexLabOralCum") {
					Raise(a_out.oral, 1);
				} else if (id == "SexLabOralStackedCum") {
					Raise(a_out.oral, 2);
				} else {
					continue;
				}
				cum = true;
			}
			return cum;
		}

		// The levels the actor had before this cast. The apply event comes before the new effect is
		// added and before it dispels the old one; should the new one already be in the list, it is
		// skipped (same base effect, no time elapsed).
		CumLevels LevelsBefore(RE::Actor* a_actor, const RE::EffectSetting* a_applying)
		{
			CumLevels out;
			auto*     target = a_actor->AsMagicTarget();
			auto*     list = target ? target->GetActiveEffectList() : nullptr;
			if (!list) {
				return out;
			}
			for (auto* ae : *list) {
				if (!ae || ae->flags.any(RE::ActiveEffect::Flag::kDispelled)) {
					continue;
				}
				const auto* base = ae->GetBaseObject();
				if (base == a_applying && ae->elapsedSeconds <= 0.0f) {
					continue;
				}
				CumLevels l;
				if (LevelsOf(base, l)) {
					Raise(out.vaginal, l.vaginal);
					Raise(out.anal, l.anal);
					Raise(out.oral, l.oral);
				}
			}
			return out;
		}

		// Afterglow sex codes (= P+): 0 male, 1 female, 3 creature male, 4 creature female
		int SexOf(RE::Actor* a_actor)
		{
			const auto* base = a_actor->GetActorBase();
			const bool  female = base && base->GetSex() == RE::SEXES::kFemale;
			if (a_actor->HasKeywordString("ActorTypeNPC")) {
				return female ? 1 : 0;
			}
			return female ? 4 : 3;
		}

		class Sink : public RE::BSTEventSink<RE::TESMagicEffectApplyEvent>
		{
		public:
			RE::BSEventNotifyControl ProcessEvent(const RE::TESMagicEffectApplyEvent* a_event, RE::BSTEventSource<RE::TESMagicEffectApplyEvent>*) override
			{
				if (!g_active || !a_event || !a_event->target) {
					return RE::BSEventNotifyControl::kContinue;
				}
				const auto* effect = RE::TESForm::LookupByID<RE::EffectSetting>(a_event->magicEffect);
				CumLevels   after;
				if (!LevelsOf(effect, after)) {
					return RE::BSEventNotifyControl::kContinue;
				}
				auto* actor = a_event->target->As<RE::Actor>();
				if (!actor) {
					return RE::BSEventNotifyControl::kContinue;
				}
				if (g_animating && actor->IsInFaction(g_animating)) {
					logger::debug("SexLab Classic cum spell on {:08X} inside a scene: left to the orgasm", actor->GetFormID());
					return RE::BSEventNotifyControl::kContinue;
				}
				const CumLevels before = LevelsBefore(actor, effect);
				const ZoneSet   zones = ClassicAddedZones(before, after);
				logger::info("SexLab Classic cum spell on {:08X} outside a scene: levels vag {} anal {} oral {} -> {} {} {}, zones {:#x}",
					actor->GetFormID(), before.vaginal, before.anal, before.oral, after.vaginal, after.anal, after.oral, zones);
				if (zones != 0) {
					Manager::OnCumZones(actor, zones, 0, SexOf(actor));
				}
				return RE::BSEventNotifyControl::kContinue;
			}
		};
	}

	void Register()
	{
		static Sink sink;
		if (auto* dh = RE::TESDataHandler::GetSingleton()) {
			g_animating = dh->LookupForm<RE::TESFaction>(0x00E50F, "SexLab.esm");
		}
		if (auto* events = RE::ScriptEventSourceHolder::GetSingleton()) {
			events->AddEventSink<RE::TESMagicEffectApplyEvent>(&sink);
		}
	}

	void Sync()
	{
		const int  mode = Settings::Get().classicCumSpells;
		const bool classic = SexLabAlpha::Detect() == SexLabAlpha::Kind::Classic;
		const bool on = mode == 1 || (mode == -1 && classic);
		g_active = on;
		if (on) {
			logger::info("SexLab cum spells outside a scene (Fill Her Up expelling...) add a layer ({})", classic ? "SexLab Classic" : "forced by iClassicCumSpells = 1");
		} else if (classic) {
			logger::info("SexLab Classic cum spells are not watched (iClassicCumSpells = 0)");
		}
	}
}
