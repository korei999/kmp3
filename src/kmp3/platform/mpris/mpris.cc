/* https://github.com/cmus/cmus/blob/master/mpris.c */

#include "mpris.hh"

#include "app.hh"

#include <sys/eventfd.h>
#include <sys/poll.h>

#ifdef OPT_BASU
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

using namespace adt;

namespace mpris
{

bool g_bReady = false;
Mutex g_mtx;
CndVar g_cnd;

static sd_bus* s_pBus {};
static int s_fdMpris = -1;
static int s_fdWake = -1;

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
msgAppendDictSX(sd_bus_message* m, const char* a, i64 i)
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
    LogDebug("mpris::togglePause\n");
    app::togglePause();
    return sd_bus_reply_method_return(m, "");
}

static int
next(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    app::g_pPlayer->selectNext();
    return sd_bus_reply_method_return(m, "");
}

static int
prev(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    app::g_pPlayer->selectPrev();
    return sd_bus_reply_method_return(m, "");
}

static int
pause(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    LogDebug("mpris::pause\n");
    app::mixer().pause(true);
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
    LogDebug("mpris::resume\n");
    app::mixer().pause(false);
    return sd_bus_reply_method_return(m, "");
}

static int
seek(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    i64 val = 0;
    CK(sd_bus_message_read_basic(m, 'x', &val));
    f64 fval = f64(val);
    fval /= 1000.0;
    fval += app::mixer().getCurrentMS();
    app::mixer().seekMS(fval);

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
    sprintf(aBuff, "/%" PRIx64, app::g_pPlayer->m_selectedI);

    const char *pPath = NULL;
    i64 val = 0;
    CK(sd_bus_message_read_basic(m, 'o', &pPath));
    CK(sd_bus_message_read_basic(m, 'x', &val));

    if (strncmp(aBuff, pPath, utils::size(aBuff)) == 0)
        app::mixer().seekMS(val / 1000.0);

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
    const char* id = app::g_config.ntsMprisName;
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
    const char* s = app::mixer().isPaused().load(atomic::ORDER::ACQUIRE) ?
        "Paused" : "Playing";
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
        repeatMethodToString(app::g_pPlayer->m_eRepeatMethod).data()
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
    LogDebug("mpris::setLoopStatus(): {}\n", t);

    auto eMethod = app::g_pPlayer->m_eRepeatMethod;
    for (isize i = 0; i < utils::size(mapPlayerRepeatMethodStrings); ++i)
        if (t == mapPlayerRepeatMethodStrings[i])
            eMethod = PLAYER_REPEAT_METHOD(i);

    app::g_pPlayer->m_eRepeatMethod = eMethod;
    app::g_pWin->m_bRedraw = true;
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
    f64 vol = app::mixer().getVolume();
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
    app::mixer().setVolume(vol);
    app::g_pWin->m_bRedraw = true;

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
    i64 t = app::mixer().getCurrentMS();
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
    f64 mul = f64(app::mixer().getChangedSampleRate()) / f64(app::mixer().getSampleRate());
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
    f64 mul = (f64)app::g_config.minSampleRate / (f64)app::mixer().getSampleRate();
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
    f64 mul = (f64)app::g_config.maxSampleRate / (f64)app::mixer().getSampleRate();
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

    if (pl.m_info.sfTitle.size() > 0)
        CK(msgAppendDictSS(reply, "xesam:title", pl.m_info.sfTitle.data()));
    if (pl.m_info.sfAlbum.size() > 0)
        CK(msgAppendDictSS(reply, "xesam:album", pl.m_info.sfAlbum.data()));
    if (pl.m_info.sfArtist.size() > 0)
        CK(msgAppendDictSAS(reply, "xesam:artist", pl.m_info.sfArtist.data()));

    {
        long duration = app::mixer().getTotalMS() * 1000;
        CK(msgAppendDictSX(reply, "mpris:length", duration));
    }

    {
        char aBuff[] = "/1122334455667788";
        auto currIdx = pl.m_selectedI;
        sprintf(aBuff, "/%" PRIx64, currIdx);
        CK(msgAppendDictSO(reply, "mpris:trackid", aBuff));
    }

    CK(sd_bus_message_close_container(reply));

    return 0;
}

