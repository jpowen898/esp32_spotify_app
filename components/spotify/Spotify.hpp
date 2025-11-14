/**
 * @file Spotify.hpp
 * @brief Spotify Web API wrapper class for ESP32 applications
 * @author Parker Owen
 * @date 2025
 *
 * This file contains the Spotify class which provides a high-level interface
 * for interacting with the Spotify Web API on ESP32 devices. It handles
 * playback control, track information, and user interaction with Spotify.
 */

#ifndef __SPOTIFY_HPP__
#define __SPOTIFY_HPP__

#include <SpotifyClient.hpp>
#include <vector>
#include <SpotifyQueueItem.hpp>
#include <SpotifyPlaylist.hpp>
#include <SpotifyPlaylistItem.hpp>
#include <atomic>

using json = nlohmann::json;

/**
 * @class Spotify
 * @brief High-level wrapper for Spotify Web API functionality
 *
 * This singleton class provides a simplified interface for controlling Spotify
 * playback, retrieving track information, and managing playback state on ESP32
 * devices. It abstracts the complexity of the underlying SpotifyClient and
 * provides structured data types for common Spotify operations.
 */
class Spotify
{
private:
    /**
     * @brief Private constructor for singleton pattern
     */
    Spotify();

    /**
     * @brief Private destructor for singleton pattern
     */
    ~Spotify();

public:
    /**
     * @brief Start the Spotify task for handling actions. This should only be called once.
     */
    void start_task();

    /**
     * @struct TrackInfo
     * @brief Contains information about a Spotify track
     */
    struct TrackInfo
    {
        std::string name;        ///< Track name
        std::string artist;      ///< Primary artist name
        std::string album;       ///< Album name
        std::string track_uri;   ///< Spotify URI for the track
        int         duration_ms; ///< Track duration in milliseconds

        /**
         * @brief Convert track information to string representation
         * @return String containing formatted track information
         */
        std::string toString();
    };
    /**
     * @struct AlbumArt
     * @brief Contains album art data and dimensions
     */
    struct AlbumArt
    {
        std::unique_ptr<std::string> data;   ///< Raw RGB565 album art image data
        uint16_t                     width;  ///< Image width in pixels
        uint16_t                     height; ///< Image height in pixels
        std::string                  url;    ///< URL of the album art image

        AlbumArt() : width(0), height(0) {}

        bool isValid() const
        {
            return data && !data->empty() && width > 0 && height > 0;
        }
        void clear()
        {
            data.reset();
            width  = 0;
            height = 0;
            url.clear();
        }
    };

    /**
     * @struct CurrentlyPlayingInfo
     * @brief Contains information about the currently playing track and playback progress
     */
    struct CurrentlyPlayingInfo
    {
        TrackInfo   currentTrack; ///< Information about the currently playing track
        std::string context_uri;  ///< URI of the context (playlist, album, etc.) being played
        int         progress_ms;  ///< Playback progress in milliseconds at timestamp
        uint64_t    timestamp_ms; ///< Timestamp when progress was last updated
        AlbumArt    album_art;    ///< Album art for the currently playing track

        /**
         * @brief Get current playback progress accounting for elapsed time
         * @return Current progress in milliseconds
         */
        int getProgress_ms();

        /**
         * @brief Get current playback progress accounting for elapsed time
         * @return Current progress in milliseconds
         */
        int getRemaining_ms();

        /**
         * @brief Get current playback progress as percentage
         * @return Progress as percentage (0-100)
         */
        float getProgress_percent();

        /**
         * @brief Convert currently playing info to string representation
         * @return String containing formatted playback information
         */
        std::string toString();
    };
    /**
     * @struct PlaybackState
     * @brief Contains information about the current playback state and device
     */
    struct PlaybackState
    {
        bool        is_playing;      ///< Whether playback is currently active
        bool        shuffle_state;   ///< Whether shuffle mode is enabled
        std::string repeat_state;    ///< Repeat mode ("off", "context", "track")
        int         volume_percent;  ///< Current volume level (0-100)
        bool        supports_volume; ///< Whether the device supports volume control
        std::string device_name;     ///< Name of the active playback device
        std::string device_id;       ///< ID of the active playback device

        /**
         * @brief Convert playback state to string representation
         * @return String containing formatted playback state information
         */
        std::string toString();
    };

    /**
     * @struct UserInfo
     * @brief Contains information about the Spotify user
     */
    struct UserInfo
    {
        std::string display_name; ///< User's display name
        std::string user_id;      ///< User's Spotify ID
    };

