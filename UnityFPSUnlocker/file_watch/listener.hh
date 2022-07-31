#ifndef LISTENER_HEADER
#define LISTENER_HEADER

#include <cstdint>

#include "dispatcher/ibusiness_event.hh"

#include <absl/container/flat_hash_map.h>

namespace FileWatch {
    class Listener : public IBusinessEvent {
    public:
        Listener();
        virtual void OnReadable(uintptr_t) override final;
        virtual void OnWritable(uintptr_t) override final{};
        virtual void OnError(uintptr_t) override final{};
        virtual void OnClose(uintptr_t) override final{};

        using OnModified = void (*)(int wd);
        static int Register(const char* file_path, OnModified callback);

    private:
        static int inotify_fd_;
        static absl::flat_hash_map<int, OnModified> registered_;
    };
} // namespace FileWatch

#endif // listener.hh