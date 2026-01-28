SET(SOURCES screens/ui_ScreenLogin.c
    screens/ui_ScreenRegister.c
    screens/ui_ScreenLobby.c
    screens/ui_ScreenGame.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    ui_events.c
    images/ui_img_arrow_png.c)

add_library(ui ${SOURCES})