    /**
     * @brief Get the singleton instance of the Spotify class
     * @return Reference to the singleton Spotify instance
     */
    static Spotify& getInstance()
    {
        static Spotify instance;
        return instance;
    }

    /**
     * @brief Check if music is currently playing
     * @return true if music is playing, false otherwise
     */
    bool isPlaying();

    /**
     * @brief Start or resume playback
     * @return true if successful, false otherwise
     */
    bool play(std::string song_uri = "", std::string context_uri = "");

    /**
     * @brief Pause playback
     * @return true if successful, false otherwise
     */
    bool pause();

    /**
     * @brief Skip to the next track
     * @return true if successful, false otherwise
     */
    bool next();

    /**
     * @brief Skip to the previous track
     * @return true if successful, false otherwise
     */
    bool previous();

    /**
     * @brief Seek to a specific position in the currently playing track
     * @param position_ms Position in milliseconds to seek to
     * @return true if successful, false otherwise
     */
    bool seek(int position_ms);

    /**
     * @brief Set the playback volume
     * @param volumeDiff Amount to change the volume (-100 to 100)
     * @return true if successful, false otherwise
     */
    bool changeVolume(int volumeDiff);

    /**
     * @brief Set the playback volume
     * @param volume Volume level (0-100)
     * @return true if successful, false otherwise
     */
    bool setVolume(int volume);

    /**
     * @brief Get the name of the currently playing track
     * @return String containing the current track name
     */
    std::string getCurrentTrack();

    /**
     * @brief Get the current playback progress in milliseconds
     * @return Current progress in milliseconds
     */
    int getTrackProgress_ms();

    /**
     * @brief Toggle shuffle mode on/off
     * @return true if successful, false otherwise
     */
    bool toggleShuffle();

    /**
     * @brief Set the repeat mode
     * @param mode Repeat mode ("off", "context", "track")
     * @return true if successful, false otherwise
     */
    bool setRepeatMode(const std::string& mode);

    /**
     * @brief Add a track to the playback queue
     * @param uri Track URI to add to queue
     * @return true if successful, false otherwise
     */
    bool addToQueue(std::string uri);

    /**
     * @brief Request recently played tracks
     * @param index Starting index for the request
     * @param limit Maximum number of tracks to retrieve
     * @return true if successful, false otherwise
     */
    bool requestRecentlyPlayed(size_t index = 0, size_t limit = 100);

    /**
     * @brief Request the current playback queue
     * @return true if successful, false otherwise
     */
    bool requestQueue();

    /**
     * @brief Request user playlists
     * @param offset Offset for pagination
     * @param limit Maximum number of playlists to retrieve
     * @return true if successful, false otherwise
     */
    bool requestPlaylists(size_t offset = 0, size_t limit = 20);

    /**
     * @brief Request a specific playlist by ID
     * @param playlist_id Spotify ID of the playlist to retrieve
     * @param offset Offset for pagination
     * @param limit Maximum number of tracks to retrieve
     * @return true if successful, false otherwise
     */
    bool requestPlaylist(std::string playlist_id, size_t offset = 0, size_t limit = 50);

    /**
     * @brief Update the currently playing track information
     * @return true if successful, false otherwise
     */
    bool updateCurrentlyPlaying();

    /**
     * @brief Update the playback state information
     * @return true if successful, false otherwise
     */
    bool updatePlaybackState();

    /**
     * @brief Refresh the Spotify API access token
     * @return true if successful, false otherwise
     */
    bool refreshToken()
    {
        return m_spotifyClient.refreshToken();
    }

    /**
     * @brief Request user information
     * @return true if successful, false otherwise
     */
    bool requestUserInfo();

    /**
     * @brief Print current status information to console
     */
    void printStatusString();

    /**
     * @brief Set the logging level for Spotify operations
     * @param level ESP-IDF log level to set
     */
    void setLogLevel(esp_log_level_t level);

    /**
     * @brief Get the current playback state information
     * @return Reference to the current PlaybackState structure
     */
    PlaybackState& getPlaybackState()
    {
        return m_playbackState;
    }

    /**
     * @brief Get the currently playing track information
     * @return Reference to the CurrentlyPlayingInfo structure
     */
    CurrentlyPlayingInfo& getCurrentlyPlayingInfo()
    {
        return m_currentlyPlayingInfo;
    }

    /**
     * @brief Verbose mode means all queued actions will log detailed information
     * @return True if verbose mode is enabled, false otherwise
     */
    bool isVerbose() const
    {
        return m_verbose;
    }

