#include "Manager.h"

#include "Body.h"
#include "Settings.h"
#include "Skee.h"

namespace AG::Manager
{
	namespace
	{
		struct Fade
		{
			RE::FormID actor;
			bool       female;
			Area       area;
			int        slot;
			float      startAlpha;
			double     start;
			double     duration;
		};

		std::mutex                                   g_lock;  // guards everything below
		std::unordered_map<RE::FormID, ActorState>   g_states;
		std::vector<Fade>                            g_fades;
		Arbiter                                      g_arbiter;
		std::mt19937                                 g_rng{ std::random_device{}() };

		std::unordered_map<std::string, bool> g_texExists;  // filled once in Start()
		std::atomic_bool                      g_taskQueued{ false };
		std::atomic_bool                      g_running{ false };
		bool                                  g_blocked{ false };
		std::string                           g_blockReason;

		constexpr std::uint32_t kRecord = 'STAT';

		double RealNow()
		{
			using namespace std::chrono;
			return duration<double>(steady_clock::now().time_since_epoch()).count();
		}

		float GameNow()
		{
			const auto* cal = RE::Calendar::GetSingleton();
			return cal ? cal->GetCurrentGameTime() : 0.0f;
		}

		std::string Name(RE::Actor* a_actor)
		{
			if (!a_actor) {
				return "None";
			}
			const char* n = a_actor->GetDisplayFullName();
			return std::format("{} ({:08X})", n && *n ? n : "?", a_actor->GetFormID());
		}

		bool Female(RE::Actor* a_actor)
		{
			const auto* base = a_actor ? a_actor->GetActorBase() : nullptr;
			return base && base->GetSex() == RE::SEXES::kFemale;
		}

		bool TextureExists(const std::string& a_path)
		{
			if (auto it = g_texExists.find(a_path); it != g_texExists.end()) {
				return it->second;
			}
			// the game's resource system: loose files AND archives (the textures ship in Afterglow*.bsa).
			// std::filesystem only sees loose files and would call every archived texture missing.
			RE::BSResourceNiBinaryStream stream(a_path);
			const bool                   ok = stream.good();
			g_texExists.emplace(a_path, ok);
			return ok;
		}

		bool IsMega(RE::Actor* a_source)
		{
			if (!a_source) {
				return false;
			}
			const auto* race = a_source->GetRace();
			if (!race) {
				return false;
			}
			const std::string_view edid = race->GetFormEditorID();
			for (const auto& r : Settings::Get().megaRaces) {
				if (r.size() == edid.size() && std::equal(r.begin(), r.end(), edid.begin(), [](char a, char b) { return std::tolower((unsigned char)a) == std::tolower((unsigned char)b); })) {
					return true;
				}
			}
			return false;
		}

		void Queue(std::function<void()> a_fn)
		{
			SKSE::GetTaskInterface()->AddTask(std::move(a_fn));
		}

		// ---- main thread only below -------------------------------------------------------

		void FadeOrErase(RE::Actor* a_actor, bool a_female, Area a_area, int a_slot)
		{
			const auto& s = Settings::Get();
			if (s.fadeSeconds <= 0.0f || !a_actor->Is3DLoaded()) {
				Skee::Erase(a_actor, a_female, a_area, a_slot);
				return;
			}
			std::scoped_lock l(g_lock);
			g_fades.push_back({ a_actor->GetFormID(), a_female, a_area, a_slot, s.opacity, RealNow(), s.fadeSeconds });
		}

		void PaintNow(RE::FormID a_target, RE::FormID a_source, ZoneSet a_zones)
		{
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_target);
			if (!actor || !Skee::Ready()) {
				return;
			}
			auto*      source = a_source ? RE::TESForm::LookupByID<RE::Actor>(a_source) : nullptr;
			const auto& s = Settings::Get();
			const bool female = Female(actor);
			const bool ube = Body::IsUBE(actor);
			const bool mega = IsMega(source);
			const float now = GameNow();
			Skee::EnsureOverlays(actor);

