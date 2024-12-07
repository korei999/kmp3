/* https://github.com/cmus/cmus/blob/master/mpris.c */

#include "mpris.hh"

#include "adt/guard.hh"
#include "app.hh"
#include "adt/logs.hh"

#ifdef USE_BASU
    #include <basu/sd-bus.h>
#else
    #include <systemd/sd-bus.h>
#endif

#include <cinttypes>

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

namespace mpris
{

bool g_bReady = false;
mtx_t g_mtx;
cnd_t g_cnd;

static sd_bus* s_pBus {};
static int s_fdMpris = -1;

static int
msgAppendDictSAS(sd_bus_message* m, const char* a, const char* b)
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
msgAppendDictSimple(sd_bus_message* m, const char* tag, char type, const void* val)
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
msgAppendDictSS(sd_bus_message* m, const char* a, const char* b)
{
    return msgAppendDictSimple(m, a, 's', b);
}

static int
msgAppendDictSX(sd_bus_message* m, const char* a, s64 i)
{
    return msgAppendDictSimple(m, a, 'x', &i);
}

static int
msgAppendDictSO(sd_bus_message* m, const char* a, const char* b)
{
    return msgAppendDictSimple(m, a, 'o', b);
}

static int
msgIgnore(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    return sd_bus_reply_method_return(m, "");
}

static int
writeIgnore(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* value,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    return sd_bus_reply_method_return(value, "");
}

static int
readFalse(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    u32 b = 0;
    return sd_bus_message_append_basic(reply, 'b', &b);
}

static int
togglePause(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    LOG("mpris::togglePause\n");
    audio::MixerTogglePause(app::g_pMixer);
    return sd_bus_reply_method_return(m, "");
}

static int
next(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    PlayerSelectNext(app::g_pPlayer);
    return sd_bus_reply_method_return(m, "");
}

static int
prev(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    PlayerSelectPrev(app::g_pPlayer);
    return sd_bus_reply_method_return(m, "");
}

static int
pause(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    LOG("mpris::pause\n");
    audio::MixerPause(app::g_pMixer, true);
    return sd_bus_reply_method_return(m, "");
}

/* same as pause */
static int
stop(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    return pause(m, data, retError);
}

static int
resume(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    LOG("mpris::resume\n");
    audio::MixerPause(app::g_pMixer, false);
    return sd_bus_reply_method_return(m, "");
}

static int
seek(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    s64 val = 0;
    CK(sd_bus_message_read_basic(m, 'x', &val));
    f64 fval = f64(val);
    fval /= 1000.0;
    fval += audio::MixerGetCurrentMS(app::g_pMixer);
    audio::MixerSeekMS(app::g_pMixer, fval);

    return sd_bus_reply_method_return(m, "");
}

static int
seekAbs(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
	char aBuff[] = "/1122334455667788";
    sprintf(aBuff, "/%" PRIx64, app::g_pPlayer->selected);

	const char *pPath = NULL;
	s64 val = 0;
	CK(sd_bus_message_read_basic(m, 'o', &pPath));
	CK(sd_bus_message_read_basic(m, 'x', &val));

	if (strncmp(aBuff, pPath, utils::size(aBuff)) == 0)
        audio::MixerSeekMS(app::g_pMixer, val / 1000);

	return sd_bus_reply_method_return(m, "");
}

static int
readTrue(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    u32 b = 1;
    return sd_bus_message_append_basic(reply, 'b', &b);
}

static int
identity(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    const char* id = "kmp3";
    return sd_bus_message_append_basic(reply, 's', id);
}

static int
uriSchemes(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* _userdata,
    [[maybe_unused]] sd_bus_error* _ret_error
)
{
    static const char* const schemes[] {{}, {}}; /* has to end with nullptr array */
    return sd_bus_message_append_strv(reply, (char**)schemes);
}
static int
mimeTypes(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
	static const char* const types[] = {nullptr};
	return sd_bus_message_append_strv(reply, (char **)types);
}

static int
playbackStatus(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    const char* s = app::g_pMixer->bPaused ? "Paused" : "Playing";
    return sd_bus_message_append_basic(reply, 's', s);
}

static int
getLoopStatus(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    return sd_bus_message_append_basic(
        reply, 's',
        PlayerRepeatMethodToString(app::g_pPlayer->eReapetMethod).pData
    );
}

static int
setLoopStatus(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* value,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    const char* t = nullptr;
    CK(sd_bus_message_read_basic(value, 's', &t));
    LOG("mpris::setLoopStatus(): {}\n", t);

    auto eMethod = app::g_pPlayer->eReapetMethod;
    for (u64 i = 0; i < utils::size(mapPlayerRepeatMethodStrings); ++i)
        if (t == mapPlayerRepeatMethodStrings[i])
            eMethod = PLAYER_REPEAT_METHOD(i);

    app::g_pPlayer->eReapetMethod = eMethod;
    return sd_bus_reply_method_return(value, "");
}

static int
getVolume(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    f64 vol = app::g_pMixer->volume;
    return sd_bus_message_append_basic(reply, 'd', &vol);
}

static int
setVolume(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* value,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    f64 vol;
    CK(sd_bus_message_read_basic(value, 'd', &vol));
    app::g_pMixer->volume = vol;

    return sd_bus_reply_method_return(value, "");
}

static int
position(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    long t = audio::MixerGetCurrentMS(app::g_pMixer);
    t *= 1000;

    return sd_bus_message_append_basic(reply, 'x', &t);
}

static int
rate(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    f64 mul = f64(app::g_pMixer->changedSampleRate) / f64(app::g_pMixer->sampleRate);
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
minRate(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    f64 mul = (f64)defaults::MIN_SAMPLE_RATE / (f64)app::g_pMixer->sampleRate;
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
maxRate(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    f64 mul = (f64)defaults::MAX_SAMPLE_RATE / (f64)app::g_pMixer->sampleRate;
    return sd_bus_message_append_basic(reply, 'd', &mul);
}

static int
shuffle(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    const uint32_t s = 0;
    return sd_bus_message_append_basic(reply, 'b', &s);
}

static int
metadata(
    [[maybe_unused]] sd_bus* bus,
    [[maybe_unused]] const char* path,
    [[maybe_unused]] const char* interface,
    [[maybe_unused]] const char* property,
    [[maybe_unused]] sd_bus_message* reply,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    CK(sd_bus_message_open_container(reply, 'a', "{sv}"));
    const auto& pl = *app::g_pPlayer;

    if (pl.info.title.size > 0)
        CK(msgAppendDictSS(reply, "xesam:title", pl.info.title.pData));
    if (pl.info.album.size > 0)
        CK(msgAppendDictSS(reply, "xesam:album", pl.info.album.pData));
    if (pl.info.artist.size > 0)
        CK(msgAppendDictSAS(reply, "xesam:artist", pl.info.artist.pData));

    {
        long duration = audio::MixerGetMaxMS(app::g_pMixer) * 1000;
        CK(msgAppendDictSX(reply, "mpris:length", duration));
    }

    {
        char aBuff[] = "/1122334455667788";
        auto currIdx = pl.selected;
        sprintf(aBuff, "/%" PRIx64, currIdx);
        CK(msgAppendDictSO(reply, "mpris:trackid", aBuff));
    }

    CK(sd_bus_message_close_container(reply));

    return 0;
}

static const sd_bus_vtable s_vtMediaPlayer2[] {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Raise", "", "", msgIgnore, 0),
    SD_BUS_METHOD("Quit", "", "", msgIgnore, 0),
    MPRIS_PROP("CanQuit", "b", readFalse),
    MPRIS_WPROP("Fullscreen", "b", readFalse, writeIgnore),
    MPRIS_PROP("CanSetFullscreen", "b", readFalse),
    MPRIS_PROP("CanRaise", "b", readFalse),
    MPRIS_PROP("HasTrackList", "b", readFalse),
    MPRIS_PROP("Identity", "s", identity),
    MPRIS_PROP("SupportedUriSchemes", "as", uriSchemes),
    MPRIS_PROP("SupportedMimeTypes", "as", mimeTypes),
    SD_BUS_VTABLE_END,
};

static const sd_bus_vtable s_vtMediaPlayer2Player[] = {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("Next", "", "", next, 0),
    SD_BUS_METHOD("Previous", "", "", prev, 0),
    SD_BUS_METHOD("Pause", "", "", pause, 0),
    SD_BUS_METHOD("PlayPause", "", "", togglePause, 0),
    SD_BUS_METHOD("Stop", "", "", stop, 0),
    SD_BUS_METHOD("Play", "", "", resume, 0),
    SD_BUS_METHOD("Seek", "x", "", seek, 0),
    SD_BUS_METHOD("SetPosition", "ox", "", seekAbs, 0),
    SD_BUS_METHOD("OpenUri", "s", "", msgIgnore, 0),
    MPRIS_PROP("PlaybackStatus", "s", playbackStatus),
    MPRIS_WPROP("LoopStatus", "s", getLoopStatus, setLoopStatus),
    MPRIS_WPROP("Rate", "d", rate, writeIgnore), /* this one is not used by playerctl afaik */
    MPRIS_WPROP("Shuffle", "b", shuffle, writeIgnore),
    MPRIS_WPROP("Volume", "d", getVolume, setVolume),
    SD_BUS_PROPERTY("Position", "x", position, 0, 0),
    MPRIS_PROP("MinimumRate", "d", minRate),
    MPRIS_PROP("MaximumRate", "d", maxRate),
    MPRIS_PROP("CanGoNext", "b", readTrue),
    MPRIS_PROP("CanGoPrevious", "b", readTrue),
    MPRIS_PROP("CanPlay", "b", readTrue),
    MPRIS_PROP("CanPause", "b", readTrue),
    MPRIS_PROP("CanSeek", "b", readTrue),
    SD_BUS_PROPERTY("CanControl", "b", readTrue, 0, 0),
    MPRIS_PROP("Metadata", "a{sv}", metadata),
    SD_BUS_SIGNAL("Seeked", "x", 0),
    SD_BUS_VTABLE_END,
};

void
init()
{
    guard::Mtx lock(&g_mtx);

    g_bReady = false;
    int res = 0;

    res = sd_bus_default_user(&s_pBus);
    if (res < 0) goto out;

    res = sd_bus_add_object_vtable(
        s_pBus,
        nullptr,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2",
        s_vtMediaPlayer2,
        nullptr
    );
    if (res < 0) goto out;

    res = sd_bus_add_object_vtable(
        s_pBus,
        nullptr,
        "/org/mpris/MediaPlayer2",
        "org.mpris.MediaPlayer2.Player",
        s_vtMediaPlayer2Player,
        nullptr
    );
    if (res < 0) goto out;

    res = sd_bus_request_name(s_pBus, "org.mpris.MediaPlayer2.kmp3", 0);
    s_fdMpris = sd_bus_get_fd(s_pBus);

out:
    if (res < 0)
    {
        sd_bus_unref(s_pBus);
        s_pBus = nullptr;
        s_fdMpris = -1;
        g_bReady = false;

        LOG_WARN("{}: {}\n", strerror(-res), "init error");
    }
    else g_bReady = true;
}

void
proc()
{
    guard::Mtx lock(&g_mtx);

    if (s_pBus && g_bReady)
    {
        while (sd_bus_process(s_pBus, nullptr) > 0 && app::g_bRunning)
            ;
    }
}

void
destroy()
{
    guard::Mtx lock(&g_mtx);

    if (!s_pBus) return;

    sd_bus_unref(s_pBus);
    s_pBus = nullptr;
    s_fdMpris = -1;
    g_bReady = false;
}

static void
playerPropertyChanged(const char* name)
{
    const char* const strv[] = {name, NULL};
    if (s_pBus)
    {
        sd_bus_emit_properties_changed_strv(
            s_pBus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", (char**)strv
        );
        sd_bus_flush(s_pBus);
    }
}

void
playbackStatusChanged()
{
    guard::Mtx lock(&g_mtx);
    playerPropertyChanged("PlaybackStatus");
}

void
loopStatusChanged()
{
    guard::Mtx lock(&g_mtx);
    playerPropertyChanged("LoopStatus");
}

void
shuffleChanged()
{
    guard::Mtx lock(&g_mtx);
    playerPropertyChanged("Shuffle");
}

void
volumeChanged()
{
    guard::Mtx lock(&g_mtx);
    playerPropertyChanged("Volume");
}

void
seeked()
{
    guard::Mtx lock(&g_mtx);

    if (!s_pBus) return;
    s64 pos = audio::MixerGetCurrentMS(app::g_pMixer) * 1000;
    sd_bus_emit_signal(s_pBus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "Seeked", "x", pos);
}

void
metadataChanged()
{
    guard::Mtx lock(&g_mtx);

    playerPropertyChanged("Metadata");
    /* the following is not necessary according to the spec but some
     * applications seem to disregard the spec and expect this to happen */
    seeked();
}

} /* namespace mpris */
