#pragma once
// Afterglow 2.0 - game-independent rules. Nothing in here touches CommonLib, so the
// whole file is unit-tested offline (tests/test_core.cpp).

#include <array>
#include <cstdint>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace AG
{
	// ------------------------------------------------------------------ zones
	enum class Zone : std::uint8_t
	{
		Vaginal,
		Anal,
		Breast,
		Face,
		Hands,
		Feet,
		Legs,
		Count
	};
	inline constexpr std::size_t kZoneCount = static_cast<std::size_t>(Zone::Count);

	// SKEE overlay node families ("Body [ovl7]", "Face [ovl7]" ...)
	enum class Area : std::uint8_t
	{
		Body,
		Face,
		Hands,
		Feet,
		Count
	};
	inline constexpr std::size_t kAreaCount = static_cast<std::size_t>(Area::Count);

	Area             AreaOf(Zone a_zone);
	std::string_view AreaName(Area a_area);    // "Body" | "Face" | "Hands" | "Feet"
	std::string_view ZoneName(Zone a_zone);    // for logs and the menu
	std::string_view ZoneFolder(Zone a_zone);  // texture folder: Vaginal Anal Breast Oral Handjob Footjob
	std::string_view ZonePrefix(Zone a_zone);  // file prefix: body chest head hands feet legs
	int              TextureCount(Zone a_zone);  // how many numbered textures a zone ships

	using ZoneSet = std::uint32_t;  // bit (1 << zone)
	constexpr ZoneSet Bit(Zone a_zone) { return ZoneSet{ 1 } << static_cast<std::uint32_t>(a_zone); }
	constexpr bool    Has(ZoneSet a_set, Zone a_zone) { return (a_set & Bit(a_zone)) != 0; }

	// ------------------------------------------------------------------ scene tags
	// The Papyrus bridge folds the scene/stage tags into this mask (one native call per orgasm).
	enum TagBit : std::uint32_t
	{
		kTagVaginal = 1 << 0,  // Vaginal, Creampie, VaginalCum
		kTagAnal = 1 << 1,     // Anal, AnalCreampie, AnalCum
		kTagOral = 1 << 2,     // Oral, Blowjob, Facial, CumInMouth
		kTagBreast = 1 << 3,   // Boobjob, Titjob, Breast, ChestCum
		kTagHands = 1 << 4,    // Handjob, HandsCum
		kTagFeet = 1 << 5,     // Footjob, FeetCum
	};
	ZoneSet ZonesForTags(std::uint32_t a_tagMask);

	// SexLab P+ cum fx type: 0 vaginal, 1 anal, 2 oral, -1 all
	ZoneSet ZonesForCumType(int a_type);

	// SexLab Classic has no "SexLabApplyCum" event. Its AddCum/ApplyCum (Fill Her Up expelling,
	// SexLab's own orgasm) casts one spell whose effect carries the NEW level of every type
	// (0 none, 1, 2 = stacked); the levels the actor already had are carried over, only the
	// requested types rise (capped at 2), and the new effect dispels the old one.
	struct CumLevels
	{
		std::uint8_t vaginal{ 0 };
		std::uint8_t anal{ 0 };
		std::uint8_t oral{ 0 };
	};
	// Zones of the types one cast added: the types whose level rose. When none rose (the requested
	// type was already at 2) and exactly one type is at 2, that one; otherwise nothing (ambiguous).
	ZoneSet ClassicAddedZones(CumLevels a_before, CumLevels a_after);

	// ------------------------------------------------------------------ who paints whom
	// SexLab P+ sex: 0 male, 1 female, 2 futa, 3 creature male, 4 creature female
	bool IsSource(int a_sex);
	bool IsTarget(int a_sex, bool a_allowMaleTargets);

	// ------------------------------------------------------------------ overlay slots
	enum class SlotState : std::uint8_t
	{
		Free,     // no texture, or SKEE's default.dds
		Ours,     // one of our textures
		Foreign,  // a tattoo, SlaveTats, body paint... never touched
	};
	SlotState ClassifySlotTexture(std::string_view a_texturePath);  // "" = Free
	// First free slot at or after a_start, or -1. Foreign and Ours slots are skipped.
	int PickFreeSlot(std::span<const SlotState> a_slots, int a_start);

	// ------------------------------------------------------------------ textures
	// Picks a texture number 1..count that no active layer of the zone uses yet; when all
	// are taken, any number. a_roll is a uniform random value (injected for tests).
	int PickTextureIndex(int a_count, std::span<const int> a_inUse, std::uint32_t a_roll);
	std::string TexturePath(bool a_ube, Zone a_zone, int a_index, bool a_mega);  // "textures\\Afterglow\\UBE\\Oral\\head3.dds"
	std::string NormalPath(std::string_view a_diffuse);                           // ..._n.dds

	// ------------------------------------------------------------------ per-actor state
	struct Layer
	{
		std::uint8_t  slot{ 0 };
		std::uint8_t  index{ 0 };
		bool          mega{ false };
		float         appliedAt{ 0.0f };  // game days (Calendar::GetCurrentGameTime)
	};

	struct ActorState
	{
		std::array<std::vector<Layer>, kZoneCount> zones{};
		bool  ube{ false };

		bool                 Empty() const;
		std::size_t          LayerCount() const;
		std::vector<int>     IndicesInUse(Zone a_zone) const;
		std::vector<Layer>&  Of(Zone a_zone) { return zones[static_cast<std::size_t>(a_zone)]; }
		const std::vector<Layer>& Of(Zone a_zone) const { return zones[static_cast<std::size_t>(a_zone)]; }
	};

	// What adding one layer to a zone should do.
	struct AddDecision
	{
		enum class Kind
		{
			UseFreeSlot,     // paint into `slot`
			ReplaceOldest,   // zone is at its cap (or no free slot): repaint our oldest layer of this zone, `replaceAt` = its position in the zone vector
			Skip,            // no free slot and no own layer to reuse
		};
		Kind kind{ Kind::Skip };
		int  slot{ -1 };
		int  replaceAt{ -1 };
	};
	AddDecision DecideAdd(const ActorState& a_state, Zone a_zone, int a_maxLayers, int a_freeSlot);

	// Layers older than a_dryDays (game days); a_dryDays <= 0 means never.
	// Returns (zone, position) pairs, newest-first per zone so erasing in order is safe.
	std::vector<std::pair<Zone, int>> ExpiredLayers(const ActorState& a_state, float a_now, float a_dryDays);

	// ------------------------------------------------------------------ save format
	// Records: count:u32, then per actor: formID:u32, ube:u8, per zone: n:u8, layers{slot,index,mega,appliedAt}
	std::vector<std::uint8_t> Serialize(const std::vector<std::pair<std::uint32_t, ActorState>>& a_actors);
	std::optional<std::vector<std::pair<std::uint32_t, ActorState>>> Deserialize(std::span<const std::uint8_t> a_bytes);
	inline constexpr std::uint32_t kSaveVersion = 1;

	// ------------------------------------------------------------------ one paint per orgasm
	// SexLab P+ raises two events for one orgasm: "SexLabOrgasm" (who came) and, when its own
	// detection found a type, "SexLabApplyCum" (on the partner). Their order is not fixed.
	//  - a scene WITH cum tags is painted from the orgasm (tags give hands/feet/breast too);
	//    ApplyCum for that target is then ignored;
	//  - a scene WITHOUT tags waits a moment for ApplyCum (P+ knows the type); if none comes
	//    (creatures: P+ never sees their bodies) the fallback zone is painted.
	class Arbiter
	{
	public:
		struct Paint
		{
			std::uint32_t target{ 0 };
			std::uint32_t source{ 0 };
			ZoneSet       zones{ 0 };
		};

		// orgasm of a_source; a_targets already filtered by IsTarget
		std::vector<Paint> OnOrgasm(std::uint32_t a_source, std::span<const std::uint32_t> a_targets, std::uint32_t a_tagMask, double a_now);
		// P+ cum fx on a_target; a_tagMask = tags of the scene a_target is in (0 = none or no scene)
		std::optional<Paint> OnApplyCum(std::uint32_t a_target, int a_type, std::uint32_t a_tagMask, double a_now);
		// the same with the zones already known (SexLab Classic cum spells)
		std::optional<Paint> OnCumZones(std::uint32_t a_target, ZoneSet a_zones, std::uint32_t a_tagMask, double a_now);
		// pending untagged orgasms whose wait ran out
		std::vector<Paint> Expire(double a_now);
		bool               HasPending() const { return !_pending.empty(); }
		void               Clear();

		ZoneSet fallbackZones{ Bit(Zone::Vaginal) };
		double  waitSeconds{ 2.0 };    // how long an untagged orgasm waits for ApplyCum
		double  ownWindow{ 4.0 };      // ApplyCum this soon after we painted the target is the same orgasm

	private:
		struct Pending
		{
			std::uint32_t source;
			double        deadline;
		};
		std::unordered_map<std::uint32_t, Pending> _pending;    // by target
		std::unordered_map<std::uint32_t, double>  _lastPaint;  // by target, tagged orgasm paints
	};
}