			std::scoped_lock l(g_lock);
			auto&            st = g_states[a_target];
			st.ube = ube;
			for (std::size_t z = 0; z < kZoneCount; ++z) {
				const auto zone = static_cast<Zone>(z);
				if (!Has(a_zones, zone)) {
					continue;
				}
				const Area area = AreaOf(zone);
				const int  n = Skee::NumSlots(area);
				std::vector<SlotState> slots(static_cast<std::size_t>(std::max(n, 0)));
				for (int i = 0; i < n; ++i) {
					slots[i] = ClassifySlotTexture(Skee::SlotTexture(actor, female, area, i));
				}
				const auto d = DecideAdd(st, zone, s.maxLayers[z], PickFreeSlot(slots, s.startSlot));
				if (d.kind == AddDecision::Kind::Skip) {
					if (s.maxLayers[z] > 0) {
						logger::info("{}: no free {} overlay slot from {} to {} for {} - other mods use them all (raise iNumOverlays in skee64.ini)",
							Name(actor), AreaName(area), s.startSlot, n - 1, ZoneName(zone));
					}
					continue;
				}
				auto inUse = st.IndicesInUse(zone);
				if (d.kind == AddDecision::Kind::ReplaceOldest) {
					inUse.erase(inUse.begin() + d.replaceAt);
				}
				const int idx = PickTextureIndex(TextureCount(zone), inUse, g_rng());
				bool      useMega = mega;
				auto      diffuse = TexturePath(ube, zone, idx, useMega);
				if (useMega && !TextureExists(diffuse)) {
					useMega = false;
					diffuse = TexturePath(ube, zone, idx, false);
				}
				if (!TextureExists(diffuse)) {
					logger::warn("{}: texture missing: {}", Name(actor), diffuse);
					continue;
				}
				auto normal = NormalPath(diffuse);
				if (!TextureExists(normal)) {
					normal.clear();
				}
				Skee::Paint(actor, female, area, d.slot, diffuse, normal, s.opacity);
				const Layer layer{ static_cast<std::uint8_t>(d.slot), static_cast<std::uint8_t>(idx), useMega, now };
				if (d.kind == AddDecision::Kind::ReplaceOldest) {
					st.Of(zone)[d.replaceAt] = layer;
				} else {
					st.Of(zone).push_back(layer);
				}
				logger::debug("{}: {} layer -> {} [Ovl{}] {}", Name(actor), ZoneName(zone), AreaName(area), d.slot, diffuse);
			}
			logger::info("painted {} (source {}, {}, {} layers)", Name(actor), source ? Name(source) : "none", ube ? "UBE" : "CBBE/3BA", st.LayerCount());
		}

		// Removes the given zones (all when a_zones == ~0) with the fade.
		void RemoveNow(RE::FormID a_target, ZoneSet a_zones, std::string_view a_why)
		{
			auto* actor = RE::TESForm::LookupByID<RE::Actor>(a_target);
			if (!actor) {
				std::scoped_lock l(g_lock);
				g_states.erase(a_target);
				return;
			}
			const bool                          female = Female(actor);
			std::vector<std::pair<Area, int>>   slots;
			{
				std::scoped_lock l(g_lock);
				auto             it = g_states.find(a_target);
				if (it == g_states.end()) {
					return;
				}
				for (std::size_t z = 0; z < kZoneCount; ++z) {
					if (!Has(a_zones, static_cast<Zone>(z))) {
						continue;
					}
					for (const auto& layer : it->second.zones[z]) {
						slots.emplace_back(AreaOf(static_cast<Zone>(z)), layer.slot);
					}
					it->second.zones[z].clear();
				}
				if (it->second.Empty()) {
					g_states.erase(it);
				}
			}
			for (auto [area, slot] : slots) {
				FadeOrErase(actor, female, area, slot);
			}
			if (!slots.empty()) {
				logger::info("removed {} layer(s) from {} ({})", slots.size(), Name(actor), a_why);
			}
		}

