#pragma once

#include "Player.hh"
#include "audio.hh"

using namespace adt;

namespace app
{

extern bool g_bRunning;
extern int g_argc;
extern char** g_argv;
extern VecBase<String> g_aArgs;

extern Player* g_pPlayer;
extern audio::Mixer* g_pMixer;

extern bool TEST_g_bExitThreadloop;

} /* namespace app */
