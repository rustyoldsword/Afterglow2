#include "core/Core.h"

#include <algorithm>
#include <cstring>

namespace AG
{
	namespace
	{
		struct ZoneInfo
		{
			Area             area;
			std::string_view name;
			std::string_view folder;
			std::string_view prefix;
			int              count;
		};

		constexpr std::array<ZoneInfo, kZoneCount> kZones{ {
			{ Area::Body, "Vaginal", "Vaginal", "body", 12 },
			{ Area::Body, "Anal", "Anal", "body", 12 },
			{ Area::Body, "Breast", "Breast", "chest", 12 },
			{ Area::Face, "Face", "Oral", "head", 12 },
			{ Area::Hands, "Hands", "Handjob", "hands", 4 },
			{ Area::Feet, "Feet", "Footjob", "feet", 4 },
			{ Area::Body, "Legs", "Footjob", "legs", 4 },
		} };

		const ZoneInfo& Info(Zone a_zone) { return kZones[static_cast<std::size_t>(a_zone)]; }

		std::string Lower(std::string_view a_text)
		{
			std::string out(a_text);
			for (auto& c : out) {
				if (c >= 'A' && c <= 'Z') {
					c = static_cast<char>(c - 'A' + 'a');
				} else if (c == '/') {
					c = '\\';
				}
			}
			return out;
		}
	}

	Area             AreaOf(Zone a_zone) { return Info(a_zone).area; }
	std::string_view ZoneName(Zone a_zone) { return Info(a_zone).name; }
	std::string_view ZoneFolder(Zone a_zone) { return Info(a_zone).folder; }
	std::string_view ZonePrefix(Zone a_zone) { return Info(a_zone).prefix; }
	int              TextureCount(Zone a_zone) { return Info(a_zone).count; }

	std::string_view AreaName(Area a_area)
	{
		switch (a_area) {
		case Area::Body:
			return "Body";
		case Area::Face:
			return "Face";
		case Area::Hands:
			return "Hands";
		case Area::Feet:
			return "Feet";
		default:
			return "";
		}
	}

	ZoneSet ZonesForTags(std::uint32_t a_tagMask)
	{
		ZoneSet z = 0;
		if (a_tagMask & kTagVaginal) {
			z |= Bit(Zone::Vaginal);
		}
		if (a_tagMask & kTagAnal) {
			z |= Bit(Zone::Anal);
		}
		if (a_tagMask & kTagOral) {
			z |= Bit(Zone::Face) | Bit(Zone::Breast);
		}
		if (a_tagMask & kTagBreast) {
			z |= Bit(Zone::Breast);
		}
		if (a_tagMask & kTagHands) {
			z |= Bit(Zone::Hands);
		}
		if (a_tagMask & kTagFeet) {
			z |= Bit(Zone::Feet) | Bit(Zone::Legs);
		}
		return z;
	}

	ZoneSet ZonesForCumType(int a_type)
	{
		switch (a_type) {
		case 0:
			return Bit(Zone::Vaginal);
		case 1:
			return Bit(Zone::Anal);
		case 2:
			return Bit(Zone::Face) | Bit(Zone::Breast);
		case -1:
			return Bit(Zone::Vaginal) | Bit(Zone::Anal) | Bit(Zone::Face) | Bit(Zone::Breast);
		default:
			return 0;
		}
	}

	ZoneSet ClassicAddedZones(CumLevels a_before, CumLevels a_after)
	{
		const std::array<std::pair<std::uint8_t, std::uint8_t>, 3> levels{ {
			{ a_before.vaginal, a_after.vaginal },  // P+ type 0
			{ a_before.anal, a_after.anal },        // 1
			{ a_before.oral, a_after.oral },        // 2
		} };
		ZoneSet rose = 0;
		ZoneSet capped = 0;
		int     cappedCount = 0;
		for (int type = 0; type < 3; ++type) {
			const auto [before, after] = levels[type];
			if (after > before) {
				rose |= ZonesForCumType(type);
			}
			if (after >= 2) {
				capped |= ZonesForCumType(type);
				++cappedCount;
			}
		}
		if (rose != 0) {
			return rose;
		}
		return cappedCount == 1 ? capped : 0;
	}

	bool IsSource(int a_sex) { return a_sex == 0 || a_sex == 2 || a_sex == 3; }

	bool IsTarget(int a_sex, bool a_allowMaleTargets)
	{
		if (a_sex == 1 || a_sex == 2 || a_sex == 4) {
			return true;
		}
		return a_allowMaleTargets && (a_sex == 0 || a_sex == 3);
	}

