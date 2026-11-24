#define DOCTEST_CONFIG_IMPLEMENT_WITH_MAIN
#include <doctest/doctest.h>

#include "core/Core.h"

using namespace AG;

TEST_CASE("tags map to zones")
{
	CHECK(ZonesForTags(0) == 0);
	CHECK(ZonesForTags(kTagVaginal) == Bit(Zone::Vaginal));
	CHECK(ZonesForTags(kTagAnal) == Bit(Zone::Anal));
	// oral covers the face and drips on the chest, like the original SLACS
	CHECK(ZonesForTags(kTagOral) == (Bit(Zone::Face) | Bit(Zone::Breast)));
	CHECK(ZonesForTags(kTagBreast) == Bit(Zone::Breast));
	CHECK(ZonesForTags(kTagHands) == Bit(Zone::Hands));
	// footjob paints the feet and the legs (body overlay)
	CHECK(ZonesForTags(kTagFeet) == (Bit(Zone::Feet) | Bit(Zone::Legs)));
}

TEST_CASE("SexLab cum fx types map to zones")
{
	CHECK(ZonesForCumType(0) == Bit(Zone::Vaginal));
	CHECK(ZonesForCumType(1) == Bit(Zone::Anal));
	CHECK(ZonesForCumType(2) == (Bit(Zone::Face) | Bit(Zone::Breast)));
	CHECK(ZonesForCumType(-1) == (Bit(Zone::Vaginal) | Bit(Zone::Anal) | Bit(Zone::Face) | Bit(Zone::Breast)));
	CHECK(ZonesForCumType(7) == 0);
}

TEST_CASE("creature males are sources - the 4264 creature bug")
{
	CHECK(IsSource(0));
	CHECK(IsSource(2));
	CHECK(IsSource(3));
	CHECK_FALSE(IsSource(1));
	CHECK_FALSE(IsSource(4));
}

TEST_CASE("targets: females, futa and female creatures; males only by option")
{
	CHECK(IsTarget(1, false));
	CHECK(IsTarget(2, false));
	CHECK(IsTarget(4, false));
	CHECK_FALSE(IsTarget(0, false));
	CHECK_FALSE(IsTarget(3, false));
	CHECK(IsTarget(0, true));
	CHECK(IsTarget(3, true));
}

TEST_CASE("slot textures are classified, foreign ones are never ours")
{
	CHECK(ClassifySlotTexture("") == SlotState::Free);
	CHECK(ClassifySlotTexture("actors\\character\\overlays\\default.dds") == SlotState::Free);
	CHECK(ClassifySlotTexture("Actors\\Character\\Overlays\\Default.dds") == SlotState::Free);
	// SKEE's live node property comes back quoted (seen in game 16.11 on every empty "Body [Ovl#]")
	CHECK(ClassifySlotTexture("\"textures\\actors\\character\\overlays\\default.dds\"") == SlotState::Free);
	CHECK(ClassifySlotTexture(" \"\" ") == SlotState::Free);
	CHECK(ClassifySlotTexture("\"textures\\Afterglow\\CBBE\\Oral\\head1.dds\"") == SlotState::Ours);
	CHECK(ClassifySlotTexture("textures\\Afterglow\\UBE\\Oral\\head3.dds") == SlotState::Ours);
	CHECK(ClassifySlotTexture("Textures/afterglow/CBBE/Vaginal/body1.dds") == SlotState::Ours);
	CHECK(ClassifySlotTexture("textures\\actors\\character\\slavetats\\tattoo.dds") == SlotState::Foreign);
	// the old SLACS paths are foreign to 2.0: we never paint over or remove them
	CHECK(ClassifySlotTexture("data\\textures\\slacs2\\Vaginal\\body1.dds") == SlotState::Foreign);
}

TEST_CASE("free slot search starts at the configured slot and skips taken ones")
{
	std::vector<SlotState> s(10, SlotState::Free);
	CHECK(PickFreeSlot(s, 7) == 7);
	s[7] = SlotState::Foreign;
	s[8] = SlotState::Ours;
	CHECK(PickFreeSlot(s, 7) == 9);
	s[9] = SlotState::Foreign;
	CHECK(PickFreeSlot(s, 7) == -1);
	CHECK(PickFreeSlot(s, 0) == 0);
	CHECK(PickFreeSlot(s, 12) == -1);
	CHECK(PickFreeSlot({}, 0) == -1);
}

TEST_CASE("texture index avoids numbers already on the zone")
{
	std::vector<int> used{ 1, 2, 4 };
	// free numbers are 3; roll picks among the free ones only
	for (std::uint32_t roll = 0; roll < 20; ++roll) {
		CHECK(PickTextureIndex(4, used, roll) == 3);
	}
	std::vector<int> all{ 1, 2, 3, 4 };
	for (std::uint32_t roll = 0; roll < 20; ++roll) {
		int i = PickTextureIndex(4, all, roll);
		CHECK(i >= 1);
		CHECK(i <= 4);
	}
	CHECK(PickTextureIndex(0, used, 5) == 0);
}

