#pragma once

#include <threads.h>

namespace adt
{
namespace guard
{

class Mtx
{
    mtx_t* mtx;

public:
    Mtx(mtx_t* _mtx) : mtx(_mtx) { mtx_lock(_mtx); }
    ~Mtx() { mtx_unlock(mtx); }
};

} /* namespace guard */
} /* namespace adt */