	SlotState ClassifySlotTexture(std::string_view a_texturePath)
	{
		// SKEE returns the live node texture wrapped in quotes ("...\default.dds")
		while (!a_texturePath.empty() && (a_texturePath.front() == '"' || a_texturePath.front() == ' ')) {
			a_texturePath.remove_prefix(1);
		}
		while (!a_texturePath.empty() && (a_texturePath.back() == '"' || a_texturePath.back() == ' ')) {
			a_texturePath.remove_suffix(1);
		}
		if (a_texturePath.empty()) {
			return SlotState::Free;
		}
		const auto p = Lower(a_texturePath);
		if (p.ends_with("actors\\character\\overlays\\default.dds")) {
			return SlotState::Free;
		}
		if (p.find("textures\\afterglow\\") != std::string::npos || p.starts_with("afterglow\\")) {
			return SlotState::Ours;
		}
		return SlotState::Foreign;
	}

	int PickFreeSlot(std::span<const SlotState> a_slots, int a_start)
	{
		for (int i = std::max(a_start, 0); i < static_cast<int>(a_slots.size()); ++i) {
			if (a_slots[i] == SlotState::Free) {
				return i;
			}
		}
		return -1;
	}

	int PickTextureIndex(int a_count, std::span<const int> a_inUse, std::uint32_t a_roll)
	{
		if (a_count <= 0) {
			return 0;
		}
		std::vector<int> free;
		free.reserve(a_count);
		for (int i = 1; i <= a_count; ++i) {
			if (std::find(a_inUse.begin(), a_inUse.end(), i) == a_inUse.end()) {
				free.push_back(i);
			}
		}
		if (free.empty()) {
			return 1 + static_cast<int>(a_roll % static_cast<std::uint32_t>(a_count));
		}
		return free[a_roll % free.size()];
	}

	std::string TexturePath(bool a_ube, Zone a_zone, int a_index, bool a_mega)
	{
		return std::string("textures\\Afterglow\\") + (a_ube ? "UBE" : "CBBE") + "\\" + std::string(ZoneFolder(a_zone)) + "\\" +
		       (a_mega ? "m_" : "") + std::string(ZonePrefix(a_zone)) + std::to_string(a_index) + ".dds";
	}

	std::string NormalPath(std::string_view a_diffuse)
	{
		if (a_diffuse.size() < 4) {
			return std::string(a_diffuse);
		}
		return std::string(a_diffuse.substr(0, a_diffuse.size() - 4)) + "_n.dds";
	}

	bool ActorState::Empty() const { return LayerCount() == 0; }

	std::size_t ActorState::LayerCount() const
	{
		std::size_t n = 0;
		for (const auto& z : zones) {
			n += z.size();
		}
		return n;
	}

	std::vector<int> ActorState::IndicesInUse(Zone a_zone) const
	{
		std::vector<int> out;
		for (const auto& l : Of(a_zone)) {
			out.push_back(l.index);
		}
		return out;
	}

	AddDecision DecideAdd(const ActorState& a_state, Zone a_zone, int a_maxLayers, int a_freeSlot)
	{
		AddDecision d;
		if (a_maxLayers <= 0) {
			return d;
		}
		const auto& layers = a_state.Of(a_zone);
		const bool  atCap = static_cast<int>(layers.size()) >= a_maxLayers;
		if (!atCap && a_freeSlot >= 0) {
			d.kind = AddDecision::Kind::UseFreeSlot;
			d.slot = a_freeSlot;
			return d;
		}
		if (layers.empty()) {
			return d;  // Skip: nothing of ours to reuse, foreign slots are never touched
		}
		int oldest = 0;
		for (int i = 1; i < static_cast<int>(layers.size()); ++i) {
			if (layers[i].appliedAt < layers[oldest].appliedAt) {
				oldest = i;
			}
		}
		d.kind = AddDecision::Kind::ReplaceOldest;
		d.slot = layers[oldest].slot;
		d.replaceAt = oldest;
		return d;
	}

	std::vector<std::pair<Zone, int>> ExpiredLayers(const ActorState& a_state, float a_now, float a_dryDays)
	{
		std::vector<std::pair<Zone, int>> out;
		if (a_dryDays <= 0.0f) {
			return out;
		}
		for (std::size_t z = 0; z < kZoneCount; ++z) {
			const auto& layers = a_state.zones[z];
			for (int i = static_cast<int>(layers.size()) - 1; i >= 0; --i) {
				if (a_now - layers[i].appliedAt > a_dryDays) {
					out.emplace_back(static_cast<Zone>(z), i);
				}
			}
		}
		return out;
	}

	// ------------------------------------------------------------------ save format
	namespace
	{
		class Writer
		{
		public:
			template <class T>
			void Put(T a_value)
			{
				const auto* p = reinterpret_cast<const std::uint8_t*>(&a_value);
				bytes.insert(bytes.end(), p, p + sizeof(T));
			}
			std::vector<std::uint8_t> bytes;
		};

		class Reader
		{
		public:
			explicit Reader(std::span<const std::uint8_t> a_bytes) :
				_bytes(a_bytes) {}

			template <class T>
			bool Get(T& a_out)
			{
				if (_pos + sizeof(T) > _bytes.size()) {
					return false;
				}
				std::memcpy(&a_out, _bytes.data() + _pos, sizeof(T));
				_pos += sizeof(T);
				return true;
			}
			bool AtEnd() const { return _pos == _bytes.size(); }

