#include <Spotify.hpp>
#include "esp_console.h"
#include "argtable3/argtable3.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <wifi.h>
#include <display_init.h>
#include <ui.h>
#include "esp_jpeg_dec.h"
#include "png.h"

static lv_img_dsc_t album_image = {
    .header =
        {
            .cf          = LV_IMG_CF_RAW,
            .always_zero = 0,
            .reserved    = 0,
            .w           = 0,
            .h           = 0,
        },
    .data_size = 0,
    .data      = nullptr,
};

#define QUEUE_ACTION(_action_)                                                                     \
    bool _ret_ = xQueueSend(m_actionQueue, &action, 0) == pdPASS;                                  \
    if (!_ret_)                                                                                    \
    {                                                                                              \
        delete action;                                                                             \
        return _ret_;                                                                              \
    }

#define QUEUE_ACTION_AND_RET(_action_)                                                             \
    QUEUE_ACTION(_action_)                                                                         \
    return _ret_;

void spotify_cmd_init();

std::unique_ptr<std::string> Spotify::decodeJpegToRgb565(const std::string& jpeg_data,
                                                         uint16_t& width, uint16_t& height)
{
    jpeg_dec_config_t config = DEFAULT_JPEG_DEC_CONFIG();
    config.output_type =
        JPEG_PIXEL_FORMAT_RGB565_BE; // Big endian RGB565 for LVGL (LV_COLOR_16_SWAP=y)

    jpeg_dec_handle_t jpeg_dec = nullptr;
    jpeg_error_t      ret      = jpeg_dec_open(&config, &jpeg_dec);
    if (ret != JPEG_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to open JPEG decoder: %d", ret);
        return nullptr;
    }

    // Parse header to get dimensions
    jpeg_dec_io_t io;
    io.inbuf        = (uint8_t*) jpeg_data.data();
    io.inbuf_len    = jpeg_data.size();
    io.inbuf_remain = jpeg_data.size();

    jpeg_dec_header_info_t header_info;
    ret = jpeg_dec_parse_header(jpeg_dec, &io, &header_info);
    if (ret != JPEG_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to parse JPEG header: %d", ret);
        jpeg_dec_close(jpeg_dec);
        return nullptr;
    }

    width  = header_info.width;
    height = header_info.height;

    // Get output buffer size
    int outbuf_len;
    ret = jpeg_dec_get_outbuf_len(jpeg_dec, &outbuf_len);
    if (ret != JPEG_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to get output buffer length: %d", ret);
        jpeg_dec_close(jpeg_dec);
        return nullptr;
    }

    // Allocate aligned output buffer
    uint8_t* outbuf = (uint8_t*) jpeg_calloc_align(outbuf_len, 16);
    if (!outbuf)
    {
        ESP_LOGE(TAG, "Failed to allocate output buffer");
        jpeg_dec_close(jpeg_dec);
        return nullptr;
    }

    // Reset IO for decoding
    io.inbuf        = (uint8_t*) jpeg_data.data();
    io.inbuf_len    = jpeg_data.size();
    io.inbuf_remain = jpeg_data.size();
    io.outbuf       = outbuf;
    io.out_size     = outbuf_len;

    // Decode the image
    ret = jpeg_dec_process(jpeg_dec, &io);
    jpeg_dec_close(jpeg_dec);

    if (ret != JPEG_ERR_OK)
    {
        ESP_LOGE(TAG, "Failed to decode JPEG: %d", ret);
        jpeg_free_align(outbuf);
        return nullptr;
    }

    // Copy to std::string for consistent memory management
    auto result = std::make_unique<std::string>((char*) outbuf, outbuf_len);
    jpeg_free_align(outbuf);

    ESP_LOGI(TAG, "Successfully decoded JPEG: %dx%d, size=%d", width, height, outbuf_len);
    return result;
}

// Helper function to download and decode album art
std::unique_ptr<Spotify::AlbumArt> Spotify::downloadAndDecodeAlbumArt(const std::string& url)
{
    if (url.empty() || url.find("http") != 0)
    {
        return nullptr;
    }

    ESP_LOGI(TAG, "Downloading album art from: %s", url.c_str());

    HttpClient                   httpClient;
    std::unique_ptr<std::string> resp = httpClient.performRequestRaw(HTTP_METHOD_GET, url);

    if (!resp || resp->empty())
    {
        ESP_LOGE(TAG, "Failed to download album art");
        return nullptr;
    }

    // Decode the image
    uint16_t                     width = 0, height = 0;
    std::unique_ptr<std::string> decoded_data = nullptr;

    // Check if it's a JPEG (starts with FF D8) or PNG (starts with 89 50 4E 47)
    if (resp->size() >= 4)
    {
        uint8_t* data = (uint8_t*) resp->data();
        if (data[0] == 0xFF && data[1] == 0xD8)
        {
            // JPEG image
            decoded_data = decodeJpegToRgb565(*resp, width, height);
        }
    }

    if (!decoded_data || !width || !height)
    {
        ESP_LOGE(TAG, "Failed to decode album art");
        return nullptr;
    }

    auto albumArt    = std::make_unique<Spotify::AlbumArt>();
    albumArt->data   = std::move(decoded_data);
    albumArt->width  = width;
    albumArt->height = height;
    albumArt->url    = url;

    ESP_LOGI(TAG, "Successfully processed album art: %dx%d, size=%d", width, height,
             albumArt->data->size());

    return albumArt;
}

