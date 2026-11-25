Scriptname Afterglow Hidden
{Afterglow 2.0 - natives of Afterglow.dll. All the work happens in the DLL.}

; One call per orgasm. akPartners are the scene positions, aiSexes their SexLab sex
; (0 male, 1 female, 2 futa, 3 creature male, 4 creature female), aiTagMask the cum tags of
; the current stage (or of the scene when the stage has none).
Function OnOrgasm(Actor akSource, Int aiSourceSex, Actor[] akPartners, Int[] aiSexes, Int aiTagMask) global native

; SexLab P+'s "SexLabApplyCum" (aiType 0 vaginal, 1 anal, 2 oral, -1 all).
Function OnApplyCum(Actor akTarget, Int aiType, Int aiTagMask, Int aiTargetSex) global native

; SexLab P+'s "SexLabClearCum".
Function OnSexLabClear(Actor akTarget, Int aiType) global native

; Bathing in Skyrim / Dirt and Blood finished washing the actor.
Function OnWashed(Actor akTarget) global native

; The washing listener (Afterglow_PlayerAlias) registered; only writes a line to Afterglow.log.
Function OnBathingListener(Bool abBathingInSkyrim, Bool abDirtAndBlood) global native

; Removes every Afterglow layer from the actor (for other mods).
Function Clear(Actor akTarget) global native

Function OnGameLoaded() global native

Int Function GetVersion() global native
