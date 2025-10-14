SET(SOURCES screens/ui_Main_Screen.c
    screens/ui_Now_Playing_Screen.c
    screens/ui_Queue_Screen.c
    screens/ui_Playlists_Screen.c
    screens/ui_PlayList_Screen.c
    screens/ui_Settings_Screen.c
    ui_theme_manager.c
    ui_themes.c
    ui.c
    components/ui_comp_hook.c
    ui_helpers.c
    images/ui_img_1151404881.c
    images/ui_img_play_arrow_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_skip_next_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_skip_previous_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_shuffle_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_queue_music_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_arrow_back_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_keyboard_arrow_up_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_keyboard_arrow_down_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c
    images/ui_img_playlist_add_40dp_e3e3e3_fill0_wght400_grad0_opsz40_png.c)

add_library(ui ${SOURCES})
