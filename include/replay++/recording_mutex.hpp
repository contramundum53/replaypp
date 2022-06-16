#pragma once
#include <mutex>
#include "replay.hpp"
namespace replaypp{

/**
 * @brief A mutex that records its lock/unlock events.
 * This is used for ensuring that the lock/unlock events are in the right order.
 * @tparam replay The replay object used for recording.
 */
template<auto& replay, class mutex = std::mutex>
struct recording_mutex{
    mutex mtx;
    static void mutex_lock(){
        replay.wrap("recording_mutex::lock", []{});
    }

    static void mutex_unlock(){
        replay.wrap("recording_mutex::unlock", []{});
    }
    void lock(){
        mutex_lock();
        mtx.lock();
    }
    void unlock(){
        mtx.unlock();
        mutex_unlock();
    }
};
}