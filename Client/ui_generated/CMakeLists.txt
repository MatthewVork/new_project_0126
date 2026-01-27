SET(SOURCES screens/ui_ScreenLogin.c
    screens/ui_ScreenRegister.c
    screens/ui_ScreenLobby.c
    screens/ui_ScreenGame.c
    screens/ui_ScreenRoom.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c)

add_library(ui ${SOURCES})