void Spotify::playlist_play_cb(lv_event_t* e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().play("0", Spotify::getInstance().m_activePlaylistId);
        if (Spotify::getInstance().getPlaybackState().shuffle_state)
            Spotify::getInstance().toggleShuffle();
    }
}
void Spotify::playlist_queue_cb(lv_event_t* e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED)
    {
        for (const auto& item : Spotify::getInstance().m_playlistItems)
        {
            Spotify::getInstance().addToQueue(item->song_uri);
        }
    }
}
void Spotify::playlist_shuffle_cb(lv_event_t* e)
{
    lv_event_code_t event_code = lv_event_get_code(e);

    if (event_code == LV_EVENT_CLICKED)
    {
        Spotify::getInstance().play("", Spotify::getInstance().m_activePlaylistId);
        if (!Spotify::getInstance().getPlaybackState().shuffle_state)
            Spotify::getInstance().toggleShuffle();
    }
}

Spotify::Spotify()
{
    setLogLevel(ESP_LOG_WARN);
    spotify_cmd_init();
}
Spotify::~Spotify() {}
void Spotify::start_task()
{
    m_actionQueue = xQueueCreate(100, sizeof(SpotifyAction*)); // 100 pending actions
    if (m_actionQueue == nullptr)
    {
        ESP_LOGE(TAG, "Failed to create Spotify action queue");
        return;
    }
    xTaskCreate([](void* obj) { static_cast<Spotify*>(obj)->task(); }, "spotifyTask", 10 * 1024,
                this, 5, &m_task);
}
void Spotify::task()
{
    m_playlists.push_back(std::make_unique<SpotifyPlaylist>(
        "Liked Songs", "spotify:collection:liked", "spotify:collection:liked"));
    m_playlists.push_back(std::make_unique<SpotifyPlaylist>(
        "Recently Played", "spotify:collection:recently_played", ""));
    lv_obj_add_event_cb(ui_Play_Playlist_Btn, playlist_play_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(ui_Queue_Playlist_Btn, playlist_queue_cb, LV_EVENT_ALL, this);
    lv_obj_add_event_cb(ui_Shuffle_Playlist_Btn, playlist_shuffle_cb, LV_EVENT_ALL, this);
    SpotifyAction* action;
    while (true)
    {
        if (!is_wifi_connected())
        {
            vTaskDelay(pdMS_TO_TICKS(1000));
        }
        else if (xQueueReceive(m_actionQueue, &action, pdMS_TO_TICKS(2000)) == pdPASS)
        {
            esp_log_level_t previous_level = esp_log_level_get(TAG);
            if (action->verbose)
            {
                setLogLevel(ESP_LOG_INFO);
            }
            ESP_LOGI(TAG, "Processing Spotify action: %s", action->toString().c_str());
            switch (action->type)
            {
                case SpotifyActionType::Play:
                {
                    if (action->context_uri == "spotify:collection:liked")
                    {
                        action->context_uri = "spotify:user:" + m_userInfo.user_id + ":collection";
                    }

                    json body;
                    if (action->song_uri == "0")
                    {
                        body = {{"context_uri", action->context_uri},
                                {"offset", {{"position", 0}}},
                                {"position_ms", 0}};
                    }
                    else
                    {
                        body = {{"context_uri", action->context_uri.empty()
                                                    ? m_currentlyPlayingInfo.context_uri
                                                    : action->context_uri},
                                {"offset",
                                 {{"uri", action->song_uri.empty()
                                              ? m_currentlyPlayingInfo.currentTrack.track_uri
                                              : action->song_uri}}},
                                {"position_ms", action->song_uri.empty()
                                                    ? m_currentlyPlayingInfo.progress_ms
                                                    : 0}};
                    }

                    auto resp = m_spotifyClient.put("me/player/play", body.dump(), true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_playbackState.is_playing = true;
                    }
                }
                break;

                case SpotifyActionType::Pause:
                {
                    auto resp = m_spotifyClient.put("me/player/pause", "", true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_playbackState.is_playing = false;
                    }
                }
                break;

                case SpotifyActionType::Next:
                {
                    m_spotifyClient.post("me/player/next", "", true);
                    updateCurrentlyPlaying();
                }
                break;

                case SpotifyActionType::Previous:
                {
                    m_spotifyClient.post("me/player/previous", "", true);
                    updateCurrentlyPlaying();
                }
                break;

                case SpotifyActionType::Seek:
                {
                    auto resp = m_spotifyClient.put("me/player/seek?position_ms=" +
                                                        std::to_string(action->int_param),
                                                    "", true);
                    if (resp && resp->contains("error") == false)
                    {
                        m_currentlyPlayingInfo.progress_ms = action->int_param;
                    }
                }
                break;

                case SpotifyActionType::SetVolume:
                {
                    if (!m_playbackState.supports_volume)
                    {
                        ESP_LOGW(TAG, "%s does not support volume adjustment",
                                 m_playbackState.device_name.c_str());
                        break;
                    }

                    m_spotifyClient.put("me/player/volume?volume_percent=" +
                                            std::to_string(m_desiredVolume),
                                        "", true);
                    m_volumeCmdInProgress    = false;
                    m_lastVolumeChangeTimeMs = getCurrentTimestampMs();
                    updatePlaybackState();
                }
                break;

                case SpotifyActionType::ToggleShuffle:
                {
                    m_spotifyClient.put(
                        "me/player/shuffle?state=" +
                            std::string(m_playbackState.shuffle_state ? "false" : "true"),
                        "", true);
                    updatePlaybackState();
                }
                break;

                case SpotifyActionType::SetRepeatMode:
                {
                    std::string mode = action->str_param;
                    if (mode != "off" && mode != "context" && mode != "track")
                    {
                        ESP_LOGE(TAG, "Invalid repeat mode: %s", mode.c_str());
                        break;
                    }
                    m_spotifyClient.put("me/player/repeat?state=" + mode, "", false);
                    updatePlaybackState();
                }
                break;

                case SpotifyActionType::UpdateCurrentlyPlaying:
                {
                    auto resp = m_spotifyClient.get("me/player/currently-playing");
                    if (resp)
                    {
                        if (resp->contains("error"))
                        {
                            ESP_LOGE(TAG, "Error getting currently playing: %s",
                                     (*resp)["error"]["message"].get<std::string>().c_str());
                            break;
                        }
                        else if (resp->is_null() || resp->empty())
                        {
                            ESP_LOGI(TAG, "No track is currently playing");
                            break;
                        }

                        if (resp->contains("item"))
                        {
                            if ((*resp)["item"].contains("name"))
                            {
                                m_currentlyPlayingInfo.currentTrack.name =
                                    std::string((*resp)["item"]["name"]);
                            }
                            if ((*resp)["item"].contains("artists") &&
                                (*resp)["item"]["artists"].is_array() &&
                                !(*resp)["item"]["artists"].empty())
                            {
                                if ((*resp)["item"]["artists"][0].contains("name"))
                                {
                                    m_currentlyPlayingInfo.currentTrack.artist =
                                        std::string((*resp)["item"]["artists"][0]["name"]);
                                }
                            }
                            if ((*resp)["item"].contains("album"))
                            {
                                if ((*resp)["item"]["album"].contains("name"))
                                {
                                    m_currentlyPlayingInfo.currentTrack.album =
                                        std::string((*resp)["item"]["album"]["name"]);
                                }
                                if ((*resp)["item"]["album"].contains("images") &&
                                    (*resp)["item"]["album"]["images"].is_array() &&
                                    !(*resp)["item"]["album"]["images"].empty())
                                {
                                    for (auto& img : (*resp)["item"]["album"]["images"])
                                    {
                                        if (img["width"] < 360 && img["height"] < 360)
                                        {
                                            // Get the first image (usually the largest)
                                            m_currentlyPlayingInfo.album_art.url =
                                                std::string(img["url"]);

                                            break;
                                        }
                                    }
                                }
                            }
                            if ((*resp)["item"].contains("duration_ms"))
                            {
                                m_currentlyPlayingInfo.currentTrack.duration_ms =
                                    (*resp)["item"]["duration_ms"];
                            }
                            if ((*resp)["item"].contains("uri"))
                            {
                                // New track started playing, handle album art
                                if (m_currentlyPlayingInfo.currentTrack.track_uri !=
                                    std::string((*resp)["item"]["uri"]))
                                {
                                    requestQueue();

                                    // Switch to pre-loaded next album art or get current
                                    if (m_nextAlbumArt.isValid() &&
                                        m_nextAlbumArt.url == m_currentlyPlayingInfo.album_art.url)
                                    {
                                        switchToNextAlbumArt();
                                    }
                                    else
                                    {
                                        getCurrentAlbumArt();
                                    }
                                }
                                m_currentlyPlayingInfo.currentTrack.track_uri =
                                    std::string((*resp)["item"]["uri"]);
                            }

                            if (resp->contains("progress_ms"))
                            {
                                m_currentlyPlayingInfo.progress_ms  = (*resp)["progress_ms"];
                                m_currentlyPlayingInfo.timestamp_ms = getCurrentTimestampMs();
                            }
                        }

                        if (resp->contains("context"))
                        {
                            if ((*resp)["context"].contains("uri"))
                            {
                                m_currentlyPlayingInfo.context_uri =
                                    std::string((*resp)["context"]["uri"]);
                            }
                        }
                        ESP_LOGI(TAG, "Updated currently playing: %s",
                                 m_currentlyPlayingInfo.toString().c_str());
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get currently playing track");
                    }
                }
                break;

                case SpotifyActionType::UpdatePlaybackState:
                {
                    auto resp = m_spotifyClient.get("me/player");
                    if (resp)
                    {
                        if (resp->contains("error"))
                        {
                            ESP_LOGE(TAG, "Error getting playback state: %s",
                                     (*resp)["error"]["message"].get<std::string>().c_str());
                            break;
                        }
                        else if (resp->is_null() || resp->empty())
                        {
                            ESP_LOGI(TAG, "No playback state available");
                            break;
                        }

                        // m_playbackState.is_playing
                        if (resp->contains("is_playing"))
                        {
                            m_playbackState.is_playing = (*resp)["is_playing"];
                        }
                        // m_playbackState.shuffle_state
                        if (resp->contains("shuffle_state"))
                        {
                            m_playbackState.shuffle_state = (*resp)["shuffle_state"];
                        }
                        // m_playbackState.repeat_state
                        if (resp->contains("repeat_state"))
                        {
                            m_playbackState.repeat_state = (*resp)["repeat_state"];
                        }
                        if (resp->contains("device"))
                        {
                            // m_playbackState.device_name
                            if ((*resp)["device"].contains("name"))
                            {
                                m_playbackState.device_name = (*resp)["device"]["name"];
                            }
                            // m_playbackState.device_id
                            if ((*resp)["device"].contains("id"))
                            {
                                m_playbackState.device_id = (*resp)["device"]["id"];
                            }
                            // m_playbackState.volume_percent
                            if ((*resp)["device"].contains("volume_percent"))
                            {
                                m_playbackState.volume_percent =
                                    (*resp)["device"]["volume_percent"];
                                if (m_desiredVolume == -1)
                                {
                                    // first volume fetch, set desired volume to current
                                    m_desiredVolume = m_playbackState.volume_percent;
                                }
                                else if (m_desiredVolume != m_playbackState.volume_percent &&
                                         !m_volumeCmdInProgress)
                                {
                                    if (timeElapsedMs(m_lastVolumeChangeTimeMs) < 2000)
                                    {
                                        // Volume didn't fully update. Send another one.
                                        queueSetVolume();
                                    }
                                    else
                                    {
                                        // external volume change detected. Update desired volume
                                        m_desiredVolume = m_playbackState.volume_percent;
                                    }
                                }
                            }
                            // m_playbackState.supports_volume
                            if ((*resp)["device"].contains("supports_volume"))
                            {
                                m_playbackState.supports_volume =
                                    (*resp)["device"]["supports_volume"];
                            }
                        }
                        ESP_LOGI(TAG, "Updated playback state: %s",
                                 m_playbackState.toString().c_str());
                    }
                    else
                    {
                        ESP_LOGE(TAG, "Failed to get currently playing track");
                    }
                }
                break;

                case SpotifyActionType::GetQueue:
                {
                    auto resp = m_spotifyClient.get("me/player/queue");

                    if (resp && resp->contains("error") == false)
                    {
                        if (resp->contains("queue") && (*resp)["queue"].is_array())
                        {
                            size_t i = 0;

                            for (auto& item : (*resp)["queue"])
                            {
                                if (item.contains("name") && item.contains("artists") &&
                                    item["artists"].is_array() && !item["artists"].empty() &&
                                    item.contains("uri"))
                                {
                                    // Remove bulky fields
                                    if (item.contains("album") &&
                                        item["album"].contains("available_markets"))
                                        item["album"].erase("available_markets");
                                    if (item.contains("available_markets"))
                                        item.erase("available_markets");

                                    std::string artist_name =
                                        item["artists"][0]["name"].get<std::string>();
                                    std::string song_name = item["name"].get<std::string>();
                                    std::string uri       = item["uri"].get<std::string>();

                                    // Extract album art URL
                                    std::string album_art_url;
                                    if (item.contains("album") &&
                                        item["album"].contains("images") &&
                                        item["album"]["images"].is_array() &&
                                        !item["album"]["images"].empty())
                                    {
                                        // Find a suitable size image (preferably < 360px)
                                        for (auto& img : item["album"]["images"])
                                        {
                                            if (img.contains("width") && img.contains("height") &&
                                                img.contains("url"))
                                            {
                                                int width  = img["width"];
                                                int height = img["height"];
                                                if (width <= 360 && height <= 360)
                                                {
                                                    album_art_url = img["url"].get<std::string>();
                                                    break;
                                                }
                                            }
                                        }
                                        // If no small image found, use the first available
                                        if (album_art_url.empty() &&
                                            item["album"]["images"][0].contains("url"))
                                        {
                                            album_art_url = item["album"]["images"][0]["url"]
                                                                .get<std::string>();
                                        }
                                    }

                                    // --- Match and reconcile logic ---
                                    bool matched = false;

                                    // While we have items and current one doesn’t match, erase it
                                    while (i < m_songQueue.size())
                                    {
                                        if (m_songQueue[i]->song_uri == uri)
                                        {
                                            // Update album art URL if it's new or empty
                                            if (!album_art_url.empty() &&
                                                m_songQueue[i]->album_art_url != album_art_url)
                                            {
                                                m_songQueue[i]->album_art_url = album_art_url;
                                            }
                                            matched = true;
                                            break;
                                        }
                                        else
                                        {
                                            m_songQueue.erase(m_songQueue.begin() + i);
                                            // Do NOT increment i, because erase shifts elements left
                                        }
                                    }

                                    // If we didn’t find a match, this is a new song → append it
                                    if (!matched)
                                    {
                                        m_songQueue.push_back(std::make_unique<SpotifyQueueItem>(
                                            song_name, artist_name, uri, album_art_url));
                                    }

                                    // Move to the next position
                                    i++;
                                }
                            }

                            // Remove any extra trailing items beyond the new queue length
                            if (i < m_songQueue.size())
                            {
                                m_songQueue.erase(m_songQueue.begin() + i, m_songQueue.end());
                            }

                            // Trigger next album art download if queue is updated
                            if (!m_songQueue.empty())
                            {
                                getNextAlbumArt();
                            }
                        }
                    }
                }
                break;

                case SpotifyActionType::GetPlaylists:
                {
                    auto get_resp = m_spotifyClient.get(
                        "me/playlists?limit=" + std::to_string(action->int_param2) +
                        "&offset=" + std::to_string(action->int_param));

                    if (get_resp && get_resp->contains("error") == false)
                    {
                        if (get_resp->contains("items") && (*get_resp)["items"].is_array())
                        {
                            // erase all but the first two playlists (Liked Songs and Recently Played)
                            m_playlists.erase(m_playlists.begin() + 2, m_playlists.end());
                            for (auto& item : (*get_resp)["items"])
                            {
                                if (item.contains("name") && item.contains("id"))
                                {
                                    std::string name = item["name"].get<std::string>();
                                    std::string id   = item["id"].get<std::string>();
                                    std::string uri  = item["uri"].get<std::string>();
                                    ESP_LOGI(TAG, "Found playlist: %s (ID: %s)", name.c_str(),
                                             id.c_str());
                                    m_playlists.push_back(
                                        std::make_unique<SpotifyPlaylist>(name, id, uri));
                                }
                            }
                        }
                    }
                }
                break;

                case SpotifyActionType::GetPlaylist:
                {
                    std::string getPlaylistUrl;

                    if (action->str_param.find("spotify:collection:liked") != std::string::npos)
                    {
                        // Liked Songs
                        getPlaylistUrl = "me/tracks?limit=" + std::to_string(action->int_param2) +
                                         "&offset=" + std::to_string(action->int_param);
                    }
                    else if (action->str_param.find("spotify:collection:recently_played") !=
                             std::string::npos)
                    {
                        // Recently Played
                        getPlaylistUrl = "me/player/recently-played?limit=" +
                                         std::to_string(std::min(action->int_param2, 50));
                    }
                    else
                    {
                        // Regular playlist
                        getPlaylistUrl = "playlists/" + action->str_param +
                                         "/tracks?limit=" + std::to_string(action->int_param2) +
                                         "&offset=" + std::to_string(action->int_param);
                    }

                    auto resp = m_spotifyClient.get(getPlaylistUrl);
                    if (resp && resp->contains("error") == false)
                    {
                        if (resp->contains("items") && (*resp)["items"].is_array())
                        {
                            for (auto& item : (*resp)["items"])
                            {
                                if (item.contains("track"))
                                {
                                    auto& track = item["track"];
                                    if (track.contains("name") && track.contains("artists") &&
                                        track["artists"].is_array() && !track["artists"].empty() &&
                                        track.contains("uri"))
                                    {
                                        std::string artist_name =
                                            track["artists"][0]["name"].get<std::string>();
                                        std::string song_name = track["name"].get<std::string>();
                                        std::string uri       = track["uri"].get<std::string>();
                                        ESP_LOGI(TAG, "Playlist Item: %s by %s (URI: %s)",
                                                 song_name.c_str(), artist_name.c_str(),
                                                 uri.c_str());
                                        m_playlistItems.push_back(
                                            std::make_unique<SpotifyPlaylistItem>(
                                                song_name, artist_name, uri, m_activePlaylistId));
                                    }
                                }
                            }
                        }
                    }
                }
                break;

                case SpotifyActionType::GetUserInfo:
                {
                    auto resp = m_spotifyClient.get("me");
                    if (resp && resp->contains("error") == false)
                    {
                        if (resp->contains("display_name"))
                        {
                            m_userInfo.display_name = (*resp)["display_name"];
                        }
                        if (resp->contains("id"))
                        {
                            m_userInfo.user_id = (*resp)["id"];
                        }
                    }
                }
                break;

                case SpotifyActionType::AddToQueue:
                {
                    auto resp =
                        m_spotifyClient.post("me/player/queue?uri=" + action->str_param, "", true);
                    if (resp && resp->contains("error") == false)
                    {
                        ESP_LOGI(TAG, "Added to queue: %s", action->str_param.c_str());
                    }
                }
                break;

                case SpotifyActionType::GetAlbumArt:
                {
                    setLogLevel(ESP_LOG_INFO);

                    if (!m_currentlyPlayingInfo.album_art.url.empty() &&
                        m_currentlyPlayingInfo.album_art.url.find("http") == 0)
                    {
                        ESP_LOGI(TAG, "Fetching Album Art: %s",
                                 m_currentlyPlayingInfo.album_art.url.c_str());

                        HttpClient                   httpClient;
                        std::unique_ptr<std::string> resp = httpClient.performRequestRaw(
                            HTTP_METHOD_GET, m_currentlyPlayingInfo.album_art.url);

                        if (resp)
                        {
                            if (m_currentlyPlayingInfo.album_art.data)
                            {
                                m_currentlyPlayingInfo.album_art.data->clear();
                            }

                            // Decode JPEG/PNG to raw RGB565 data for LVGL
                            std::unique_ptr<std::string> decoded_data = nullptr;
                            uint16_t                     decoded_w = 0, decoded_h = 0;

                            // Check if it's a JPEG (starts with FF D8) or PNG (starts with 89 50 4E 47)
                            if (resp->size() >= 4)
                            {
                                uint8_t* data = (uint8_t*) resp->data();
                                if (data[0] == 0xFF && data[1] == 0xD8)
                                {
                                    // JPEG image
                                    decoded_data = decodeJpegToRgb565(*resp, decoded_w, decoded_h);
                                }
                            }

                            if (decoded_data)
                            {
                                m_currentlyPlayingInfo.album_art.data   = std::move(decoded_data);
                                m_currentlyPlayingInfo.album_art.width  = decoded_w;
                                m_currentlyPlayingInfo.album_art.height = decoded_h;

                                // Update album image structure for LVGL
                                album_image.header.w = decoded_w;
                                album_image.header.h = decoded_h;
                                album_image.header.cf =
                                    LV_IMG_CF_TRUE_COLOR; // RGB565 is considered true color in LVGL
                                album_image.data =
                                    (const uint8_t*) m_currentlyPlayingInfo.album_art.data->data();
                                album_image.data_size =
                                    m_currentlyPlayingInfo.album_art.data->size();

                                // Calculate and set zoom to fit in 360x360 area
                                float zoom = 256.0f * std::min(360.0f / (float) decoded_w,
                                                               360.0f / (float) decoded_h);

                                ui_lvgl_lock(-1);
                                lv_img_set_src(ui_Album_Art_Image, &album_image);
                                lv_img_set_zoom(ui_Album_Art_Image, static_cast<uint16_t>(zoom));
                                ui_lvgl_unlock();
                            }
                            else
                            {
                                // Fallback: use raw data (won't support zoom)
                                m_currentlyPlayingInfo.album_art.data = std::move(resp);
                            }

                            ESP_LOGI(TAG, "Album art processed: %s len=%d  %dx%d",
                                     m_currentlyPlayingInfo.album_art.url.c_str(),
                                     m_currentlyPlayingInfo.album_art.data->size(),
                                     m_currentlyPlayingInfo.album_art.width,
                                     m_currentlyPlayingInfo.album_art.height);
                        }
                    }

                    setLogLevel(previous_level);
                }
                break;

                case SpotifyActionType::GetCurrentAlbumArt:
                {
                    setLogLevel(ESP_LOG_INFO);

                    if (!m_currentlyPlayingInfo.album_art.url.empty())
                    {
                        auto albumArt =
                            downloadAndDecodeAlbumArt(m_currentlyPlayingInfo.album_art.url);
                        if (albumArt)
                        {
                            m_currentlyPlayingInfo.album_art = std::move(*albumArt);
                            displayCurrentAlbumArt();

                            // Start downloading next song's art
                            getNextAlbumArt();
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "No album art URL available for current track");
                    }

                    setLogLevel(previous_level);
                }
                break;

                case SpotifyActionType::GetNextAlbumArt:
                {
                    setLogLevel(ESP_LOG_INFO);

                    std::string nextUrl = getNextSongAlbumArtUrl();
                    if (!nextUrl.empty() && nextUrl != m_nextAlbumArt.url)
                    {
                        auto albumArt = downloadAndDecodeAlbumArt(nextUrl);
                        if (albumArt)
                        {
                            m_nextAlbumArt = std::move(*albumArt);
                            ESP_LOGI(TAG, "Pre-loaded next album art: %dx%d", m_nextAlbumArt.width,
                                     m_nextAlbumArt.height);
                        }
                    }
                    else
                    {
                        ESP_LOGW(TAG, "No next song album art URL available or already cached");
                    }

                    setLogLevel(previous_level);
                }
                break;

                default:
                    break;
            }
            if (action->verbose)
            {
                setLogLevel(previous_level);
            }
            delete action;
        }
        else
        {
            // update periodically if no other actions
            updateCurrentlyPlaying();
            updatePlaybackState();
        }
    }
}
int Spotify::CurrentlyPlayingInfo::getProgress_ms()
{
    if (!Spotify::getInstance().isPlaying())
    {
        return progress_ms;
    }
    uint64_t current_time_ms     = getCurrentTimestampMs();
    int      elapsed_ms          = static_cast<int>(current_time_ms - timestamp_ms);
    int      current_progress_ms = progress_ms + elapsed_ms;
    if (current_progress_ms > currentTrack.duration_ms)
    {
        current_progress_ms = currentTrack.duration_ms;
    }
    return current_progress_ms;
}
int Spotify::CurrentlyPlayingInfo::getRemaining_ms()
{
    return currentTrack.duration_ms - getProgress_ms();
}
float Spotify::CurrentlyPlayingInfo::getProgress_percent()
{
    if (currentTrack.duration_ms == 0)
    {
        return 0;
    }
    return (float) (getProgress_ms()) / (float) currentTrack.duration_ms;
}
bool Spotify::isPlaying()
{
    return m_playbackState.is_playing;
}
std::string Spotify::getCurrentTrack()
{
    return m_currentlyPlayingInfo.currentTrack.name;
}
int Spotify::getTrackProgress_ms()
{
    return m_currentlyPlayingInfo.getProgress_ms();
}
bool Spotify::pause()
{
    updateCurrentlyPlaying();
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Pause, m_verbose};
    QUEUE_ACTION(action);
    return updatePlaybackState();
}
bool Spotify::next()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Next, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::previous()
{
    // If the current track has been playing for more than 5 seconds, seek to the beginning
    if (getCurrentlyPlayingInfo().getProgress_ms() > 5000)
    {
        return seek(0);
    }
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Previous, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::changeVolume(int volumeDiff)
{
    return setVolume(m_desiredVolume + volumeDiff);
}
bool Spotify::setVolume(int volume)
{
    if (m_playbackState.supports_volume == false)
    {
        ESP_LOGI(TAG, "%s does not support volume adjustment", m_playbackState.device_name.c_str());
        return false;
    }

    if (m_desiredVolume == -1)
    {
        ESP_LOGI(TAG, "Wait for first volume fetch before setting volume");
        return false;
    }

    m_desiredVolume = std::max(0, std::min(100, volume));
    if (m_volumeCmdInProgress)
    {
        // volume command already in the queue. Update the desired volume and skip adding another volume command to the queue.
        return true;
    }
    return queueSetVolume();
}
bool Spotify::queueSetVolume()
{
    m_volumeCmdInProgress = true;
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::SetVolume, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::updateCurrentlyPlaying()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::UpdateCurrentlyPlaying, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::play(std::string song_uri, std::string context_uri)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Play, m_verbose};
    action->song_uri      = song_uri;
    action->context_uri   = context_uri;
    QUEUE_ACTION(action);
    updateCurrentlyPlaying();
    return updatePlaybackState();
}
bool Spotify::updatePlaybackState()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::UpdatePlaybackState, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::toggleShuffle()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::ToggleShuffle, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::setRepeatMode(const std::string& mode)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::SetRepeatMode, m_verbose};
    action->str_param     = mode;
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::seek(int position_ms)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::Seek, m_verbose};
    action->int_param     = position_ms;
    QUEUE_ACTION(action);
    getCurrentlyPlayingInfo().progress_ms = position_ms;
    return updateCurrentlyPlaying();
}
bool Spotify::addToQueue(std::string uri)
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::AddToQueue, m_verbose};
    action->str_param     = uri;
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::requestQueue()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetQueue, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::requestPlaylists(size_t offset, size_t limit)
{
    if (m_userInfo.user_id.empty())
    {
        requestUserInfo();
    }

    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetPlaylists, m_verbose};
    action->int_param     = offset;
    action->int_param2    = limit;
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::requestPlaylist(std::string playlist_id, size_t offset, size_t limit)
{
    if (m_userInfo.user_id.empty())
    {
        requestUserInfo();
    }

    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetPlaylist, m_verbose};
    action->str_param     = playlist_id;
    action->int_param     = offset;
    action->int_param2    = limit;
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::requestUserInfo()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetUserInfo, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}
bool Spotify::getAlbumArt()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetAlbumArt, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}

