#ifdef _WIN32
#include "updater.h"
#include "update_util.h"
#include "version.h"
#include "sys.h"
#include "unzip.h"
#include <windows.h>
#include <winhttp.h>
#include <cstdio>
#include <vector>

namespace {
// The repo whose releases are checked. Releases are made by .github/workflows/release.yml.
const char* const kOwner = "AshJamB";
const char* const kRepo = "Earthworm-Jim-Collection";
const char* const kExeName = "EarthwormJimCollection.exe";
const size_t kMinExeSize = 1u << 20;      // a real build is several MB: anything under 1 MB is a broken download

// GET `url` (https), following redirects; the body goes to `out`. False with `err` set on any failure.
bool http_get(const std::string& url, std::vector<uint8_t>& out, std::string& err, size_t max_bytes) {
    out.clear();
    std::wstring wurl = from_utf8(url);
    URL_COMPONENTS uc = {};
    uc.dwStructSize = sizeof(uc);
    wchar_t host[256] = {}, path[2048] = {};
    uc.lpszHostName = host;  uc.dwHostNameLength = 255;
    uc.lpszUrlPath = path;   uc.dwUrlPathLength = 2047;
    if (!WinHttpCrackUrl(wurl.c_str(), (DWORD)wurl.size(), 0, &uc)) { err = "bad URL"; return false; }

    const std::wstring ua = from_utf8(std::string("EarthwormJimCollection/v") + kAppVersion);
    HINTERNET ses = WinHttpOpen(ua.c_str(), WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY, WINHTTP_NO_PROXY_NAME,
                                WINHTTP_NO_PROXY_BYPASS, 0);
    if (!ses) { err = "no network session"; return false; }
    bool ok = false;
    HINTERNET con = WinHttpConnect(ses, host, uc.nPort, 0);
    HINTERNET req = con ? WinHttpOpenRequest(con, L"GET", path, nullptr, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES,
                                             uc.nScheme == INTERNET_SCHEME_HTTPS ? WINHTTP_FLAG_SECURE : 0) : nullptr;
    if (req) {
        DWORD redirects = 10;
        WinHttpSetOption(req, WINHTTP_OPTION_MAX_HTTP_AUTOMATIC_REDIRECTS, &redirects, sizeof(redirects));
        const wchar_t* hdr = L"Accept: application/vnd.github+json\r\n";
        if (!WinHttpSendRequest(req, hdr, (DWORD)-1, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) || !WinHttpReceiveResponse(req, nullptr)) {
            err = "could not reach the server";
        } else {
            DWORD code = 0, sz = sizeof(code);
            WinHttpQueryHeaders(req, WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER, WINHTTP_HEADER_NAME_BY_INDEX, &code, &sz,
                                WINHTTP_NO_HEADER_INDEX);
            if (code != 200) {
                err = code == 404 ? "no release found (the repository may be private or have no releases yet)"
                                  : "server answered " + std::to_string(code);
            } else {
                ok = true;
                for (;;) {
                    DWORD avail = 0;
                    if (!WinHttpQueryDataAvailable(req, &avail)) { ok = false; err = "download interrupted"; break; }
                    if (avail == 0) break;
                    size_t at = out.size();
                    if (at + avail > max_bytes) { ok = false; err = "download too large"; break; }
                    out.resize(at + avail);
                    DWORD got = 0;
                    if (!WinHttpReadData(req, out.data() + at, avail, &got)) { ok = false; err = "download interrupted"; break; }
                    out.resize(at + got);
                }
            }
        }
    } else {
        err = "could not open the request";
    }
    if (req) WinHttpCloseHandle(req);
    if (con) WinHttpCloseHandle(con);
    WinHttpCloseHandle(ses);
    return ok;
}

std::wstring temp_dir() {
    wchar_t buf[MAX_PATH + 2];
    DWORD n = GetTempPathW(MAX_PATH, buf);
    return std::wstring(buf, n);
}

std::wstring exe_path() {
    wchar_t buf[MAX_PATH * 2];
    DWORD n = GetModuleFileNameW(nullptr, buf, MAX_PATH * 2);
    return std::wstring(buf, n);
}

// Writes the helper script that waits for this process to close, swaps the exe (keeping the old one as .bak) and
// starts the new one, then starts it. False if it could not be written or launched.
bool launch_swap_script(const std::wstring& exe, const std::wstring& staged) {
    const std::wstring bat = temp_dir() + L"ejc_update.bat";
    const std::wstring bak = exe + L".bak";
    std::string s = "@echo off\r\n"
                    "ping 127.0.0.1 -n 4 >nul\r\n";
    auto q = [](const std::wstring& w) { return "\"" + to_utf8(w) + "\""; };
    s += "if exist " + q(bak) + " del /f /q " + q(bak) + "\r\n";
    s += "move /y " + q(exe) + " " + q(bak) + " >nul\r\n";
    s += "if errorlevel 1 exit /b 1\r\n";
    s += "move /y " + q(staged) + " " + q(exe) + " >nul\r\n";
    s += "if errorlevel 1 (move /y " + q(bak) + " " + q(exe) + " >nul & exit /b 1)\r\n";
    s += "start \"\" " + q(exe) + "\r\n";
    s += "(goto) 2>nul & del \"%~f0\"\r\n";
    if (!write_file(bat, s.data(), s.size())) return false;

    std::wstring cmd = L"cmd.exe /c \"\"" + bat + L"\"\"";
    std::vector<wchar_t> buf(cmd.begin(), cmd.end());
    buf.push_back(0);
    STARTUPINFOW si = {};
    si.cb = sizeof(si);
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    PROCESS_INFORMATION pi = {};
    // (CREATE_NO_WINDOW is ignored when combined with DETACHED_PROCESS, which is what showed the console window)
    if (!CreateProcessW(nullptr, buf.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
        return false;
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
}  // namespace

Updater::~Updater() { join(); }

void Updater::join() {
    if (th_.joinable()) th_.join();
}

void Updater::set(State s, const std::string& version, const std::string& message) {
    std::lock_guard<std::mutex> lk(mu_);
    st_.state = s;
    if (!version.empty()) st_.version = version;
    st_.message = message;
    st_.fresh = true;
}

Updater::Status Updater::poll() {
    std::lock_guard<std::mutex> lk(mu_);
    Status out = st_;
    st_.fresh = false;
    return out;
}

void Updater::check(bool manual) {
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (busy_) return;
        busy_ = true;
        st_.manual = manual;
        st_.state = State::Checking;
        st_.fresh = false;
    }
    join();
    th_ = std::thread([this]() {
        std::string err;
        std::vector<uint8_t> body;
        const std::string api = std::string("https://api.github.com/repos/") + kOwner + "/" + kRepo + "/releases/latest";
        if (!http_get(api, body, err, 4u << 20)) {
            set(State::Failed, "", err);
        } else {
            const std::string json(body.begin(), body.end());
            const std::string tag = upd::json_string(json, "tag_name");
            const std::string url = upd::find_asset_url(json, "-win64.zip");
            if (tag.empty()) {
                set(State::Failed, "", "the release information could not be read");
            } else if (!upd::version_newer(tag, kAppVersion)) {
                set(State::UpToDate, tag, "");
            } else if (url.empty()) {
                set(State::Failed, "", "release " + tag + " has no Windows download");
            } else {
                {
                    std::lock_guard<std::mutex> lk(mu_);
                    asset_url_ = url;
                }
                set(State::Available, tag, "");
            }
        }
        std::lock_guard<std::mutex> lk(mu_);
        busy_ = false;
    });
}

void Updater::install() {
    std::string url;
    {
        std::lock_guard<std::mutex> lk(mu_);
        if (busy_ || st_.state != State::Available || asset_url_.empty()) return;
        busy_ = true;
        url = asset_url_;
    }
    join();
    set(State::Downloading, "", "");
    th_ = std::thread([this, url]() {
        auto fail = [this](const std::string& why) {
            set(State::Failed, "", why);
            std::lock_guard<std::mutex> lk(mu_);
            busy_ = false;
        };
        std::string err;
        std::vector<uint8_t> zipdata;
        if (!http_get(url, zipdata, err, 200u << 20)) return fail(err);

        // Everything is checked in a scratch file before the working install is touched.
        const std::wstring tmp_zip = temp_dir() + L"ejc_update.zip";
        if (!write_file(tmp_zip, zipdata.data(), zipdata.size())) return fail("could not write the download");
        std::vector<ZipEntry> entries;
        if (!zip_list(tmp_zip, entries)) return fail("the download is not a valid zip");
        const ZipEntry* found = nullptr;
        for (const ZipEntry& e : entries)
            if (upd::ends_with(e.name, std::string("/") + kExeName) || e.name == kExeName) found = &e;
        std::vector<uint8_t> exe_bytes;
        if (!found || !zip_read(tmp_zip, *found, exe_bytes)) return fail("the update does not contain the program");
        if (exe_bytes.size() < kMinExeSize || exe_bytes[0] != 'M' || exe_bytes[1] != 'Z') return fail("the downloaded program looks damaged");

        const std::wstring exe = exe_path();
        const std::wstring staged = exe + L".new";
        if (!write_file(staged, exe_bytes.data(), exe_bytes.size()))
            return fail("could not write next to the program (is the folder read-only?)");
        DeleteFileW(tmp_zip.c_str());
        if (!launch_swap_script(exe, staged)) {
            DeleteFileW(staged.c_str());
            return fail("could not start the update helper");
        }
        set(State::Restarting, "", "");
        std::lock_guard<std::mutex> lk(mu_);
        busy_ = false;
    });
}
#endif
