# Plan: HTTP POST of shots to a REST API + web UI for URL/GUID

## Context
The user wants the target to POST every shot as JSON to an external REST API, and to be able to configure the URL and the access GUID from a web page.

The firmware already contains a half-finished version of this:
- `REMOTE_URL`, `REMOTE_KEY` and `REMOTE_ACTIVE` exist in the JSON table (`main/json.c:98-100`) and are stored in NVS.
- `reduce()` (`main/freETarget.c:478`) and `start_new_session()` (`main/freETarget.c:608`) call `http_native_request(json_remote_url, METHOD_POST, ...)`.

It does not work today:
- `json_remote_modes` is never set anywhere, so the POST branch never runs.
- `http_native_request` (`main/TCPIP/http_client.c:558`) has a hard-coded URL (`joshua.10nine.co`) and a hard-coded `X-API-KEY`. It also writes the response into the shared `_xs` buffer, and it blocks shot processing in `reduce()` while the network call runs.
- `json_remote_active` is declared as `int[URL_SIZE]` (`main/json.h:161`), and `KEY_SIZE` is 31 characters, which is too short for a GUID (36 characters).
- HTTPS is disabled (`CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS` is not set).

Decisions from the user:
- How the GUID is sent is **configurable**: either as an `"apikey"` field inside the JSON payload (the default) or as an `X-API-KEY` HTTP header.
- The payload uses the **existing TCPIP format** (`SCORE_TCPIP`).
- Both **HTTP and HTTPS** must work.
- All code and comments are written in **English**, with thorough comments in the existing header-block style (`@function / @brief / @return` plus explanatory text).

## Implementation