    /**
     * @brief Verbose mode means all queued actions will log detailed information
     * @param verbose True to enable verbose mode, false to disable
     */
    void setVerbose(bool verbose)
    {
        m_verbose = verbose;
    }

    /**
     * @brief Get the album art URL for the currently playing track (deprecated)
     * @return true if successful
     */
    bool getAlbumArt();

    /**
     * @brief Get and display album art for current track
     * @return true if successful
     */
    bool getCurrentAlbumArt();

    /**
     * @brief Get and cache album art for next track
     * @return true if successful
     */
    bool getNextAlbumArt();

    static void playlist_play_cb(lv_event_t* e);
    static void playlist_queue_cb(lv_event_t* e);
    static void playlist_shuffle_cb(lv_event_t* e);

private:
    const char*    TAG = "Spotify"; ///< Log tag for ESP-IDF logging
    UserInfo       m_userInfo;
    SpotifyClient& m_spotifyClient =
        SpotifyClient::getInstance();              ///< Reference to the underlying Spotify client
    PlaybackState          m_playbackState;        ///< Current playback state information
    CurrentlyPlayingInfo   m_currentlyPlayingInfo; ///< Currently playing track information
    std::vector<TrackInfo> m_queue;                ///< Current playback queue
    std::vector<TrackInfo> m_recentlyPlayed;       ///< Recently played tracks
    bool                   m_verbose = false;      ///< Whether to log detailed information
    std::vector<std::unique_ptr<SpotifyQueueItem>>    m_songQueue;
    std::vector<std::unique_ptr<SpotifyPlaylist>>     m_playlists;
    std::string                                       m_activePlaylistId;
    std::vector<std::unique_ptr<SpotifyPlaylistItem>> m_playlistItems;

    AlbumArt m_nextAlbumArt; ///< Album art for the next track (pre-loaded for caching)

    std::atomic<bool> m_volumeCmdInProgress{false};
    std::atomic<int>  m_desiredVolume{-1};
    uint64_t          m_lastVolumeChangeTimeMs = 0;

    friend class SpotifyPlaylist;
    friend class SpotifyPlaylistItem;
    friend class SpotifyQueueItem;

    /**
     * @brief Get current system timestamp in milliseconds
     * @return Current timestamp in milliseconds since system start
     */
    static uint64_t getCurrentTimestampMs()
    {
        return xTaskGetTickCount() * portTICK_PERIOD_MS;
    }

    /**
     * @brief Calculate elapsed time in milliseconds since a given start time
     * @param startTimeMs Start time in milliseconds
     * @return Elapsed time in milliseconds
     */
    static uint64_t timeElapsedMs(uint64_t startTimeMs)
    {
        return getCurrentTimestampMs() - startTimeMs;
    }

    friend int spotify_cmd(int    argc,
                           char** argv); ///< Allow console command access to private members

    /**
     * @brief Queue a volume set action. This will automatically use the desired volume.
     * @return True if the action was successfully queued, false otherwise
     */
    bool queueSetVolume();

    /**
     * @brief Display the current album art on the UI
     */
    void displayCurrentAlbumArt();

    /**
     * @brief Switch to next album art (currently just clears current)
     */
    void switchToNextAlbumArt();

    /**
     * @brief Get next song's album art URL from the queue
     * @return URL of next song's album art, empty if not available
     */
    std::string getNextSongAlbumArtUrl();

    /**
     * @brief Decode JPEG image data to RGB565 format
     * @param jpeg_data Input JPEG image data
     * @param width Output image width in pixels
     * @param height Output image height in pixels
     * @return Unique pointer to decoded RGB565 image data string
     */
    std::unique_ptr<std::string> decodeJpegToRgb565(const std::string& jpeg_data, uint16_t& width,
                                                    uint16_t& height);

    /**
     * @brief Download and decode album art from a given URL
     * @param url URL of the album art image
     * @return Unique pointer to AlbumArt structure containing decoded image data
     */
    std::unique_ptr<Spotify::AlbumArt> downloadAndDecodeAlbumArt(const std::string& url);

    /**
     * @brief Main task function that processes Spotify actions from the queue
     *
     * This function runs in a separate FreeRTOS task and continuously processes
     * Spotify actions from the action queue. It handles API calls asynchronously
     * to prevent blocking the main application thread.
     */
    void task();

    TaskHandle_t m_task = NULL; ///< Handle to the FreeRTOS task for processing Spotify actions
    QueueHandle_t
        m_actionQueue; ///< Queue for storing Spotify actions to be processed asynchronously

