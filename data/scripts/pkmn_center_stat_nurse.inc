EventScript_PkmnCenterStatNurse::
	goto_if_questlog EventScript_ReleaseEnd
	message Text_WelcomeWhatService
	waitmessage
	multichoice 1, 1, MULTICHOICE_STAT_NURSE_SERVICE, FALSE | (TRUE << 1)
	switch VAR_RESULT
	case 0, EventScript_StatNurse_NatureChange
	case 1, EventScript_StatNurse_EVChange
    case 2, EventScript_PkmnCenterNurse_Goodbye
	case SCR_MENU_CANCEL, EventScript_PkmnCenterNurse_Goodbye
	end

EventScript_StatNurse_NatureChange::
    special ChoosePartyMon
    waitstate
    message Text_NatureChange_WhichStatUp
    waitmessage
    multichoice 1, 1, MULTICHOICE_NATURE_INCREASE_STAT, FALSE
    switch VAR_RESULT
    case SCR_MENU_CANCEL, EventScript_StatNurse_NatureChange_Canceled
    copyvar VAR_0x8005, VAR_RESULT
    message Text_NatureChange_WhichStatDown
    waitmessage
    multichoice 1, 1, MULTICHOICE_NATURE_DECREASE_STAT, FALSE
    switch VAR_RESULT
    case SCR_MENU_CANCEL, EventScript_StatNurse_NatureChange_Canceled
    copyvar VAR_0x8006, VAR_RESULT
    special SetChosenMonNature
    msgbox Text_NatureChange_Done
    return

EventScript_StatNurse_NatureChange_Canceled::
    return

EventScript_StatNurse_EVChange::
	special ChoosePartyMon
	waitstate
	return
