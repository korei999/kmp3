#include "app.hh"

namespace app
{

bool g_bRunning {};
int g_argc {};
char** g_argv {};
VecBase<String> g_aArgs {};

Player* g_pPlayer {};
audio::Mixer* g_pMixer {};

bool TEST_g_bExitThreadloop {};

} /* namespace app */