### 0. Save the plan
- Copy this plan to `docs/http_post_plan.md` in the repo (`D:\FreeTarget\freETarget\Software\ESP32\docs\`).

### 1. Data/configuration
- `main/json.h`: change `json_remote_active` to `EXTERN int json_remote_active;`, set `KEY_SIZE` to 63, and remove `json_remote_modes`. Add `EXTERN int json_remote_key_mode;` and the defines `REMOTE_KEY_IN_BODY 0` (`"apikey"` field in the JSON) and `REMOTE_KEY_IN_HEADER 1` (`X-API-KEY` header).
- `main/nonvol.h`: add `#define NONVOL_REMOTE_KEY_MODE "REMOTE_KMODE"`. NVS keys are limited to 15 characters, so the name is shortened.
- `main/json.c`: add `{SHOW + LOCK, "\"REMOTE_KEY_MODE\":", &json_remote_key_mode, IS_INT32, 0, NONVOL_REMOTE_KEY_MODE, REMOTE_KEY_IN_BODY, 8}` next to the other `REMOTE_*` entries, keeping the alphabetical order of the table. This also makes the setting persistent and lets it be set over USB/TCP.
- `main/TCPIP/http_client.h`: remove `API_KEY`/`INCLUDE_API_KEY`. Declare `void http_post_init(void);`, `void http_post_shot(const char *json);` and `extern int http_post_last_status;`.
- `main/nonvol.c` + `nonvol.h`: add `nonvol_write_str(char *name, char *value)`. It copies the existing `nonvol_write_i32` (`nonvol.c:444`) but uses `nvs_set_str`.

### 2. Asynchronous POST task (`main/TCPIP/http_client.c`)
- Add a FreeRTOS queue (e.g. 8 entries of 512 bytes each) and an `http_post_task`.
- `http_post_shot(json)` does nothing if `json_remote_active == 0` or the URL is empty. Otherwise it copies the payload into the queue with `xQueueSend(..., 0)` so the call never blocks. If the queue is full, the shot is dropped and a DLT message is logged.
- `http_post_task` waits on the queue. For each shot it:
  - creates a client with `esp_http_client_config_t{ .url = json_remote_url, .crt_bundle_attach = esp_crt_bundle_attach, .timeout_ms = 5000 }`, then sets the method to POST and the header to `Content-Type: application/json`;
  - adds the key, only if `json_remote_key[0] != 0`:
    - `REMOTE_KEY_IN_BODY`: inserts `,"apikey":"<guid>"` just before the last `}` (found with `strrchr`);
    - `REMOTE_KEY_IN_HEADER`: calls `esp_http_client_set_header(client, "X-API-KEY", json_remote_key)` and leaves the body unchanged;
  - sets the post field to the payload;
  - calls `esp_http_client_perform`;
  - stores the status in `http_post_last_status` (-1 on a connection error);
  - logs with `DLT(DLT_COMMUNICATION, printf(...))`, not `_xs`, because the task runs concurrently with the other tasks;
  - calls `esp_http_client_cleanup`.
- `http_post_init()` creates the queue and the task (stack K6, priority `BACKGROUND`). It is called from `app_main` (`main/main.c`) after the other tasks have been started.
- Remove the broken `http_native_request`, or replace its POST branch, so there are not two implementations.

### 3. Calls from shot processing (`main/freETarget.c`)
- Lines 478-482 and 608-612: replace `if (json_remote_modes & REMOTE_MODE_CLIENT) {...http_native_request...}` with `build_json_score(..., SCORE_TCPIP); http_post_shot(_xs);`. The enable check now lives inside `http_post_shot`.

### 4. Web UI (port 81, next to `/menu`)
- New file `main/HTML/remote.html` in the same style as `menu.html`. It holds a `<form action="/remote" method="get">` with:
  - a URL field `name="url" value="^U"`
  - a GUID field `name="key" value="^K"`
  - a select `name="kmode"` with the options `<option value="0" ^0>apikey in JSON body</option>` and `<option value="1" ^1>X-API-KEY header</option>`
  - a checkbox `name="active" ^R` (Active)
  - a SAVE button
  - a "Send test" button (`/remote?test`)
  - a status line `^S` that shows the last HTTP status
- `menu.html`: add a link/button to `/remote`.
- `main/CMakeLists.txt`: add `"html/remote.html"` under `EMBED_FILES`.
- `main/HTML/html.h`: add `remote_html_start/end` and `SIZEOF_remote_HTML`, following the same pattern as the menu page.
- `main/TCPIP/http_services.c`:
  - Add `{EVENT_HTTP_PORT, "Remote server setup", {"/remote", HTTP_GET, service_get_remote, ...}}` to `uri_list`.
  - `service_get_remote` works like this:
    - It reads the query with `httpd_req_get_url_query_str`, then uses `httpd_query_key_value` for `url`, `key`, `kmode` and `active`.
    - It URL-decodes the values with a small helper that reuses the `%XX` logic from `get_url_arg` (`http_server.c:262`) and turns `+` into a space.
    - If `url` is in the query (SAVE), it updates `json_remote_url`, `json_remote_key`, `json_remote_key_mode` and `json_remote_active`, then persists them with `nonvol_write_str`/`nonvol_write_i32`. The checkbox only appears in the query when it is ticked.
    - If `test` is in the query, it calls `http_post_shot("{\"shot\":0,\"test\":1}")`.
    - Finally it renders the page with `http_printf(&remote_html_start, req, SIZEOF_remote_HTML)`.
  - `http_printf`: add these cases. Each one ends with `i = strlen(_xs);`, as the `SELECTED` macro does.
    - `'U'`: strcat `json_remote_url`
    - `'K'`: strcat `json_remote_key`
    - `'0'` / `'1'`: " selected " when `json_remote_key_mode` matches
    - `'R'`: " checked " if active
    - `'S'`: `http_post_last_status`

### 5. HTTPS
- `sdkconfig`: set `CONFIG_ESP_HTTP_CLIENT_ENABLE_HTTPS=y`. Use `idf.py menuconfig` → Component config → ESP HTTP client, or edit the line directly. `CONFIG_MBEDTLS_CERTIFICATE_BUNDLE=y` is already enabled.

## Notes
- POST only works when the target is in WiFi **station mode**, meaning it is joined to a network that can reach the API. It does not work in access-point mode.
- In body mode the JSON field name for the GUID is `"apikey"`. In header mode the header name is `X-API-KEY`.
- The settings can still be set via JSON as well: `{"REMOTE_URL":"https://...","REMOTE_KEY":"<guid>","REMOTE_KEY_MODE":0,"REMOTE_ACTIVE":1}`.

## Verification
1. `idf.py build`: compiles without errors or warnings in the changed files.
2. Flash the target and connect it to WiFi in station mode.
3. Open `http://<target>.local:81/remote`. Enter the URL of a test receiver (e.g. `https://webhook.site/<id>`) and a GUID, tick "Active" and press SAVE.
4. Reload the page and confirm the values are kept. Reboot the target and confirm they are still there (NVS).
5. Press "Send test" with the mode set to "apikey in JSON body". Confirm that webhook.site receives JSON containing `"apikey":"<guid>"` and that the status line shows 200.
   Then switch to "X-API-KEY header", save, and send another test. Confirm the request now carries the `X-API-KEY: <guid>` header and the body has no `apikey` field.
6. Fire shots and confirm each one arrives as `{"shot":..,"session":..,"time":..,"x":..,"y":..,...,"apikey":"..."}`.
7. Stop the server and confirm the target still processes shots normally: a timeout must not block shot processing.
8. Test with both an `http://` and an `https://` URL.