		void DryNow()
		{
			const float dryDays = Settings::Get().dryHours / 24.0f;
			if (dryDays <= 0.0f) {
				return;
			}
			const float now = GameNow();
			std::vector<std::tuple<RE::FormID, Area, int>> gone;
			{
				std::scoped_lock l(g_lock);
				for (auto it = g_states.begin(); it != g_states.end();) {
					for (auto [zone, pos] : ExpiredLayers(it->second, now, dryDays)) {
						auto& layers = it->second.Of(zone);
						gone.emplace_back(it->first, AreaOf(zone), layers[pos].slot);
						layers.erase(layers.begin() + pos);
					}
					it = it->second.Empty() ? g_states.erase(it) : std::next(it);
				}
			}
			for (auto [id, area, slot] : gone) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(id)) {
					FadeOrErase(actor, Female(actor), area, slot);
				}
			}
			if (!gone.empty()) {
				logger::info("{} layer(s) dried off", gone.size());
			}
		}

		// Swimming washes like a bath (OCum does the same). Bathing in Skyrim sends no wash event when the
		// actor bathes while swimming or underwater (it skips the animation), so this also covers that case.
		void SwimNow()
		{
			if (!Settings::Get().washOnSwimming) {
				return;
			}
			std::vector<RE::FormID> ids;
			{
				std::scoped_lock l(g_lock);
				ids.reserve(g_states.size());
				for (const auto& [id, st] : g_states) {
					ids.push_back(id);
				}
			}
			for (auto id : ids) {
				auto*       actor = RE::TESForm::LookupByID<RE::Actor>(id);
				const auto* state = actor && actor->Is3DLoaded() ? actor->AsActorState() : nullptr;
				if (state && state->IsSwimming()) {
					RemoveNow(id, ~ZoneSet{ 0 }, "swimming");
				}
			}
		}

		// Mu Dynamic NormalMap guard. The NoDynamicNormalMap mark lives on the overlay 3D, which SKEE rebuilds
		// (armor swap, cell load, save load) and Mu then covers with its skin map again. Every pass: re-mark loaded
		// slots that lost it, and for a few seconds after re-marking put our normal back (a Mu update queued
		// before the mark may still land).
		struct Reshow
		{
			RE::FormID  actor;
			Area        area;
			int         slot;
			std::string normal;
			double      until;
		};
		std::vector<Reshow> g_reshow;  // main thread only

		void GuardNow(double a_now)
		{
			std::vector<std::tuple<RE::FormID, Area, int, std::string>> painted;
			{
				std::scoped_lock l(g_lock);
				for (const auto& [id, st] : g_states) {
					for (std::size_t z = 0; z < kZoneCount; ++z) {
						for (const auto& layer : st.zones[z]) {
							painted.emplace_back(id, AreaOf(static_cast<Zone>(z)), layer.slot,
								NormalPath(TexturePath(st.ube, static_cast<Zone>(z), layer.index, layer.mega)));
						}
					}
				}
			}
			int remarked = 0;
			for (auto& [id, area, slot, normal] : painted) {
				auto* actor = RE::TESForm::LookupByID<RE::Actor>(id);
				if (!actor || !Skee::ShieldFromMu(actor, area, slot)) {
					continue;
				}
				++remarked;
				if (TextureExists(normal)) {
					Skee::ShowNormal(actor, area, slot, normal);
					g_reshow.push_back({ id, area, slot, normal, a_now + 6.0 });
				}
			}
			if (remarked) {
				logger::info("NoDynamicNormalMap set on {} overlay slot(s) (Mu Dynamic NormalMap keeps off our normal maps)", remarked);
			}
			std::erase_if(g_reshow, [a_now](const Reshow& r) { return r.until < a_now; });
			for (const auto& r : g_reshow) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(r.actor)) {
					Skee::ShowNormal(actor, r.area, r.slot, r.normal);
				}
			}
		}

		void StepFades(double a_now)
		{
			std::vector<Fade> done;
			std::vector<std::pair<Fade, float>> live;
			{
				std::scoped_lock l(g_lock);
				for (auto it = g_fades.begin(); it != g_fades.end();) {
					const double t = (a_now - it->start) / it->duration;
					if (t >= 1.0) {
						done.push_back(*it);
						it = g_fades.erase(it);
					} else {
						live.emplace_back(*it, static_cast<float>(it->startAlpha * (1.0 - t)));
						++it;
					}
				}
			}
			for (auto& [f, alpha] : live) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(f.actor)) {
					Skee::SetLiveAlpha(actor, f.area, f.slot, alpha);
				}
			}
			for (auto& f : done) {
				if (auto* actor = RE::TESForm::LookupByID<RE::Actor>(f.actor)) {
					Skee::Erase(actor, f.female, f.area, f.slot);
				}
			}
		}

		// Ticker: sleeps, and only posts a game-thread task when there is work. Never a
		// self-requeueing task (that would spin inside one frame).
		void Ticker()
		{
			double lastDry = RealNow();
			double lastGuard = lastDry;
			while (g_running) {
				bool fading = false;
				bool pending = false;
				bool tracked = false;
				{
					std::scoped_lock l(g_lock);
					fading = !g_fades.empty();
					pending = g_arbiter.HasPending();
					tracked = !g_states.empty();
				}
				const double now = RealNow();
				const bool   dry = tracked && now - lastDry >= 10.0;
				if (dry) {
					lastDry = now;
				}
				const bool guard = tracked && now - lastGuard >= 2.0;
				if (guard) {
					lastGuard = now;
				}
				if ((fading || pending || dry || guard) && !g_taskQueued.exchange(true)) {
					Queue([dry, guard]() {
						g_taskQueued = false;
						const double t = RealNow();
						std::vector<Arbiter::Paint> paints;
						{
							std::scoped_lock l(g_lock);
							paints = g_arbiter.Expire(t);
						}
						for (const auto& p : paints) {
							logger::info("untagged scene, no SexLab cum type for {:08X}: fallback zone", p.target);
							PaintNow(p.target, p.source, p.zones);
						}
						StepFades(t);
						if (dry) {
							DryNow();
						}
						if (guard) {
							SwimNow();
							GuardNow(t);
						}
					});
				}
				std::this_thread::sleep_for(fading ? 50ms : 250ms);
			}
		}

		void ApplyArbiterSettings()
		{
			static constexpr std::array<ZoneSet, 3> kFallback{ Bit(Zone::Vaginal), Bit(Zone::Anal), Bit(Zone::Face) | Bit(Zone::Breast) };
			g_arbiter.fallbackZones = kFallback[std::clamp(Settings::Get().fallbackZone, 0, 2)];
		}
	}

	void Start()
	{
		auto* dh = RE::TESDataHandler::GetSingleton();
		if (dh && (dh->LookupLoadedModByName("SLACS_2_Plus_SCO.esp") || dh->LookupLoadedLightModByName("SLACS_2_Plus_SCO.esp"))) {
			g_blocked = true;
			g_blockReason = "The old Afterglow (SLACS_2_Plus_SCO.esp) is active. Disable it - Afterglow 2.0 replaces it and draws nothing while both are on.";
			logger::warn("{}", g_blockReason);
		}
		int missing = 0;
		for (bool ube : { false, true }) {
			for (std::size_t z = 0; z < kZoneCount; ++z) {
				for (int i = 1; i <= TextureCount(static_cast<Zone>(z)); ++i) {
					if (!TextureExists(TexturePath(ube, static_cast<Zone>(z), i, false))) {
						++missing;
					}
				}
			}
		}
		logger::info("textures checked: {} of the base set missing", missing);
		ApplyArbiterSettings();
		if (!g_running.exchange(true)) {
			std::thread(Ticker).detach();
		}
	}

	bool             Blocked() { return g_blocked; }
	std::string_view BlockReason() { return g_blockReason; }

	void OnOrgasm(RE::Actor* a_source, int a_sourceSex, const std::vector<RE::Actor*>& a_partners, const std::vector<std::int32_t>& a_sexes, std::uint32_t a_tagMask)
	{
		if (g_blocked || !a_source) {
			return;
		}
		if (!IsSource(a_sourceSex)) {
			logger::debug("orgasm of {} (sex {}): not a source", Name(a_source), a_sourceSex);
			return;
		}
		const bool               males = Settings::Get().maleTargets;
		std::vector<std::uint32_t> targets;
		for (std::size_t i = 0; i < a_partners.size(); ++i) {
			auto* p = a_partners[i];
			const int sex = i < a_sexes.size() ? a_sexes[i] : -1;
			if (p && p != a_source && IsTarget(sex, males)) {
				targets.push_back(p->GetFormID());
			}
		}
		std::vector<Arbiter::Paint> paints;
		{
			std::scoped_lock l(g_lock);
			ApplyArbiterSettings();
			paints = g_arbiter.OnOrgasm(a_source->GetFormID(), targets, a_tagMask, RealNow());
		}
		logger::info("orgasm of {} (sex {}), tags {:#x}, {} target(s){}", Name(a_source), a_sourceSex, a_tagMask, targets.size(),
			a_tagMask ? "" : " - untagged, waiting for SexLab's cum type");
		for (const auto& p : paints) {
			Queue([p]() { PaintNow(p.target, p.source, p.zones); });
		}
	}

	void OnApplyCum(RE::Actor* a_target, int a_type, std::uint32_t a_tagMask, int a_targetSex)
	{
		OnCumZones(a_target, ZonesForCumType(a_type), a_tagMask, a_targetSex);
	}

	void OnCumZones(RE::Actor* a_target, ZoneSet a_zones, std::uint32_t a_tagMask, int a_targetSex)
	{
		if (g_blocked || !a_target || !IsTarget(a_targetSex, Settings::Get().maleTargets)) {
			return;
		}
		std::optional<Arbiter::Paint> paint;
		{
			std::scoped_lock l(g_lock);
			paint = g_arbiter.OnCumZones(a_target->GetFormID(), a_zones, a_tagMask, RealNow());
		}
		logger::debug("SexLab cum zones {:#x} on {}, tags {:#x}: {}", a_zones, Name(a_target), a_tagMask, paint ? "painting" : "left to the orgasm/tags");
		if (paint) {
			Queue([p = *paint]() { PaintNow(p.target, p.source, p.zones); });
		}
	}

	void OnSexLabClear(RE::Actor* a_target, int a_type)
	{
		if (!a_target || !Settings::Get().followSexLabCleanup) {
			return;
		}
		ZoneSet zones = ZonesForCumType(a_type);
		if (a_type == -1) {
			zones = ~ZoneSet{ 0 };
		}
		const auto id = a_target->GetFormID();
		Queue([id, zones]() { RemoveNow(id, zones, "SexLab cleanup"); });
	}

	void Wash(RE::Actor* a_target)
	{
		if (!a_target) {
			return;
		}
		const auto id = a_target->GetFormID();
		Queue([id]() { RemoveNow(id, ~ZoneSet{ 0 }, "washed"); });
	}

	void Test(RE::Actor* a_target, ZoneSet a_zones)
	{
		if (!a_target || g_blocked) {
			return;
		}
		const auto id = a_target->GetFormID();
		Queue([id, a_zones]() { PaintNow(id, 0, a_zones); });
	}

	void ClearAll(RE::Actor* a_target) { Wash(a_target); }

	std::size_t TrackedActors()
	{
		std::scoped_lock l(g_lock);
		return g_states.size();
	}

	std::string Describe(RE::Actor* a_actor)
	{
		if (!a_actor) {
			return "no actor";
		}
		std::size_t layers = 0;
		{
			std::scoped_lock l(g_lock);
			if (auto it = g_states.find(a_actor->GetFormID()); it != g_states.end()) {
				layers = it->second.LayerCount();
			}
		}
		return std::format("{}: {}; {} layer(s)", Name(a_actor), Body::Describe(a_actor), layers);
	}

	void Save(SKSE::SerializationInterface* a_intfc)
	{
		std::vector<std::pair<std::uint32_t, ActorState>> all;
		{
			std::scoped_lock l(g_lock);
			all.assign(g_states.begin(), g_states.end());
		}
		const auto bytes = Serialize(all);
		const auto size = static_cast<std::uint32_t>(bytes.size());
		if (!a_intfc->OpenRecord(kRecord, kSaveVersion) || !a_intfc->WriteRecordData(size) || (size && !a_intfc->WriteRecordData(bytes.data(), size))) {
			logger::error("save: could not write the state record");
		}
	}

	void Load(SKSE::SerializationInterface* a_intfc)
	{
		std::uint32_t type, version, length;
		while (a_intfc->GetNextRecordInfo(type, version, length)) {
			if (type != kRecord) {
				continue;
			}
			if (version != kSaveVersion) {
				logger::warn("load: unknown state version {}, ignored", version);
				continue;
			}
			std::uint32_t size = 0;
			if (!a_intfc->ReadRecordData(size)) {
				continue;
			}
			std::vector<std::uint8_t> bytes(size);
			if (size && a_intfc->ReadRecordData(bytes.data(), size) != size) {
				logger::error("load: truncated state record");
				continue;
			}
			auto parsed = Deserialize(bytes);
			if (!parsed) {
				logger::error("load: corrupt state record, starting clean");
				continue;
			}
			std::scoped_lock l(g_lock);
			g_states.clear();
			for (auto& [old, st] : *parsed) {
				RE::FormID id = 0;
				if (a_intfc->ResolveFormID(old, id)) {
					g_states[id] = std::move(st);
				}
			}
			logger::info("load: {} actor(s) with cum layers", g_states.size());
		}
	}

	void Revert(SKSE::SerializationInterface*)
	{
		std::scoped_lock l(g_lock);
		g_states.clear();
		g_fades.clear();
		g_arbiter.Clear();
		Body::ClearCache();
	}
}