		private:
			std::span<const std::uint8_t> _bytes;
			std::size_t                   _pos{ 0 };
		};
	}

	std::vector<std::uint8_t> Serialize(const std::vector<std::pair<std::uint32_t, ActorState>>& a_actors)
	{
		Writer w;
		w.Put(static_cast<std::uint32_t>(a_actors.size()));
		for (const auto& [formID, st] : a_actors) {
			w.Put(formID);
			w.Put(static_cast<std::uint8_t>(st.ube));
			for (const auto& zone : st.zones) {
				w.Put(static_cast<std::uint8_t>(std::min<std::size_t>(zone.size(), 255)));
				for (std::size_t i = 0; i < zone.size() && i < 255; ++i) {
					w.Put(zone[i].slot);
					w.Put(zone[i].index);
					w.Put(static_cast<std::uint8_t>(zone[i].mega));
					w.Put(zone[i].appliedAt);
				}
			}
		}
		return w.bytes;
	}

	std::optional<std::vector<std::pair<std::uint32_t, ActorState>>> Deserialize(std::span<const std::uint8_t> a_bytes)
	{
		Reader        r(a_bytes);
		std::uint32_t count = 0;
		if (!r.Get(count)) {
			return std::nullopt;
		}
		std::vector<std::pair<std::uint32_t, ActorState>> out;
		for (std::uint32_t a = 0; a < count; ++a) {
			std::uint32_t formID = 0;
			std::uint8_t  ube = 0;
			if (!r.Get(formID) || !r.Get(ube)) {
				return std::nullopt;
			}
			ActorState st;
			st.ube = ube != 0;
			for (auto& zone : st.zones) {
				std::uint8_t n = 0;
				if (!r.Get(n)) {
					return std::nullopt;
				}
				for (std::uint8_t i = 0; i < n; ++i) {
					Layer        l;
					std::uint8_t mega = 0;
					if (!r.Get(l.slot) || !r.Get(l.index) || !r.Get(mega) || !r.Get(l.appliedAt)) {
						return std::nullopt;
					}
					l.mega = mega != 0;
					zone.push_back(l);
				}
			}
			out.emplace_back(formID, std::move(st));
		}
		if (!r.AtEnd()) {
			return std::nullopt;
		}
		return out;
	}

	// ------------------------------------------------------------------ arbiter
	std::vector<Arbiter::Paint> Arbiter::OnOrgasm(std::uint32_t a_source, std::span<const std::uint32_t> a_targets, std::uint32_t a_tagMask, double a_now)
	{
		std::vector<Paint> out;
		const ZoneSet      zones = ZonesForTags(a_tagMask);
		for (auto target : a_targets) {
			if (zones != 0) {
				_pending.erase(target);
				_lastPaint[target] = a_now;
				out.push_back({ target, a_source, zones });
			} else if (auto it = _lastPaint.find(target); it != _lastPaint.end() && a_now - it->second < ownWindow) {
				// SexLab's ApplyCum came first and already painted this orgasm
			} else {
				_pending[target] = { a_source, a_now + waitSeconds };
			}
		}
		return out;
	}

	std::optional<Arbiter::Paint> Arbiter::OnApplyCum(std::uint32_t a_target, int a_type, std::uint32_t a_tagMask, double a_now)
	{
		return OnCumZones(a_target, ZonesForCumType(a_type), a_tagMask, a_now);
	}

	std::optional<Arbiter::Paint> Arbiter::OnCumZones(std::uint32_t a_target, ZoneSet a_zones, std::uint32_t a_tagMask, double a_now)
	{
		if (a_tagMask != 0) {
			return std::nullopt;  // the scene has cum tags: its orgasm paints (with hands/feet/breast)
		}
		const ZoneSet zones = a_zones;
		if (zones == 0) {
			return std::nullopt;
		}
		if (auto it = _lastPaint.find(a_target); it != _lastPaint.end() && a_now - it->second < ownWindow) {
			return std::nullopt;
		}
		std::uint32_t source = 0;
		if (auto it = _pending.find(a_target); it != _pending.end()) {
			source = it->second.source;
			_pending.erase(it);
		}
		_lastPaint[a_target] = a_now;
		return Paint{ a_target, source, zones };
	}

	std::vector<Arbiter::Paint> Arbiter::Expire(double a_now)
	{
		std::vector<Paint> out;
		for (auto it = _pending.begin(); it != _pending.end();) {
			if (a_now >= it->second.deadline) {
				out.push_back({ it->first, it->second.source, fallbackZones });
				it = _pending.erase(it);
			} else {
				++it;
			}
		}
		// forget old paint stamps so the map does not grow forever
		std::erase_if(_lastPaint, [&](const auto& kv) { return a_now - kv.second > ownWindow * 4; });
		return out;
	}

	void Arbiter::Clear()
	{
		_pending.clear();
		_lastPaint.clear();
	}
}
