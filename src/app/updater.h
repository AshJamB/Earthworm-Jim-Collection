// Windows self-update from GitHub Releases: checks the repo's latest release in the background, and on request
// downloads its win64 zip, stages the new exe next to the running one and hands over to a small helper script that
// swaps the files once the app has closed. Only works while the repo's releases are publicly readable (the check is
// the same unauthenticated request a browser makes; no token is stored). A failed check is silent unless asked for.
#pragma once
#ifdef _WIN32
#include <mutex>
#include <string>
#include <thread>

class Updater {
public:
    enum class State { Idle, Checking, UpToDate, Available, Failed, Downloading, Restarting };
    struct Status {
        State state = State::Idle;
        std::string version;     // the latest release's version (Available / Downloading)
        std::string message;     // Failed: why
        bool manual = false;     // the check was asked for by the user
        bool fresh = false;      // the state changed since the last poll()
    };

    ~Updater();
    void check(bool manual);     // background check of the latest release; no-op while one is running
    void install();              // after Available: download, stage and launch the swap helper (then state = Restarting)
    Status poll();               // snapshot; clears `fresh`

private:
    void set(State s, const std::string& version, const std::string& message);
    void join();

    std::mutex mu_;
    Status st_;
    std::string asset_url_;
    std::thread th_;
    bool busy_ = false;
};
#endif
