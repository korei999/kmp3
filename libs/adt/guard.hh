#pragma once

#include "Thread.hh"

namespace adt
{
namespace guard
{

class Mtx
{
    Mutex* pMtx {};

public:
    Mtx(Mutex* _pMtx) : pMtx(_pMtx) { pMtx->lock(); }
    ~Mtx() { pMtx->unlock(); }
};

} /* namespace guard */
} /* namespace adt */