bool Spotify::getCurrentAlbumArt()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetCurrentAlbumArt, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}

bool Spotify::getNextAlbumArt()
{
    SpotifyAction* action = new SpotifyAction{SpotifyActionType::GetNextAlbumArt, m_verbose};
    QUEUE_ACTION_AND_RET(action);
}

void Spotify::displayCurrentAlbumArt()
{
    if (!m_currentlyPlayingInfo.album_art.isValid())
    {
        ESP_LOGW(TAG, "No valid current album art to display");
        return;
    }

    // Update album image structure for LVGL
    album_image.header.w  = m_currentlyPlayingInfo.album_art.width;
    album_image.header.h  = m_currentlyPlayingInfo.album_art.height;
    album_image.header.cf = LV_IMG_CF_TRUE_COLOR; // RGB565 is considered true color in LVGL
    album_image.data      = (const uint8_t*) m_currentlyPlayingInfo.album_art.data->data();
    album_image.data_size = m_currentlyPlayingInfo.album_art.data->size();

    // Calculate and set zoom to fit in 360x360 area
    float zoom = 256.0f * std::min(360.0f / (float) m_currentlyPlayingInfo.album_art.width,
                                   360.0f / (float) m_currentlyPlayingInfo.album_art.height);

    ui_lvgl_lock(-1);
    lv_img_set_src(ui_Album_Art_Image, &album_image);
    lv_img_set_zoom(ui_Album_Art_Image, static_cast<uint16_t>(zoom));
    ui_lvgl_unlock();

    ESP_LOGI(TAG, "Displayed album art: %dx%d, zoom=%.1f", m_currentlyPlayingInfo.album_art.width,
             m_currentlyPlayingInfo.album_art.height, zoom / 256.0f);
}

