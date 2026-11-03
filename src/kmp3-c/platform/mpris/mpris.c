/* https://github.com/cmus/cmus/blob/master/mpris.c */

#include "mpris.h"

#include "app.h"

#include <sys/eventfd.h>
#include <poll.h>

#include "klib/Ctx.h"
#include "klib/Gpa.h"

#ifdef OPT_BASU
    #include <basu/sd-bus.h>
#elif defined OPT_ELOGIND
    #include <elogind/sd-bus.h>
#else
    #include <systemd/sd-bus.h>
#endif

#define MPRIS_PROP(name, type, read) SD_BUS_PROPERTY(name, type, read, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE)

#define MPRIS_WPROP(name, type, read, write)                                                                           \
    SD_BUS_WRITABLE_PROPERTY(name, type, read, write, 0, SD_BUS_VTABLE_PROPERTY_EMITS_CHANGE)

#define CK(v)                                                                                                          \
    do                                                                                                                 \
    {                                                                                                                  \
        int tmp = (v);                                                                                                 \
        if (tmp < 0)                                                                                                   \
            return tmp;                                                                                                \
    } while (0)

bool platform_mpris_g_bReady;
k_Mutex platform_mpris_g_mtx;
k_CndVar platform_mpris_g_cnd;

static sd_bus* platform_mpris_s_pBus;
static int platform_mpris_s_fdMpris = -1;
static int platform_mpris_s_fdWake = -1;

static int
platform_mpris_msgAppendDictSAS(sd_bus_message* m, const char* a, const char* b)
{
    CK(sd_bus_message_open_container(m, 'e', "sv"));
    CK(sd_bus_message_append_basic(m, 's', a));
    CK(sd_bus_message_open_container(m, 'v', "as"));
    CK(sd_bus_message_open_container(m, 'a', "s"));
    CK(sd_bus_message_append_basic(m, 's', b));
    CK(sd_bus_message_close_container(m));
    CK(sd_bus_message_close_container(m));
    CK(sd_bus_message_close_container(m));
    return 0;
}

static int
platform_mpris_msgAppendDictSimple(sd_bus_message* m, const char* tag, char type, const void* val)
{
    const char s[] = {type, 0};
    CK(sd_bus_message_open_container(m, 'e', "sv"));
    CK(sd_bus_message_append_basic(m, 's', tag));
    CK(sd_bus_message_open_container(m, 'v', s));
    CK(sd_bus_message_append_basic(m, type, val));
    CK(sd_bus_message_close_container(m));
    CK(sd_bus_message_close_container(m));

    return 0;
}

static int
platform_mpris_msgAppendDictSS(sd_bus_message* m, const char* a, const char* b)
{
    return platform_mpris_msgAppendDictSimple(m, a, 's', b);
}

static int
platform_mpris_msgAppendDictSX(sd_bus_message* m, const char* a, int64_t i)
{
    return platform_mpris_msgAppendDictSimple(m, a, 'x', &i);
}

static int
platform_mpris_msgAppendDictSO(sd_bus_message* m, const char* a, const char* b)
{
    return platform_mpris_msgAppendDictSimple(m, a, 'o', b);
}

