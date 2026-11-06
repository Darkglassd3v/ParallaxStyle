#include "TestSupport.h"

#include <cstdlib>
#include <new>

// Replaces the global operator new/delete to count the allocations made by a
// thread while tracking is on: the audio thread must never allocate.
namespace
{
    thread_local bool tracking = false;
    thread_local int  allocations = 0;

    void* allocate (std::size_t size)
    {
        if (tracking)
            ++allocations;

        if (void* p = std::malloc (size == 0 ? 1 : size))
            return p;

        throw std::bad_alloc();
    }
}

int  alloc::allocationsOnThisThread()       { return allocations; }
void alloc::setTracking (bool shouldTrack)  { tracking = shouldTrack; }

void* operator new (std::size_t size)                            { return allocate (size); }
void* operator new[] (std::size_t size)                          { return allocate (size); }
void* operator new (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void* operator new[] (std::size_t size, const std::nothrow_t&) noexcept
{
    try { return allocate (size); } catch (...) { return nullptr; }
}
void operator delete (void* p) noexcept                          { std::free (p); }
void operator delete[] (void* p) noexcept                        { std::free (p); }
void operator delete (void* p, std::size_t) noexcept             { std::free (p); }
void operator delete[] (void* p, std::size_t) noexcept           { std::free (p); }
void operator delete (void* p, const std::nothrow_t&) noexcept   { std::free (p); }
void operator delete[] (void* p, const std::nothrow_t&) noexcept { std::free (p); }