void Spotify::switchToNextAlbumArt()
{
    if (m_nextAlbumArt.isValid())
    {
        ESP_LOGI(TAG, "Switching to next album art");
        m_currentlyPlayingInfo.album_art = std::move(m_nextAlbumArt);
        m_nextAlbumArt.clear();
        displayCurrentAlbumArt();
    }
    else
    {
        ESP_LOGW(TAG, "No next album art available to switch to");
        m_currentlyPlayingInfo.album_art.clear();
    }
}

std::string Spotify::getNextSongAlbumArtUrl()
{
    if (!m_songQueue.empty())
    {
        return m_songQueue[0]->album_art_url;
    }
    return "";
}
std::string Spotify::TrackInfo::toString()
{
    return "Track: " + name + ", Artist: " + artist + ", Album: " + album;
}
std::string Spotify::CurrentlyPlayingInfo::toString()
{
    return "Currently Playing: " + currentTrack.toString() + ", Context: " + context_uri +
           ", Progress: " + std::to_string(getProgress_ms()) + " ms (" +
           std::to_string(100 * getProgress_percent()) + "%)";
}
std::string Spotify::PlaybackState::toString()
{
    return "Playback State - Playing: " + std::to_string(is_playing) +
           ", Volume: " + std::to_string(volume_percent) + "%" +
           ", Supports_Volume: " + std::to_string(supports_volume) +
           ", Shuffle: " + std::to_string(shuffle_state) + ", Repeat: " + repeat_state +
           ", Device: " + device_name;
}
void Spotify::printStatusString()
{
    printf("%s\n", m_playbackState.toString().c_str());
    printf("%s\n", m_currentlyPlayingInfo.toString().c_str());
}
void Spotify::setLogLevel(esp_log_level_t level)
{
    esp_log_level_set(TAG, level);
    m_spotifyClient.setLogLevel(level);
}