static int
trackList(
    [[maybe_unused]] sd_bus_message* m,
    [[maybe_unused]] void* data,
    [[maybe_unused]] sd_bus_error* retError
)
{
    sd_bus_reply_method_return(m, "");
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

[[maybe_unused]] static const sd_bus_vtable s_vtMediaPlayer2TrackList[] {
    SD_BUS_VTABLE_START(0),
    SD_BUS_METHOD("GetTracksMetadata", "ao", "", trackList, 0),
    SD_BUS_METHOD("AddTrack", "sob", "", msgIgnore, 0),
    SD_BUS_METHOD("RemoveTrack", "o", "", msgIgnore, 0),
    SD_BUS_METHOD("GoTo", "o", "", msgIgnore, 0),
    SD_BUS_SIGNAL("TrackListReplaced", "aoo", 0),
    SD_BUS_SIGNAL("TrackAdded", "a{vs}o", 0),
    SD_BUS_SIGNAL("TrackRemoved", "o", 0),
    SD_BUS_SIGNAL("TrackMetadataChanged", "oa{sv}", 0),
    MPRIS_PROP("Tracks", "ao", readFalse),
    MPRIS_PROP("CanEditTracks", "b", readFalse),
    SD_BUS_VTABLE_END,
};

void
init()
{
    static bool s_bReInit = true;
    if (!s_bReInit) return;

    LockGuard lock {&g_mtx};

    s_fdWake = eventfd(0, EFD_NONBLOCK);

    g_bReady = false;
    int res = 0;
    const isize nTries = 50;

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

    // res = sd_bus_add_object_vtable(
    //     s_pBus,
    //     nullptr,
    //     "/org/mpris/MediaPlayer2",
    //     "org.mpris.MediaPlayer2.TrackList",
    //     s_vtMediaPlayer2TrackList,
    //     nullptr
    // );
    // if (res < 0) goto out;

    static char aBuff[64];
    for (isize i = 0; i < nTries; ++i)
    {
        memset(aBuff, 0, sizeof(aBuff));
        print::toSpan(aBuff, "org.mpris.MediaPlayer2.{}_{}", app::g_config.ntsMprisName, i);
        res = sd_bus_request_name(s_pBus, aBuff, 0);
        if (res >= 0) break;
        if ((i >= nTries - 1) && (res < 0))
        {
            app::player().pushErrorMsg({
                "failed to create mpris media player instance", 5000.0, Player::Msg::TYPE::ERROR
            });
            s_bReInit = false;
        }
    }

    s_fdMpris = sd_bus_get_fd(s_pBus);

out:
    if (res < 0)
    {
        sd_bus_unref(s_pBus);
        s_pBus = nullptr;
        s_fdMpris = -1;
        g_bReady = false;

        LogError("{}: {}\n", strerror(-res), "init error");
    }
    else g_bReady = true;
}

void
proc()
{
    g_mtx.lock();

    static pollfd s_aPfds[2] {
        {.fd = s_fdMpris, .events = POLLIN, .revents = {}},
        {.fd = s_fdWake, .events = POLLIN, .revents = {}}
    };

    if (s_pBus && g_bReady)
    {
        g_mtx.unlock();

        int r = poll(s_aPfds, utils::size(s_aPfds), -1);
        if (r < 0) return;

        LockGuard lock {&g_mtx};

        if (s_aPfds[0].revents & POLLIN)
        {
            while (sd_bus_process(s_pBus, nullptr) > 0 && app::g_bRunning)
                ;
        }

        if (s_aPfds[1].revents & POLLIN)
        {
            eventfd_t d = 0;
            int r = eventfd_read(s_fdWake, &d);
            LogDebug("eventfd_read({}): {}\n", r, d);
        }
    }
}

void
wakeUp() noexcept
{
    LockGuard lock {&g_mtx};

    if (s_fdMpris > 0)
    {
        eventfd_write(s_fdWake, 1);
        LogDebug("({}) Waking Up\n", s_fdMpris);
    }
}

void
destroy()
{
    LockGuard lock {&g_mtx};

    if (!s_pBus) return;

    if (s_fdWake > 0) close(s_fdWake);
    sd_bus_flush_close_unref(s_pBus);
    s_pBus = nullptr;
    s_fdMpris = -1;
    g_bReady = false;
}

THREAD_STATUS
pollLoop(void*) noexcept
{
    while (app::g_bRunning)
        mpris::proc();

    return THREAD_STATUS(0);
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
    LockGuard lock {&g_mtx};
    playerPropertyChanged("PlaybackStatus");
}

void
loopStatusChanged()
{
    LockGuard lock {&g_mtx};
    playerPropertyChanged("LoopStatus");
}

void
shuffleChanged()
{
    LockGuard lock {&g_mtx};
    playerPropertyChanged("Shuffle");
}

void
volumeChanged()
{
    LockGuard lock {&g_mtx};
    playerPropertyChanged("Volume");
}

void
seeked()
{
    LockGuard lock {&g_mtx};

    if (!s_pBus) return;
    i64 pos = app::mixer().getCurrentMS() * 1000;
    sd_bus_emit_signal(s_pBus, "/org/mpris/MediaPlayer2", "org.mpris.MediaPlayer2.Player", "Seeked", "x", pos);
}

void
metadataChanged()
{
    LockGuard lock {&g_mtx};

    playerPropertyChanged("Metadata");
    seeked();
}

} /* namespace mpris */