TEST_CASE("texture paths")
{
	CHECK(TexturePath(true, Zone::Face, 3, false) == "textures\\Afterglow\\UBE\\Oral\\head3.dds");
	CHECK(TexturePath(false, Zone::Legs, 2, true) == "textures\\Afterglow\\CBBE\\Footjob\\m_legs2.dds");
	CHECK(TexturePath(false, Zone::Breast, 12, false) == "textures\\Afterglow\\CBBE\\Breast\\chest12.dds");
	CHECK(NormalPath("textures\\Afterglow\\CBBE\\Breast\\chest12.dds") == "textures\\Afterglow\\CBBE\\Breast\\chest12_n.dds");
	CHECK(AreaOf(Zone::Legs) == Area::Body);
	CHECK(AreaOf(Zone::Feet) == Area::Feet);
	CHECK(AreaOf(Zone::Face) == Area::Face);
	CHECK(TextureCount(Zone::Hands) == 4);
	CHECK(TextureCount(Zone::Vaginal) == 12);
}

TEST_CASE("adding layers: free slot, then cap, then reuse of own oldest only")
{
	ActorState st;
	auto d = DecideAdd(st, Zone::Vaginal, 2, 7);
	CHECK(d.kind == AddDecision::Kind::UseFreeSlot);
	CHECK(d.slot == 7);

	st.Of(Zone::Vaginal).push_back({ 7, 1, false, 1.0f });
	st.Of(Zone::Vaginal).push_back({ 8, 2, false, 2.0f });
	d = DecideAdd(st, Zone::Vaginal, 2, 9);  // at cap even though slot 9 is free
	CHECK(d.kind == AddDecision::Kind::ReplaceOldest);
	CHECK(d.slot == 7);
	CHECK(d.replaceAt == 0);

	// below cap but no free slot: reuse own oldest of the zone
	d = DecideAdd(st, Zone::Vaginal, 6, -1);
	CHECK(d.kind == AddDecision::Kind::ReplaceOldest);
	CHECK(d.slot == 7);

	// no free slot and nothing of ours in this zone: skip (never a foreign slot)
	d = DecideAdd(st, Zone::Anal, 6, -1);
	CHECK(d.kind == AddDecision::Kind::Skip);

	// a zone switched off (cap 0) never paints
	d = DecideAdd(st, Zone::Hands, 0, 7);
	CHECK(d.kind == AddDecision::Kind::Skip);
}

TEST_CASE("drying: only layers older than the limit, newest first per zone")
{
	ActorState st;
	st.Of(Zone::Face).push_back({ 7, 1, false, 1.0f });
	st.Of(Zone::Face).push_back({ 8, 2, false, 1.5f });
	st.Of(Zone::Face).push_back({ 9, 3, false, 2.9f });
	st.Of(Zone::Anal).push_back({ 7, 1, false, 1.2f });
	auto e = ExpiredLayers(st, 3.0f, 1.0f);  // older than 1 day: appliedAt < 2.0
	REQUIRE(e.size() == 3);
	CHECK(e[0] == std::pair{ Zone::Anal, 0 });
	CHECK(e[1] == std::pair{ Zone::Face, 1 });
	CHECK(e[2] == std::pair{ Zone::Face, 0 });
	CHECK(ExpiredLayers(st, 100.0f, 0.0f).empty());  // never dries
}

TEST_CASE("save round trip and corrupt input")
{
	ActorState a;
	a.ube = true;
	a.Of(Zone::Breast).push_back({ 19, 5, true, 12.25f });
	a.Of(Zone::Hands).push_back({ 7, 2, false, 3.5f });
	ActorState b;
	b.Of(Zone::Vaginal).push_back({ 7, 1, false, 0.5f });
	std::vector<std::pair<std::uint32_t, ActorState>> in{ { 0x14, a }, { 0xFE000801, b } };

	auto bytes = Serialize(in);
	auto out = Deserialize(bytes);
	REQUIRE(out.has_value());
	REQUIRE(out->size() == 2);
	CHECK((*out)[0].first == 0x14);
	CHECK((*out)[0].second.ube);
	REQUIRE((*out)[0].second.Of(Zone::Breast).size() == 1);
	CHECK((*out)[0].second.Of(Zone::Breast)[0].slot == 19);
	CHECK((*out)[0].second.Of(Zone::Breast)[0].index == 5);
	CHECK((*out)[0].second.Of(Zone::Breast)[0].mega);
	CHECK((*out)[0].second.Of(Zone::Breast)[0].appliedAt == doctest::Approx(12.25f));
	CHECK((*out)[1].first == 0xFE000801);
	CHECK((*out)[1].second.LayerCount() == 1);

	bytes.pop_back();
	CHECK_FALSE(Deserialize(bytes).has_value());
	CHECK(Deserialize({}).has_value() == false);
}

TEST_CASE("arbiter: tagged orgasm paints, P+ ApplyCum for the same target is ignored")
{
	Arbiter arb;
	std::uint32_t targets[] = { 0x14 };
	auto p = arb.OnOrgasm(0xAA, targets, kTagOral, 10.0);
	REQUIRE(p.size() == 1);
	CHECK(p[0].target == 0x14);
	CHECK(p[0].source == 0xAA);
	CHECK(p[0].zones == (Bit(Zone::Face) | Bit(Zone::Breast)));
	// ApplyCum in a tagged scene: tags already painted it
	CHECK_FALSE(arb.OnApplyCum(0x14, 2, kTagOral, 10.1).has_value());
	CHECK_FALSE(arb.HasPending());
}