static int
platform_mpris_msgIgnore(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_writeIgnore(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* value,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    return sd_bus_reply_method_return(value, "");
}

static int
platform_mpris_readFalse(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    uint32_t b = 0;
    return sd_bus_message_append_basic(reply, 'b', &b);
}

static int
platform_mpris_togglePause(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    K_CTX_LOG_DEBUG("mpris_togglePause");
    app_togglePause();
    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_next(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    PlayerSelectNext(app_g_pPlayer);
    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_prev(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    PlayerSelectPrev(app_g_pPlayer);
    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_pause(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    K_CTX_LOG_DEBUG("mpris_pause");
    audio_IMixerPause(app_g_pMixer, true);
    return sd_bus_reply_method_return(m, "");
}

/* same as pause */
static int
platform_mpris_stop(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    return platform_mpris_pause(m, data, retError);
}

static int
platform_mpris_resume(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    K_CTX_LOG_DEBUG("mpris_resume");
    audio_IMixerPause(app_g_pMixer, false);
    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_seek(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    int64_t val = 0;
    CK(sd_bus_message_read_basic(m, 'x', &val));
    double fval = val;
    fval /= 1000.0;
    fval += audio_IMixerGetCurrentMS(app_g_pMixer);
    audio_IMixerSeekMS(app_g_pMixer, fval);

    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_seekAbs(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    char aBuff[] = "/1122334455667788";
    sprintf(aBuff, "/%" PRIx64, app_g_pPlayer->selectedI);

    const char *pPath = NULL;
    int64_t val = 0;
    CK(sd_bus_message_read_basic(m, 'o', &pPath));
    CK(sd_bus_message_read_basic(m, 'x', &val));

    if (strncmp(aBuff, pPath, K_ASIZE(aBuff)) == 0)
        audio_IMixerSeekMS(app_g_pMixer, val / 1000.0);

    return sd_bus_reply_method_return(m, "");
}

static int
platform_mpris_readTrue(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    uint32_t b = 1;
    return sd_bus_message_append_basic(reply, 'b', &b);
}

static int
platform_mpris_identity(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    const char* id = app_g_config.ntsMprisName;
    return sd_bus_message_append_basic(reply, 's', id);
}

static int
platform_mpris_uriSchemes(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* _userdata,
    K_UNUSED sd_bus_error* _ret_error
)
{
    static const char* const schemes[] = {NULL, NULL}; /* has to end with nullptr array */
    return sd_bus_message_append_strv(reply, (char**)schemes);
}
static int
platform_mpris_mimeTypes(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
	static const char* const types[] = {NULL};
	return sd_bus_message_append_strv(reply, (char **)types);
}

static int
platform_mpris_playbackStatus(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    const char* s = k_atomic_U8LoadRelaxed(&app_g_pMixer->atom_bPaused) ? "Paused" : "Playing";
    return sd_bus_message_append_basic(reply, 's', s);
}

static int
platform_mpris_getLoopStatus(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    return sd_bus_message_append_basic(
        reply, 's',
        PLAYER_REPEAT_METHOD_STRINGS[app_g_pPlayer->eRepeatMethod]
    );
}

static int
platform_mpris_setLoopStatus(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* value,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    const char* t = NULL;
    CK(sd_bus_message_read_basic(value, 's', &t));
    K_CTX_LOG_DEBUG("mpris_setLoopStatus(): {nts}", t);

    PLAYER_REPEAT_METHOD eMethod = app_g_pPlayer->eRepeatMethod;
    for (ssize_t i = 0; i < K_ASIZE(PLAYER_REPEAT_METHOD_STRINGS); ++i)
        if (!strcmp(t, PLAYER_REPEAT_METHOD_STRINGS[i]))
            eMethod = i;

    app_g_pPlayer->eRepeatMethod = eMethod;
    if (app_g_pWin) WindowWakeUp(app_g_pWin);
    return sd_bus_reply_method_return(value, "");
}

static int
platform_mpris_getVolume(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    double vol = audio_IMixerGetVolume(app_g_pMixer) / 100.0;
    return sd_bus_message_append_basic(reply, 'd', &vol);
}

static int
platform_mpris_setVolume(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* value,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    double vol;
    CK(sd_bus_message_read_basic(value, 'd', &vol));
    audio_IMixerSetVolume(app_g_pMixer, vol * 100);

    return sd_bus_reply_method_return(value, "");
}

static int
platform_mpris_position(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    int64_t t = audio_IMixerGetCurrentMS(app_g_pMixer) * 1000;
    return sd_bus_message_append_basic(reply, 'x', &t);
}

static int
platform_mpris_rate(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    double mul = (double)app_g_pMixer->changedSampleRate / (double)app_g_pMixer->sampleRate;
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
platform_mpris_minRate(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    double mul = (double)app_g_config.minSampleRate / (double)app_g_pMixer->sampleRate;
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
platform_mpris_maxRate(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    double mul = (double)app_g_config.maxSampleRate / (double)app_g_pMixer->sampleRate;
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
platform_mpris_shuffle(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    const uint32_t s = 0;
    return sd_bus_message_append_basic(reply, 'b', &s);
}

static int
platform_mpris_metadata(
    K_UNUSED sd_bus* bus,
    K_UNUSED const char* path,
    K_UNUSED const char* interface,
    K_UNUSED const char* property,
    K_UNUSED sd_bus_message* reply,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    CK(sd_bus_message_open_container(reply, 'a', "{sv}"));
    const Player* pPl = app_g_pPlayer;

    if (k_StringSize(&pPl->info.sTitle) > 0)
        CK(platform_mpris_msgAppendDictSS(reply, "xesam:title", k_StringDataConst(&pPl->info.sTitle)));
    if (k_StringSize(&pPl->info.sAlbum) > 0)
        CK(platform_mpris_msgAppendDictSS(reply, "xesam:album", k_StringDataConst(&pPl->info.sAlbum)));
    if (k_StringSize(&pPl->info.sArtist) > 0)
        CK(platform_mpris_msgAppendDictSAS(reply, "xesam:artist", k_StringDataConst(&pPl->info.sArtist)));

    {
        int64_t duration = audio_IMixerGetTotalMS(app_g_pMixer) * 1000;
        CK(platform_mpris_msgAppendDictSX(reply, "mpris:length", duration));
    }

    {
        char aBuff[] = "/1122334455667788";
        int64_t currIdx = pPl->selectedI;
        sprintf(aBuff, "/%" PRIx64, currIdx);
        CK(platform_mpris_msgAppendDictSO(reply, "mpris:trackid", aBuff));
    }

    CK(sd_bus_message_close_container(reply));

    return 0;
}

static int
platform_mpris_trackList(
    K_UNUSED sd_bus_message* m,
    K_UNUSED void* data,
    K_UNUSED sd_bus_error* retError
)
{
    sd_bus_reply_method_return(m, "");
    return 0;
}

static const sd_bus_vtable platform_mpris_s_vtMediaPlayer2[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Raise", "", "", platform_mpris_msgIgnore, 0),
    SD_BUS_METHOD("Quit", "", "", platform_mpris_msgIgnore, 0),
    MPRIS_PROP("CanQuit", "b", platform_mpris_readFalse),
    MPRIS_WPROP("Fullscreen", "b", platform_mpris_readFalse, platform_mpris_writeIgnore),
    MPRIS_PROP("CanSetFullscreen", "b", platform_mpris_readFalse),
    MPRIS_PROP("CanRaise", "b", platform_mpris_readFalse),
    MPRIS_PROP("HasTrackList", "b", platform_mpris_readFalse),
    MPRIS_PROP("Identity", "s", platform_mpris_identity),
    MPRIS_PROP("SupportedUriSchemes", "as", platform_mpris_uriSchemes),
    MPRIS_PROP("SupportedMimeTypes", "as", platform_mpris_mimeTypes),
    SD_BUS_VTABLE_END,
};

static const sd_bus_vtable platform_mpris_s_vtMediaPlayer2Player[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Next", "", "", platform_mpris_next, 0),
    SD_BUS_METHOD("Previous", "", "", platform_mpris_prev, 0),
    SD_BUS_METHOD("Pause", "", "", platform_mpris_pause, 0),
    SD_BUS_METHOD("PlayPause", "", "", platform_mpris_togglePause, 0),
    SD_BUS_METHOD("Stop", "", "", platform_mpris_stop, 0),
    SD_BUS_METHOD("Play", "", "", platform_mpris_resume, 0),
    SD_BUS_METHOD("Seek", "x", "", platform_mpris_seek, 0),
    SD_BUS_METHOD("SetPosition", "ox", "", platform_mpris_seekAbs, 0),
    SD_BUS_METHOD("OpenUri", "s", "", platform_mpris_msgIgnore, 0),
    MPRIS_PROP("PlaybackStatus", "s", platform_mpris_playbackStatus),
    MPRIS_WPROP("LoopStatus", "s", platform_mpris_getLoopStatus, platform_mpris_setLoopStatus),
    MPRIS_WPROP("Rate", "d", platform_mpris_rate, platform_mpris_writeIgnore), /* this one is not used by playerctl afaik */
    MPRIS_WPROP("Shuffle", "b", platform_mpris_shuffle, platform_mpris_writeIgnore),
    MPRIS_WPROP("Volume", "d", platform_mpris_getVolume, platform_mpris_setVolume),
    SD_BUS_PROPERTY("Position", "x", platform_mpris_position, 0, 0),
    MPRIS_PROP("MinimumRate", "d", platform_mpris_minRate),
    MPRIS_PROP("MaximumRate", "d", platform_mpris_maxRate),
    MPRIS_PROP("CanGoNext", "b", platform_mpris_readTrue),
    MPRIS_PROP("CanGoPrevious", "b", platform_mpris_readTrue),
    MPRIS_PROP("CanPlay", "b", platform_mpris_readTrue),
    MPRIS_PROP("CanPause", "b", platform_mpris_readTrue),
    MPRIS_PROP("CanSeek", "b", platform_mpris_readTrue),
    SD_BUS_PROPERTY("CanControl", "b", platform_mpris_readTrue, 0, 0),
    MPRIS_PROP("Metadata", "a{sv}", platform_mpris_metadata),
    SD_BUS_SIGNAL("Seeked", "x", 0),
    SD_BUS_VTABLE_END,
};

K_UNUSED static const sd_bus_vtable platform_mpris_s_vtMediaPlayer2TrackList[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetTracksMetadata", "ao", "", platform_mpris_trackList, 0),
    SD_BUS_METHOD("AddTrack", "sob", "", platform_mpris_msgIgnore, 0),
    SD_BUS_METHOD("RemoveTrack", "o", "", platform_mpris_msgIgnore, 0),
    SD_BUS_METHOD("GoTo", "o", "", platform_mpris_msgIgnore, 0),
    SD_BUS_SIGNAL("TrackListReplaced", "aoo", 0),
    SD_BUS_SIGNAL("TrackAdded", "a{vs}o", 0),
    SD_BUS_SIGNAL("TrackRemoved", "o", 0),
    SD_BUS_SIGNAL("TrackMetadataChanged", "oa{sv}", 0),
    MPRIS_PROP("Tracks", "ao", platform_mpris_readFalse),
    MPRIS_PROP("CanEditTracks", "b", platform_mpris_readFalse),
    SD_BUS_VTABLE_END,
};

bool
platform_mpris_init(void)
{
    static bool s_bReInit = true;
    if (!s_bReInit) return platform_mpris_g_bReady;

    k_MutexLock(&platform_mpris_g_mtx);

    platform_mpris_s_fdWake = eventfd(0, EFD_NONBLOCK);

    platform_mpris_g_bReady = false;
    int res = 0;
    const ssize_t nTries = 50;

    res = sd_bus_default_user(&platform_mpris_s_pBus);
    if (res < 0) goto out;

    res = sd_bus_add_object_vtable(
        platform_mpris_s_pBus,
        NULL,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2",
        platform_mpris_s_vtMediaPlayer2,
        NULL
    );
    if (res < 0) goto out;

    res = sd_bus_add_object_vtable(
        platform_mpris_s_pBus,
        NULL,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        platform_mpris_s_vtMediaPlayer2Player,
        NULL
    );
    if (res < 0) goto out;

    static char aBuff[64];
    for (ssize_t i = 0; i < nTries; ++i)
    {
        memset(aBuff, 0, sizeof(aBuff));
        k_print_toBuffer(aBuff, sizeof(aBuff), "org.mpris.MediaPlayer2.{nts}_{i}", app_g_config.ntsMprisName, i);
        res = sd_bus_request_name(platform_mpris_s_pBus, aBuff, 0);
        if (res >= 0) break;
        if ((i >= nTries - 1) && (res < 0))
        {
            PlayerMsg msg = {
                .eType = PLAYER_MSG_TYPE_ERROR,
                .time = 5000 * K_TIME_MSEC,
                .sMsg = k_StringCreateSv(&k_GpaInst()->base, K_SV("failed to create mpris media player instance"))
            };
            PlayerPushErrorMsg(app_g_pPlayer, &msg);
            s_bReInit = false;
        }
    }

    platform_mpris_s_fdMpris = sd_bus_get_fd(platform_mpris_s_pBus);

out:
    if (res < 0)
    {
        sd_bus_unref(platform_mpris_s_pBus);
        platform_mpris_s_pBus = NULL;
        platform_mpris_s_fdMpris = -1;
        platform_mpris_g_bReady = false;

        K_CTX_LOG_DEBUG("{nts}: init error", strerror(-res));
    }
    else platform_mpris_g_bReady = true;

    k_MutexUnlock(&platform_mpris_g_mtx);

    return platform_mpris_g_bReady;
}

void
platform_mpris_proc(void)
{
    k_MutexLock(&platform_mpris_g_mtx);

    struct pollfd s_aPfds[2] = {
        {.fd = platform_mpris_s_fdMpris, .events = POLLIN,},
        {.fd = platform_mpris_s_fdWake, .events = POLLIN,}
    };

    if (platform_mpris_s_pBus && platform_mpris_g_bReady)
    {
        k_MutexUnlock(&platform_mpris_g_mtx);

        int r = poll(s_aPfds, K_ASIZE(s_aPfds), -1);
        if (r < 0) return;

        k_MutexLock(&platform_mpris_g_mtx);

        if (s_aPfds[0].revents & POLLIN)
        {
            while (sd_bus_process(platform_mpris_s_pBus, NULL) > 0 && app_g_vol_bRunning)
                ;
        }

        if (s_aPfds[1].revents & POLLIN)
        {
            eventfd_t d = 0;
            int r = eventfd_read(platform_mpris_s_fdWake, &d);
            K_CTX_LOG_DEBUG("eventfd_read({i}): {u64}", r, d);
        }

        k_MutexUnlock(&platform_mpris_g_mtx);
    }
    else
    {
        k_MutexUnlock(&platform_mpris_g_mtx);
    }
}

void
platform_mpris_wakeUp(void)
{
    k_MutexLock(&platform_mpris_g_mtx);

    if (platform_mpris_s_fdMpris > 0)
    {
        eventfd_write(platform_mpris_s_fdWake, 1);
        K_CTX_LOG_DEBUG("({i}) Waking Up", platform_mpris_s_fdMpris);
    }

    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_deinit(void)
{
    k_MutexLock(&platform_mpris_g_mtx);

    if (!platform_mpris_s_pBus)
    {
        k_MutexUnlock(&platform_mpris_g_mtx);
        return;
    }

    if (platform_mpris_s_fdWake > 0) close(platform_mpris_s_fdWake);
    sd_bus_flush_close_unref(platform_mpris_s_pBus);
    platform_mpris_s_pBus = NULL;
    platform_mpris_s_fdMpris = -1;
    platform_mpris_g_bReady = false;

    k_MutexUnlock(&platform_mpris_g_mtx);
}

K_THREAD_RESULT
platform_mpris_pollLoop(void* pArg)
{
    k_CtxInitArenaForThisThread(K_SIZE_1K*10);

    while (app_g_vol_bRunning)
        platform_mpris_proc();

    k_CtxDestroyArenaForThisThread();

    return (K_THREAD_RESULT){0};
}

void
platform_mpris_initLocks(void)
{
    k_MutexInit(&platform_mpris_g_mtx, K_MUTEX_TYPE_RECURSIVE);
    k_CndVarInit(&platform_mpris_g_cnd);
}

void
platform_mpris_destroyLocks(void)
{
    k_MutexDestroy(&platform_mpris_g_mtx);
    k_CndVarDestroy(&platform_mpris_g_cnd);
}

static void
platform_mpris_playerPropertyChanged(const char* name)
{
    const char* const strv[] = {name, NULL};
    if (platform_mpris_s_pBus)
    {
        sd_bus_emit_properties_changed_strv(
            platform_mpris_s_pBus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", (char**)strv
        );
        sd_bus_flush(platform_mpris_s_pBus);
    }

    if (app_g_pWin) WindowWakeUp(app_g_pWin);
}

void
platform_mpris_playbackStatusChanged(void)
{
    k_MutexLock(&platform_mpris_g_mtx);
    platform_mpris_playerPropertyChanged("PlaybackStatus");
    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_loopStatusChanged(void)
{
    k_MutexLock(&platform_mpris_g_mtx);
    platform_mpris_playerPropertyChanged("LoopStatus");
    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_shuffleChanged(void)
{
    k_MutexLock(&platform_mpris_g_mtx);
    platform_mpris_playerPropertyChanged("Shuffle");
    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_volumeChanged(void)
{
    k_MutexLock(&platform_mpris_g_mtx);
    platform_mpris_playerPropertyChanged("Volume");
    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_seeked(void)
{
    k_MutexLock(&platform_mpris_g_mtx);

    if (!platform_mpris_s_pBus) goto out;
    int64_t pos = audio_IMixerGetCurrentMS(app_g_pMixer) * 1000;
    sd_bus_emit_signal(platform_mpris_s_pBus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "Seeked", "x", pos);

    if (app_g_pWin) WindowWakeUp(app_g_pWin);

out:
    k_MutexUnlock(&platform_mpris_g_mtx);
}

void
platform_mpris_metadataChanged(void)
{
    k_MutexLock(&platform_mpris_g_mtx);

    platform_mpris_playerPropertyChanged("Metadata");
    platform_mpris_seeked();

    k_MutexUnlock(&platform_mpris_g_mtx);
}
