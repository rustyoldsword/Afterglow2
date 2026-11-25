Scriptname Afterglow_PlayerAlias extends ReferenceAlias
{Afterglow 2.0: the washing listener and the load hook. It names no SexLab or OStim type, so it links
with either framework or none: Bathing in Skyrim / Dirt and Blood reach Afterglow under OStim alone too.
After every load it asks the quest's bridges to re-register through a mod event (no cast to them:
a cast to Afterglow_SexLab would unlink this script whenever SexLab is missing).}

Event OnInit()
	Register()
EndEvent

Event OnPlayerLoadGame()
	Register()
	GetOwningQuest().SendModEvent("Afterglow_GameLoaded")
EndEvent

Function Register()
	RegisterForModEvent("BiS_CleanActorDirt", "OnBiSClean")
	RegisterForModEvent("BiS_WashActorFinish", "OnBiSWashFinish")
	RegisterForModEvent("DnB_CleanActorDirt", "OnDnBClean")
	; Afterglow_SexLab sits on the quest; registering the quest delivers the event to it
	GetOwningQuest().RegisterForModEvent("Afterglow_GameLoaded", "OnAfterglowGameLoaded")
	Afterglow.OnBathingListener(Game.IsPluginInstalled("Bathing in Skyrim.esp"), Game.IsPluginInstalled("Dirt and Blood - Dynamic Visuals.esp"))
EndFunction

Event OnBiSClean(Form akTarget, Float afTimeToClean, Float afInterval, Bool abUsedSoap)
	Afterglow.OnWashed(akTarget as Actor)
EndEvent

Event OnBiSWashFinish(Form akBathingActor, Form akWashProp, Bool abUsingSoap)
	Afterglow.OnWashed(akBathingActor as Actor)
EndEvent

Event OnDnBClean(Form akTarget)
	Afterglow.OnWashed(akTarget as Actor)
EndEvent
