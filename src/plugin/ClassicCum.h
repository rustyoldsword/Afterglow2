#pragma once

namespace AG::ClassicCum
{
	// SexLab Classic 1.6x sends no "SexLabApplyCum": its AddCum only casts a cum spell. Mods that
	// call it outside a scene (Fill Her Up when expelling) therefore never reached Afterglow.
	// On Classic only, a SexLab cum spell landing on an actor who is NOT in a SexLab scene is
	// treated like P+'s ApplyCum: one layer on the type that spell added. Inside a scene the
	// orgasm already paints. On P+ this stays off - P+ raises its own event.
	// Afterglow.ini iClassicCumSpells: -1 auto (Classic only), 0 never, 1 always (testing on P+).
	void Register();  // kDataLoaded
	void Sync();      // kNewGame / kPostLoadGame
}
