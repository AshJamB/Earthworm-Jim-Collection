// Unit tests for src/app/update_util.h. Build:  g++ -std=c++17 -Isrc tests/update_test.cpp -o update_test
#include <cstdio>
#include "../src/app/update_util.h"

static int failures = 0;
#define CHECK(c) do { if (!(c)) { printf("FAIL line %d: %s\n", __LINE__, #c); failures++; } } while (0)

int main() {
    CHECK(upd::version_newer("v1.2.0", "1.1.9"));
    CHECK(upd::version_newer("0.10.0", "0.9.5"));
    CHECK(upd::version_newer("1.0.1", "1.0"));
    CHECK(!upd::version_newer("1.0", "1.0.0"));
    CHECK(!upd::version_newer("1.0.0", "1.0.0"));
    CHECK(!upd::version_newer("0.9", "0.10"));
    CHECK(!upd::version_newer("", "1.0"));
    CHECK(!upd::version_newer("garbage", "1.0"));
    CHECK(upd::version_newer("v2", "1.9.9"));

    const std::string json =
        "{\"url\":\"https://api.github.com/x\",\"html_url\":\"https://github.com/a/b/releases/tag/v0.2.0\","
        "\"tag_name\": \"v0.2.0\",\"name\":\"Release \\\"two\\\"\","
        "\"assets\":[{\"name\":\"notes.txt\",\"browser_download_url\":\"https://github.com/a/b/releases/download/v0.2.0/notes.txt\"},"
        "{\"name\":\"z.zip\",\"browser_download_url\" : \"https://github.com/a/b/releases/download/v0.2.0/Earthworm-Jim-Collection-v0.2.0-win64.zip\"}]}";
    CHECK(upd::json_string(json, "tag_name") == "v0.2.0");
    CHECK(upd::json_string(json, "name") == "Release \"two\"");
    CHECK(upd::json_string(json, "missing").empty());
    CHECK(upd::find_asset_url(json, "-win64.zip") ==
          "https://github.com/a/b/releases/download/v0.2.0/Earthworm-Jim-Collection-v0.2.0-win64.zip");
    CHECK(upd::find_asset_url(json, ".apk").empty());

    if (failures == 0) printf("update_util: all tests passed\n");
    return failures ? 1 : 0;
}
