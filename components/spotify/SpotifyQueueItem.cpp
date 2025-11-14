/**
 * @file SpotifyQueueItem.cpp
 * @brief Spotify Queue Item class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyQueueItem class which provides a high-level interface
 * for representing and managing individual items in a Spotify playback queue. It handles
 * the display and interaction logic for queue items within the user interface.
 */

#include <SpotifyQueueItem.hpp>
#include <Spotify.hpp>
#include <display_init.h>

static bool everyOther = false;

void SpotifyQueueItem::song_queue_clicked_cb(lv_event_t* e)
{
    SpotifyQueueItem* data = static_cast<SpotifyQueueItem*>(lv_event_get_user_data(e));
    lv_event_code_t   code = lv_event_get_code(e);
    if (data && code == LV_EVENT_CLICKED)
    {
        printf("Clicked SONG BTN for song in the queue: %s by %s\n", data->song_name.c_str(),
               data->artist_name.c_str());
        Spotify::getInstance().play(data->song_uri);
        Spotify::getInstance().requestQueue();
    }
}

SpotifyQueueItem::SpotifyQueueItem() : SpotifyQueueItem("Song Name", "Artist Name", "Song URI", "")
{
}
SpotifyQueueItem::SpotifyQueueItem(const std::string& song, const std::string& artist,
                                   const std::string& uri, const std::string& art_url)
    : song_name(song), artist_name(artist), song_uri(uri), album_art_url(art_url)
{
    ui_lvgl_lock(-1);

    queue_item_pannel = lv_obj_create(ui_Queue_Container);
    lv_obj_set_width(queue_item_pannel, 360);
    lv_obj_set_height(queue_item_pannel, 45);
    lv_obj_set_align(queue_item_pannel, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(queue_item_pannel, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(queue_item_pannel, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(queue_item_pannel, LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_bg_color(queue_item_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_opa(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_color(queue_item_pannel, lv_color_hex(0x413C3C),
                                  LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_border_opa(queue_item_pannel, 150, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_left(queue_item_pannel, 45, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_right(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_top(queue_item_pannel, 5, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_pad_bottom(queue_item_pannel, 0, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_bg_color(queue_item_pannel, lv_color_hex(0xFFFFFF),
                              LV_PART_MAIN | LV_STATE_PRESSED);
    lv_obj_set_style_bg_opa(queue_item_pannel, 150, LV_PART_MAIN | LV_STATE_PRESSED);

    labels_container = lv_obj_create(queue_item_pannel);
    lv_obj_remove_style_all(labels_container);
    lv_obj_set_width(labels_container, 309);
    lv_obj_set_height(labels_container, 32);
    lv_obj_set_align(labels_container, LV_ALIGN_CENTER);
    lv_obj_set_flex_flow(labels_container, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(labels_container, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START,
                          LV_FLEX_ALIGN_START);
    lv_obj_clear_flag(labels_container,
                      LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE); /// Flags
    lv_obj_set_style_transform_pivot_x(labels_container, 100, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_transform_pivot_y(labels_container, 0, LV_PART_MAIN | LV_STATE_DEFAULT);

    song_label = lv_label_create(labels_container);
    lv_obj_set_width(song_label, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(song_label, LV_SIZE_CONTENT); /// 1
    lv_label_set_text(song_label, song_name.c_str());
    lv_obj_set_style_text_font(song_label, &lv_font_montserrat_16, LV_PART_MAIN | LV_STATE_DEFAULT);

    artist_label = lv_label_create(labels_container);
    lv_obj_set_width(artist_label, LV_SIZE_CONTENT);  /// 1
    lv_obj_set_height(artist_label, LV_SIZE_CONTENT); /// 1
    lv_label_set_text(artist_label, artist_name.c_str());
    lv_obj_set_style_text_font(artist_label, &lv_font_montserrat_12,
                               LV_PART_MAIN | LV_STATE_DEFAULT);

    // unscroll by the same amount as adding the new item
    lv_obj_scroll_by(ui_Queue_Container, 0, 45 / 2 + (everyOther ? 1 : 0), LV_ANIM_OFF);
    everyOther = !everyOther;

    ui_lvgl_unlock();
}
SpotifyQueueItem::~SpotifyQueueItem()
{
    ui_lvgl_lock(-1);

    // Remove all event callbacks first
    lv_obj_remove_event_cb(queue_item_pannel, song_queue_clicked_cb);
    lv_obj_del(queue_item_pannel);
    song_name.clear();
    artist_name.clear();

    // unscroll by the same amount as adding the new item
    everyOther = !everyOther;
    lv_obj_scroll_by(ui_Queue_Container, 0, -45 / 2 - (everyOther ? 1 : 0), LV_ANIM_OFF);

    ui_lvgl_unlock();
}
void SpotifyQueueItem::setSongName(const std::string& name)
{
    ui_lvgl_lock(-1);
    song_name = name;
    lv_label_set_text(song_label, song_name.c_str());
    ui_lvgl_unlock();
}
void SpotifyQueueItem::setArtistName(const std::string& name)
{
    ui_lvgl_lock(-1);
    artist_name = name;
    lv_label_set_text(artist_label, artist_name.c_str());
    ui_lvgl_unlock();
}
