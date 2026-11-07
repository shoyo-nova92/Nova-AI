#pragma once
#include <atomic>

// Signal-only cross-thread control. Session state is modified solely by the
// execution thread after a model call or an already-dispatched action settles.
namespace RuntimeControl {
inline std::atomic<bool> pauseRequested{false};
inline std::atomic<unsigned> activeCalls{0};
inline void requestPause(){pauseRequested.store(true,std::memory_order_release);}
inline bool isPauseRequested(){return pauseRequested.load(std::memory_order_acquire);}
inline void clearPause(){pauseRequested.store(false,std::memory_order_release);}
inline bool active(){return activeCalls.load(std::memory_order_acquire)>0;}
struct Activity {
    Activity(){activeCalls.fetch_add(1,std::memory_order_acq_rel);}
    ~Activity(){activeCalls.fetch_sub(1,std::memory_order_acq_rel);}
    Activity(const Activity&)=delete;
    Activity& operator=(const Activity&)=delete;
};
}
