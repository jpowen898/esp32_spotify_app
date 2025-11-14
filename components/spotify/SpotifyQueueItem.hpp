/**
 * @file SpotifyQueueItem.hpp
 * @brief Spotify Queue Item class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify SpotifyQueueItem class which provides a high-level interface
 * for representing and managing individual items in a Spotify playback queue. It handles
 * the display and interaction logic for queue items within the user interface.
 */

#ifndef __SPOTIFYQUEUEITEM_HPP__
#define __SPOTIFYQUEUEITEM_HPP__

#include <vector>
#include <string>
#include "ui.h"
#include "lvgl.h"

class SpotifyQueueItem
{
public:
    static void song_queue_clicked_cb(lv_event_t* e);
    SpotifyQueueItem();
    SpotifyQueueItem(const std::string& song, const std::string& artist, const std::string& uri,
                     const std::string& art_url = "");
    ~SpotifyQueueItem();
    void setSongName(const std::string& name);
    void setArtistName(const std::string& name);

    // private:
    std::string song_name;
    std::string artist_name;
    std::string song_uri;
    std::string album_art_url; ///< URL of the album art for this song
    lv_obj_t*   queue_item_pannel;
    lv_obj_t*   labels_container;
    lv_obj_t*   song_label;
    lv_obj_t*   artist_label;
    lv_obj_t*   move_up_btn;
    lv_obj_t*   move_down_btn;
};

#endif // __SPOTIFYQUEUEITEM_HPP__