/****************************************************************************/
/************************ SPOTIFY CONSOLE COMMAND ***************************/
/****************************************************************************/
int spotify_cmd(int argc, char** argv);

const char* SPOTIFY_USAGE_STRING = "Spotify command usage:\n"
                                   "  spotify <action>\n"
                                   "    action: 'play', 'pause', 'next', 'previous', 'status', "
                                   "'shuffle', 'update', 'getQueue', 'refreshToken', 'userInfo', "
                                   "'getPlaylists', 'repeat', 'getAlbumArt', 'getNextAlbumArt'\n";
static struct
{
    struct arg_str* action;
    // struct arg_str* inputStr;
    struct arg_end* end;
} s_spotify_cmd_args;
static esp_console_cmd_t s_spotify_cmd_struct{
    .command        = "spotify",
    .help           = "Manually run Spotify actions from console",
    .hint           = NULL,
    .func           = &spotify_cmd,
    .argtable       = &s_spotify_cmd_args,
    .func_w_context = NULL,
    .context        = NULL,
};

void spotify_cmd_init()
{
    // s_spotify_cmd_args = (decltype(s_spotify_cmd_args)) calloc(1, sizeof(*s_spotify_cmd_args));
    s_spotify_cmd_args.action =
        arg_str1(NULL, NULL, "<action>",
                 "action: 'play', 'pause', 'next', 'previous', 'status', 'shuffle', 'update', "
                 "'getQueue', 'refreshToken', 'userInfo', 'getPlaylists', 'repeat', 'getAlbumArt', "
                 "'getNextAlbumArt'");
    s_spotify_cmd_args.end = arg_end(2);

    ESP_ERROR_CHECK(esp_console_cmd_register(&s_spotify_cmd_struct));
}
int spotify_cmd(int argc, char** argv)
{
    int nerrors = arg_parse(argc, argv, (void**) &s_spotify_cmd_args);
    if (nerrors != 0)
    {
        printf(SPOTIFY_USAGE_STRING);
        return 1;
    }

    const char* action = s_spotify_cmd_args.action->sval[0];

    Spotify& sp = Spotify::getInstance();

    bool wasVerbose = sp.isVerbose();
    sp.setVerbose(true);
    if (strcmp(action, "status") == 0)
    {
        sp.printStatusString();
    }
    else if (strcmp(action, "play") == 0)
    {
        sp.play();
    }
    else if (strcmp(action, "pause") == 0)
    {
        sp.pause();
    }
    else if (strcmp(action, "next") == 0)
    {
        sp.next();
    }
    else if (strcmp(action, "previous") == 0)
    {
        sp.previous();
    }
    else if (strcmp(action, "shuffle") == 0)
    {
        sp.toggleShuffle();
    }
    else if (strcmp(action, "getQueue") == 0)
    {
        sp.requestQueue();
    }
    else if (strcmp(action, "update") == 0)
    {
        sp.updateCurrentlyPlaying();
        sp.updatePlaybackState();
    }
    else if (strcmp(action, "refreshToken") == 0)
    {
        sp.refreshToken();
    }
    else if (strcmp(action, "userInfo") == 0)
    {
        sp.requestUserInfo();
    }
    else if (strcmp(action, "getPlaylists") == 0)
    {
        sp.requestPlaylists();
    }
    else if (strcmp(action, "getAlbumArt") == 0)
    {
        sp.getCurrentAlbumArt();
    }
    else if (strcmp(action, "getNextAlbumArt") == 0)
    {
        sp.getNextAlbumArt();
    }
    else if (strcmp(action, "repeat") == 0)
    {
        // toggle between off, context, track
        std::string new_mode;
        if (sp.m_playbackState.repeat_state == "off")
        {
            new_mode = "context";
        }
        else if (sp.m_playbackState.repeat_state == "context")
        {
            new_mode = "track";
        }
        else
        {
            new_mode = "off";
        }
        sp.setRepeatMode(new_mode);
    }
    else
    {
        printf("Unknown action: %s\n", action);
        printf(SPOTIFY_USAGE_STRING);
    }

    sp.setVerbose(wasVerbose);

    return 0;
}