    /**
     * @enum SpotifyActionType
     * @brief Enumeration of all supported Spotify action types
     *
     * This enum defines the different types of actions that can be queued
     * and processed asynchronously by the Spotify task. Each action type
     * corresponds to a specific Spotify Web API operation.
     */
    enum class SpotifyActionType
    {
        None,                   ///< No action (default/invalid state)
        Play,                   ///< Start or resume playback
        Pause,                  ///< Pause current playback
        Next,                   ///< Skip to next track
        Previous,               ///< Skip to previous track
        Seek,                   ///< Seek to a specific position in the currently playing track
        SetVolume,              ///< Change playback volume
        ToggleShuffle,          ///< Toggle shuffle mode on/off
        SetRepeatMode,          ///< Set repeat mode
        UpdateCurrentlyPlaying, ///< Refresh currently playing track information
        UpdatePlaybackState,    ///< Refresh playback state information
        GetQueue,               ///< Get current playback queue
        GetPlaylists,           ///< Get user's playlists
        GetPlaylist,            ///< Get a specific playlist
        GetUserInfo,            ///< Get user information
        AddToQueue,             ///< Add a track to the playback queue
        GetAlbumArt,            ///< Get album art for the currently playing track
        GetCurrentAlbumArt,     ///< Get and display album art for current track
        GetNextAlbumArt         ///< Get and cache album art for next track
    };

    /**
     * @struct SpotifyAction
     * @brief Container for a Spotify action with parameters
     *
     * This structure represents a single action to be executed by the Spotify
     * task. It contains the action type and optional parameters that may be
     * required for specific actions (e.g., volume level, repeat mode).
     */
    struct SpotifyAction
    {
        SpotifyAction() : type(SpotifyActionType::None), int_param(0), int_param2(0), verbose(false)
        {
        }
        SpotifyAction(SpotifyActionType type, bool verbose)
            : type{type}, int_param{0}, int_param2{0}, verbose{verbose}
        {
        }
        SpotifyActionType type;            ///< The type of action to perform
        std::string       str_param;       ///< String parameter (used for repeat mode, etc.)
        std::string       context_uri;     ///< Context URI for play action
        std::string       song_uri;        ///< Song URI for play action
        int               int_param;       ///< Integer parameter (used for volume level, etc.)
        int               int_param2;      ///< Integer parameter (used for volume level, etc.)
        bool              verbose = false; ///< Whether to log detailed action information

        /**
         * @brief Convert action type to human-readable string
         * @return C-string representation of the action type
         */
        const char* actionToString() const
        {
            switch (type)
            {
                case SpotifyActionType::None:
                    return "None";
                case SpotifyActionType::Play:
                    return "Play";
                case SpotifyActionType::Pause:
                    return "Pause";
                case SpotifyActionType::Next:
                    return "Next";
                case SpotifyActionType::Previous:
                    return "Previous";
                case SpotifyActionType::SetVolume:
                    return "SetVolume";
                case SpotifyActionType::Seek:
                    return "Seek";
                case SpotifyActionType::ToggleShuffle:
                    return "ToggleShuffle";
                case SpotifyActionType::SetRepeatMode:
                    return "SetRepeatMode";
                case SpotifyActionType::UpdateCurrentlyPlaying:
                    return "UpdateCurrentlyPlaying";
                case SpotifyActionType::UpdatePlaybackState:
                    return "UpdatePlaybackState";
                case SpotifyActionType::GetQueue:
                    return "GetQueue";
                case SpotifyActionType::GetPlaylists:
                    return "GetPlaylists";
                case SpotifyActionType::GetPlaylist:
                    return "GetPlaylist";
                case SpotifyActionType::GetUserInfo:
                    return "GetUserInfo";
                case SpotifyActionType::AddToQueue:
                    return "AddToQueue";
                case SpotifyActionType::GetAlbumArt:
                    return "GetAlbumArt";
                case SpotifyActionType::GetCurrentAlbumArt:
                    return "GetCurrentAlbumArt";
                case SpotifyActionType::GetNextAlbumArt:
                    return "GetNextAlbumArt";
                default:
                    return "Unknown";
            }
        }

        /**
         * @brief Convert the entire action to a formatted string representation
         * @return String containing action type and all parameters for debugging/logging
         */
        std::string toString()
        {
            return "Action Type: " + std::string(actionToString());
        }
    };
};

#endif // __SPOTIFY_HPP__
