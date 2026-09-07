EventScript_UseSurf::
	goto_if_questlog EventScript_UseSurfEnd
	special CanUseSurf
	goto_if_eq VAR_RESULT, FALSE, EventScript_UseSurfEnd
	lockall
	msgbox Text_WantToSurf, MSGBOX_YESNO
	goto_if_eq VAR_RESULT, NO, EventScript_UseSurfRelease
	msgbox Text_UsedSurf
	dofieldeffect FLDEFF_USE_SURF
EventScript_UseSurfRelease:
	releaseall
EventScript_UseSurfEnd:
	end

EventScript_CurrentTooFast::
	lockall
	msgbox Text_CurrentTooFast
	releaseall
	end