TEST_CASE("arbiter: ApplyCum first in a tagged scene does nothing, the orgasm paints once")
{
	Arbiter arb;
	CHECK_FALSE(arb.OnApplyCum(0x14, 0, kTagVaginal, 5.0).has_value());
	std::uint32_t targets[] = { 0x14 };
	CHECK(arb.OnOrgasm(0xAA, targets, kTagVaginal, 5.05).size() == 1);
}

TEST_CASE("arbiter: untagged orgasm waits for ApplyCum, which then paints once")
{
	Arbiter arb;
	std::uint32_t targets[] = { 0x14 };
	CHECK(arb.OnOrgasm(0xAA, targets, 0, 1.0).empty());
	CHECK(arb.HasPending());
	auto p = arb.OnApplyCum(0x14, 1, 0, 1.5);
	REQUIRE(p.has_value());
	CHECK(p->zones == Bit(Zone::Anal));
	CHECK(p->source == 0xAA);
	CHECK_FALSE(arb.HasPending());
	CHECK(arb.Expire(10.0).empty());
}

TEST_CASE("arbiter: untagged creature orgasm without ApplyCum paints the fallback after the wait")
{
	Arbiter arb;
	std::uint32_t targets[] = { 0x14 };
	CHECK(arb.OnOrgasm(0xCC, targets, 0, 1.0).empty());
	CHECK(arb.Expire(2.5).empty());  // still waiting (deadline 3.0)
	auto p = arb.Expire(3.1);
	REQUIRE(p.size() == 1);
	CHECK(p[0].zones == Bit(Zone::Vaginal));
	CHECK(p[0].source == 0xCC);
	CHECK_FALSE(arb.HasPending());
}

TEST_CASE("arbiter: untagged scene where ApplyCum arrives before the orgasm paints once")
{
	Arbiter arb;
	REQUIRE(arb.OnApplyCum(0x14, 0, 0, 1.0).has_value());
	std::uint32_t targets[] = { 0x14 };
	CHECK(arb.OnOrgasm(0xAA, targets, 0, 1.2).empty());
	CHECK_FALSE(arb.HasPending());  // no fallback paint on top
	CHECK(arb.Expire(5.0).empty());
}

TEST_CASE("arbiter: ApplyCum outside any orgasm (other mods, console) paints by type")
{
	Arbiter arb;
	auto p = arb.OnApplyCum(0x14, -1, 0, 50.0);
	REQUIRE(p.has_value());
	CHECK(p->source == 0);
	CHECK(p->zones == ZonesForCumType(-1));
}

// SexLab Classic: AddCum casts one spell carrying the new level of every type; Fill Her Up
// expels with AddCum(actor, vaginal/oral/anal) outside any scene.
TEST_CASE("classic cum: the type whose level rose is the one added")
{
	const CumLevels none{};
	// first vaginal expel on a clean body
	CHECK(ClassicAddedZones(none, { 1, 0, 0 }) == ZonesForCumType(0));
	// anal on top of an existing oral: the oral level is carried over, only anal is new
	CHECK(ClassicAddedZones({ 0, 0, 1 }, { 0, 1, 1 }) == ZonesForCumType(1));
	// second vaginal: 1 -> 2 (stacked), the oral level stays
	CHECK(ClassicAddedZones({ 1, 0, 1 }, { 2, 0, 1 }) == ZonesForCumType(0));
	// oral from nothing
	CHECK(ClassicAddedZones(none, { 0, 0, 1 }) == ZonesForCumType(2));
}

TEST_CASE("classic cum: a type already at the cap is found when it is the only one at 2")
{
	// vaginal again while vaginal is stacked: nothing rose, vaginal is the only type at 2
	CHECK(ClassicAddedZones({ 2, 0, 1 }, { 2, 0, 1 }) == ZonesForCumType(0));
	// two types at 2 and nothing rose: cannot tell which was requested, paint nothing
	CHECK(ClassicAddedZones({ 2, 2, 0 }, { 2, 2, 0 }) == 0);
	// nothing at all
	CHECK(ClassicAddedZones({}, {}) == 0);
}

TEST_CASE("arbiter: zone-set cum outside a scene paints once, a repeat inside the window does not")
{
	Arbiter arb;
	auto p = arb.OnCumZones(0x14, Bit(Zone::Anal), 0, 20.0);
	REQUIRE(p.has_value());
	CHECK(p->zones == Bit(Zone::Anal));
	CHECK_FALSE(arb.OnCumZones(0x14, Bit(Zone::Anal), 0, 21.0).has_value());
	CHECK(arb.OnCumZones(0x14, Bit(Zone::Anal), 0, 30.0).has_value());
	CHECK_FALSE(arb.OnCumZones(0x14, 0, 0, 40.0).has_value());
}
