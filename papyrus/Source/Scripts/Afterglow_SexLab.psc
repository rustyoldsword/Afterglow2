Scriptname Afterglow_SexLab extends Quest
{Afterglow 2.0 bridge: SexLab sends its events with forms, which only Papyrus can receive.
Each event becomes one native call; nothing waits or loops here.
SexLab P+ 2.18.1/2.19: ThreadSlots.GetActorController, GetPositions, HasStageTag/HasSceneTag, GetSex.
SexLab Classic 1.6x: the same controller, but Positions, Animation.HasTag and GetGender instead
(different sex codes, no stage tags, no SexLabApplyCum event). One branch per version.}

SexLabFramework SexLab
Bool IsPlus

Event OnInit()
	Maintenance()
EndEvent

; after every load (Afterglow_PlayerAlias sends "Afterglow_GameLoaded" to this quest)
Event OnAfterglowGameLoaded(String asEventName, String asStrArg, Float afNumArg, Form akSender)
	Maintenance()
EndEvent

; washing (Bathing in Skyrim / Dirt and Blood) is heard by Afterglow_PlayerAlias, which links without SexLab
Function Maintenance()
	SexLab = None
	IsPlus = False
	If Game.IsPluginInstalled("SexLab.esm")
		SexLab = Game.GetFormFromFile(0x00000D62, "SexLab.esm") as SexLabFramework
		; P+ returns its SKSE plugin version (2.x), Classic a constant like 16300
		IsPlus = SexLabUtil.GetVersion() >= 20000
	EndIf
	RegisterForModEvent("SexLabOrgasm", "OnSexLabOrgasm")
	RegisterForModEvent("SexLabApplyCum", "OnSexLabApplyCum")
	RegisterForModEvent("SexLabClearCum", "OnSexLabClearCum")
	; saves from 2.0.2 and older: the wash events moved to the player alias, drop the quest's copy
	UnregisterForModEvent("BiS_CleanActorDirt")
	UnregisterForModEvent("BiS_WashActorFinish")
	UnregisterForModEvent("DnB_CleanActorDirt")
	Afterglow.OnGameLoaded()
EndFunction

Event OnSexLabOrgasm(Form akActor, Int aiEnjoyment, Int aiCount)
	Actor src = akActor as Actor
	If !src || !SexLab
		Return
	EndIf
	sslThreadController thread = SexLab.ThreadSlots.GetActorController(src)
	If !thread
		Return
	EndIf
	Actor[] positions
	If IsPlus
		positions = thread.GetPositions()
	Else
		positions = thread.Positions
	EndIf
	If !positions
		Return
	EndIf
	Int[] sexes = Utility.CreateIntArray(positions.Length)
	Int i = 0
	While i < positions.Length
		sexes[i] = SexOf(positions[i])
		i += 1
	EndWhile
	Afterglow.OnOrgasm(src, SexOf(src), positions, sexes, TagMask(thread))
EndEvent

; P+ only (Classic has no such event)
Event OnSexLabApplyCum(Form akActor, Int aiType)
	Actor target = akActor as Actor
	If !target || !SexLab
		Return
	EndIf
	Int mask = 0
	sslThreadController thread = SexLab.ThreadSlots.GetActorController(target)
	If thread
		mask = TagMask(thread)
	EndIf
	Afterglow.OnApplyCum(target, aiType, mask, SexOf(target))
EndEvent

Event OnSexLabClearCum(Form akActor, Int aiType)
	Actor target = akActor as Actor
	If target
		Afterglow.OnSexLabClear(target, aiType)
	EndIf
EndEvent

; Afterglow sex codes = P+ ones: 0 male, 1 female, 2 futa, 3 creature male, 4 creature female, -1 none
Int Function SexOf(Actor akActor)
	If !akActor
		Return -1
	ElseIf IsPlus
		Return SexLab.GetSex(akActor)
	EndIf
	; Classic GetGender: 0 male, 1 female, 2 creature male, 3 creature female
	Int g = SexLab.GetGender(akActor)
	If g >= 2
		Return g + 1
	EndIf
	Return g
EndFunction

; The cum tags of the current stage; scenes that only tag the whole animation fall back to it.
; Classic has animation tags only.
Int Function TagMask(sslThreadController akThread)
	Int mask = 0
	If IsPlus
		mask = MaskOf(akThread, 1)
		If mask == 0
			mask = MaskOf(akThread, 0)
		EndIf
	Else
		mask = MaskOf(akThread, 2)
	EndIf
	Return mask
EndFunction

; bits: 1 vaginal, 2 anal, 4 oral, 8 breast, 16 hands, 32 feet (Afterglow.dll TagBit)
; aiSource: 1 P+ stage tags, 0 P+ scene tags, 2 Classic animation tags
Int Function MaskOf(sslThreadController akThread, Int aiSource)
	sslBaseAnimation anim = None
	If aiSource == 2
		anim = akThread.Animation
		If !anim
			Return 0
		EndIf
	EndIf
	Int mask = 0
	If HasAny(akThread, anim, aiSource, "Vaginal", "Creampie", "VaginalCum", "")
		mask += 1
	EndIf
	If HasAny(akThread, anim, aiSource, "Anal", "AnalCreampie", "AnalCum", "")
		mask += 2
	EndIf
	If HasAny(akThread, anim, aiSource, "Oral", "Blowjob", "Facial", "CumInMouth")
		mask += 4
	EndIf
	If HasAny(akThread, anim, aiSource, "Boobjob", "Titjob", "Breast", "ChestCum")
		mask += 8
	EndIf
	If HasAny(akThread, anim, aiSource, "Handjob", "HandsCum", "", "")
		mask += 16
	EndIf
	If HasAny(akThread, anim, aiSource, "Footjob", "FeetCum", "", "")
		mask += 32
	EndIf
	Return mask
EndFunction

Bool Function HasAny(sslThreadController akThread, sslBaseAnimation akAnim, Int aiSource, String a1, String a2, String a3, String a4)
	Return Has(akThread, akAnim, aiSource, a1) || Has(akThread, akAnim, aiSource, a2) || Has(akThread, akAnim, aiSource, a3) || Has(akThread, akAnim, aiSource, a4)
EndFunction

Bool Function Has(sslThreadController akThread, sslBaseAnimation akAnim, Int aiSource, String asTag)
	If asTag == ""
		Return False
	ElseIf aiSource == 2
		Return akAnim.HasTag(asTag)
	ElseIf aiSource == 1
		Return akThread.HasStageTag(asTag)
	EndIf
	Return akThread.HasSceneTag(asTag)
EndFunction
