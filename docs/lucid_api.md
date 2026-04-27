# Lucid Control Server API

The Lucid Control Server is an HTTP API that runs on port 8765 (localhost only) and provides programmatic access to control and test the Lucid video library application. It allows external tools, scripts, or automated tests to interact with the application without requiring a user interface.

## Overview

- **Port**: 8765
- **Host**: 127.0.0.1 (localhost only - security measure)
- **Protocol**: HTTP
- **Content-Type**: application/json

## Running Lucid in the Background

### Using the background_process Tool

The CodeMax agent provides a `background_process` tool that allows you to start Lucid in the background, send commands to it, and monitor its output. This is useful for testing the API without manually managing processes.

**Start Lucid in the Background:**
```
action: start
command: ./build/lucid
cwd: /home/lee/projects/Programming/projects_C++/Lucid
```

The app takes 10-15 seconds to fully initialize (loading models, starting control server, etc.), so wait before sending commands.

**Poll for Output:**
```
action: poll
process_id: <process_id_from_start>
```

**Send Input to Stdin:**
```
action: send
process_id: <process_id>
input: any input data
```

**List Running Processes:**
```
action: list
```

**Stop the App:**
```
action: stop
process_id: <process_id>
```

### Example Session

Here's a complete example of using the background_process tool to run Lucid and test the API:

**Step 1: Start the app**
```json
{
  "action": "start",
  "command": "./build/lucid",
  "cwd": "/home/lee/projects/Programming/projects_C++/Lucid"
}
```
*Returns: `{"process_id": "abc123", ...}`*

**Step 2: Wait for initialization**
Give the app time to load models (10-15 seconds). Poll the output to check progress:
```json
{
  "action": "poll",
  "process_id": "abc123",
  "clear_output": false
}
```
*Look for "ControlServer] Started on http://127.0.0.1:8765" in the output.*

**Step 3: Test the API with curl**
Once the app is running, use curl commands to interact with it:

```bash
# Check if the server is responding
curl http://127.0.0.1:8765/health

# Get current settings
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}'

# Navigate to settings/Advanced tab
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "navigate_to_settings_tab", "args": {"tab": "advanced"}}'
```

**Step 4: Monitor output while running tests**
```json
{
  "action": "poll",
  "process_id": "abc123",
  "clear_output": false
}
```
This shows the app's log output, including any API commands being processed.

**Step 5: Stop the app when done**
```json
{
  "action": "stop",
  "process_id": "abc123"
}
```

### Scripted Testing Example

Here's a complete bash script that starts Lucid, waits for it to be ready, runs tests, and stops:

```bash
#!/bin/bash

# Start Lucid in background
./build/lucid &
LUCID_PID=$!
echo "Started Lucid with PID: $LUCID_PID"

# Wait for the control server to be ready (check health endpoint)
echo "Waiting for control server..."
for i in {1..30}; do
    if curl -s http://127.0.0.1:8765/health > /dev/null 2>&1; then
        echo "Control server is ready!"
        break
    fi
    sleep 1
done

# Run tests
echo "=== Running API tests ==="

echo "1. Get settings state:"
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}' | jq .

echo "2. Navigate to settings/Models tab:"
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "navigate_to_settings_tab", "args": {"tab": "models"}}'

echo "3. Change thumbnail size:"
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "simulate_settings_change", "args": {"widget": "thumbnail_size", "value": "2"}}'

echo "4. Run stress test:"
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "settings_stress_test", "args": {"iterations": 5, "delay_ms": 300}}'

echo "5. Verify final settings:"
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}' | jq '.thumbnail_size'

# Stop Lucid
echo "Stopping Lucid..."
kill $LUCID_PID
wait $LUCID_PID 2>/dev/null
echo "Done."
```

### Python Testing Example

```python
import requests
import subprocess
import time
import signal

# Start Lucid
print("Starting Lucid...")
proc = subprocess.Popen(
    ['./build/lucid'],
    stdout=subprocess.PIPE,
    stderr=subprocess.STDOUT,
    text=True,
    bufsize=1
)

# Wait for server to be ready
print("Waiting for control server...")
for _ in range(30):
    try:
        r = requests.get('http://127.0.0.1:8765/health', timeout=1)
        if r.status_code == 200:
            print("Server ready!")
            break
    except:
        time.sleep(1)
else:
    print("Timeout waiting for server")
    proc.kill()
    exit(1)

# Run tests
def api_command(command, args=None):
    payload = {"command": command}
    if args:
        payload["args"] = args
    r = requests.post(
        'http://127.0.0.1:8765/api/command',
        json=payload,
        timeout=30
    )
    return r.json()

# Test 1: Get settings state
print("Test 1: Get settings state")
state = api_command("get_settings_state")
print(f"  Current tab: {state.get('current_tab')}")
print(f"  Thumbnail size: {state.get('thumbnail_size')}")

# Test 2: Navigate to settings
print("Test 2: Navigate to settings/Advanced")
result = api_command("navigate_to_settings_tab", {"tab": "advanced"})
print(f"  Result: {result}")

# Test 3: Change a setting
print("Test 3: Change thumbnail size to large (2)")
result = api_command("simulate_settings_change", {
    "widget": "thumbnail_size",
    "value": "2"
})
print(f"  Result: {result}")

# Test 4: Batch settings change
print("Test 4: Batch settings change")
result = api_command("batch_settings_change", {
    "use_gpu": True,
    "vision_interval": 15
})
print(f"  Result: {result}")

# Test 5: Stress test
print("Test 5: Run stress test (5 iterations)")
result = api_command("settings_stress_test", {
    "iterations": 5,
    "delay_ms": 300,
    "test_type": "full"
})
print(f"  Result: {result}")

# Verify final state
print("Verifying final settings...")
state = api_command("get_settings_state")
print(f"  Thumbnail size: {state.get('thumbnail_size')}")
print(f"  Vision scan: always enabled")

# Cleanup
print("Stopping Lucid...")
proc.send_signal(signal.SIGTERM)
proc.wait(timeout=5)
print("Done.")
```

## Base URL

```
http://127.0.0.1:8765
```

## Endpoints

### Health Check

```
GET /health
```

Returns the server status.

**Response:**
```json
{
  "status": "ok",
  "server": "lucid-control"
}
```

---

### Get All Settings

```
GET /api/settings
```

Returns all current application settings including model selections, GPU settings, and watched folders.

**Response:**
```json
{
  "whisper_model": "whisper-tiny",
  "embedding_model": "nomic-embed-text",
  "llm_model": "qwen2-vl-2b",
  "use_gpu": true,
  "max_parallel_jobs": 4,
  "embedding_context_size": 2048,
  "whisper_context_size": 16384,
  "thumbnail_size": 1,
  "vision_scan_enabled": true,  // Always enabled
  "vision_scan_interval": 10,
  "vision_max_per_video": 25,
  "watched_folders": ["/path/to/videos"]
}
```

---

### Set Vision Scan Settings

```
POST /api/settings/vision
```

Configure vision scanning parameters (scan is always enabled).

**Request Body:**
```json
{
  "interval": 10,
  "max_per_video": 25
}
```

**Response:**
```json
{
  "vision_scan_enabled": true,
  "vision_scan_interval": 10,
  "vision_max_per_video": 25,
  "success": true
}
```

---

### Generic Command

```
POST /api/command
```

Execute various commands to control the application. The command name is specified in the JSON body, along with optional arguments.

**Request Body:**
```json
{
  "command": "command_name",
  "args": {
    // Command-specific arguments
  }
}
```

**Response:**
```json
{
  "success": true,
  // Command-specific response data
}
```

---

### Get Pending Notifications

```
GET /api/notifications
```

Returns pending notifications from the server (used for real-time updates).

---

## Command Reference

### Navigation Commands

#### `navigate`

Navigate to a specific page in the application.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| page      | string | Yes      | Target page name |

**Valid Page Names:**
- `home` - Home/Videos page
- `search` - Search page
- `video_detail` - Video player page
- `analysis_queue` - Analysis queue panel
- `settings` - Settings page
- `model_hub` - Model download page
- `chat` - Chat panel

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "navigate", "args": {"page": "settings"}}'
```

---

#### `click_button`

Simulate clicking a sidebar button.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| button    | string | Yes      | Button name |

**Valid Button Names:**
- `add_folder`
- `home`
- `search`
- `analysis_queue`
- `settings`
- `model_hub`
- `chat`
- `refresh`

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "click_button", "args": {"button": "settings"}}'
```

---

### Settings Commands

#### `get_settings_state`

Get the current state of the settings page including which tab is active and all setting values.

**Arguments:** None

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}'
```

**Response:**
```json
{
  "success": true,
  "current_tab": "general",
  "whisper_model": "whisper-tiny",
  "embedding_model": "nomic-embed-text",
  "llm_model": "qwen2-vl-2b",
  "use_gpu": true,
  "max_parallel_jobs": 4,
  "embedding_context_size": 2048,
  "whisper_context_size": 16384,
  "thumbnail_size": 1,
  "vision_scan_enabled": true,  // Always enabled
  "vision_scan_interval": 10,
  "vision_max_per_video": 25,
  "vision_extract_size": 1,
  "watched_folders": []
}
```

---

#### `navigate_to_settings_tab`

Navigate to the settings page and switch to a specific tab.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| tab       | string | No       | Tab name (default: "general") |

**Valid Tab Names:**
- `general` - General settings tab
- `folders` - Watched folders tab
- `models` - AI model selection tab
- `advanced` - Advanced settings tab (GPU, vision scanning)

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "navigate_to_settings_tab", "args": {"tab": "models"}}'
```

---

#### `simulate_settings_change`

Simulate changing a settings widget value. This triggers the widget's callback just like a user interaction would.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| widget    | string | Yes      | Widget identifier |
| value     | string | Yes      | New value |

**Valid Widgets:**
- `whisper_combo` - Whisper model dropdown (model ID)
- `embedding_combo` - Embedding model dropdown (model ID)
- `thumbnail_size` - Thumbnail size (0=small, 1=medium, 2=large)
- `gpu_switch` - GPU acceleration toggle (true/false)
- `vision_switch` - Vision scanning toggle (true/false)
- `vision_interval` - Vision scan interval (integer)
- `vision_max` - Max visions per video (integer)

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "simulate_settings_change", "args": {"widget": "thumbnail_size", "value": "2"}}'
```

---

#### `batch_settings_change`

Change multiple settings in one call, scheduled with small delays between changes.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| whisper_model      | string | No | Whisper model ID |
| embedding_model    | string | No | Embedding model ID |
| thumbnail_size    | int    | No | Thumbnail size (0-2) |
| use_gpu           | bool   | No | GPU acceleration |
| vision_scan_interval| int  | No | Vision scan interval |
| vision_max_per_video| int | No | Max visions per video |
| delay_ms          | int    | No | Delay between changes (default: 100ms) |

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "batch_settings_change",
    "args": {
      "whisper_model": "base",
      "thumbnail_size": 1,
      "use_gpu": true,
      "delay_ms": 200
    }
  }'
```

---

#### `set_thumbnail_size`

Set the thumbnail display size.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| size      | int    | Yes      | 0=small, 1=medium, 2=large |

---

#### `set_use_gpu`

Enable or disable GPU acceleration.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| enabled   | bool   | Yes      | Enable GPU acceleration |

---

#### `set_max_parallel_jobs`

Set the maximum number of parallel video processing jobs.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| jobs      | int    | Yes      | Number of jobs (1-16) |

---

#### `set_whisper_context_size`

Set the Whisper context size.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| size      | int    | Yes      | Context size (1024-65536) |

---

#### `set_embedding_context_size`

Set the embedding context size.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| size      | int    | Yes      | Context size (256-8192) |

---

### Folder Management

#### `add_folder`

Add a watched folder to scan for videos.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| path      | string | Yes      | Folder path |

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "add_folder", "args": {"path": "/home/user/videos"}}'
```

---

#### `remove_folder`

Remove a watched folder.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| path      | string | Yes      | Folder path |

---

#### `get_watched_folders`

Get list of watched folders.

**Arguments:** None

---

### Analysis Commands

#### `get_analysis_queue`

Get analysis queue status with video counts by state.

**Arguments:** None

**Response:**
```json
{
  "success": true,
  "total_videos": 42,
  "pending": 30,
  "processing": 2,
  "completed": 10,
  "failed": 0
}
```

---

#### `get_videos`

Get list of videos with their status.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| limit     | int    | No       | Max results (default: 50) |
| offset   | int    | No       | Offset for pagination |
| status   | string | No       | Filter by status |

**Status Values:** `pending`, `processing`, `completed`, `failed`

---

#### `trigger_analysis`

Mark a video for re-analysis.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| video_id  | string | Yes      | Video ID |

---

#### `rescan_all`

Rescan all watched folders and reset all videos to pending for re-analysis.

**Arguments:** None

---

#### `reset_videos`

Reset videos to pending status.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| status    | string | No       | Filter by status |

---

### Pagination Commands

#### `set_page`

Navigate to a specific page in the video list.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| page      | int    | Yes      | Page number (0-based) |

---

#### `set_page_size`

Change the number of videos per page.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| page_size | int    | Yes      | Videos per page (25, 50, 100, 200) |

---

#### `get_pagination_status`

Get current pagination state.

**Arguments:** None

---

### Search Commands

#### `semantic_search`

Perform semantic search across video transcripts and markers.

**Arguments:**
| Parameter    | Type   | Required | Description |
|--------------|--------|----------|-------------|
| query        | string | Yes      | Search query |
| max_results  | int    | No       | Max results (default: 10) |

---

#### `hybrid_search`

Perform hybrid search (keyword + semantic).

**Arguments:**
| Parameter        | Type   | Required | Description |
|------------------|--------|----------|-------------|
| query            | string | Yes      | Search query |
| max_results      | int    | No       | Max results (default: 10) |
| semantic_weight  | float  | No       | Semantic weight (default: 0.7) |
| keyword_weight   | float  | No       | Keyword weight (default: 0.3) |

---

#### `search_in_video`

Search within a specific video.

**Arguments:**
| Parameter   | Type   | Required | Description |
|------------|--------|----------|-------------|
| video_id   | string | Yes      | Video ID |
| query      | string | Yes      | Search query |
| max_results| int    | No       | Max results (default: 10) |

---

#### `get_search_stats`

Get search index statistics.

**Arguments:** None

---

### Model Management

#### `get_download_status`

Get download status for a specific model.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| model_id   | string | Yes      | Model ID |

---

#### `cancel_download`

Cancel an ongoing model download.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| model_id  | string | Yes      | Model ID |

---

#### `delete_model`

Delete a downloaded model.

**Arguments:**
| Parameter | Type   | Required | Description |
|-----------|--------|----------|-------------|
| model_id  | string | Yes      | Model ID |

---

#### `get_downloaded_models`

Get list of all downloaded models.

**Arguments:** None

---

### Testing Commands

#### `settings_stress_test`

Run a stress test to expose race conditions and crashes when changing settings. This performs rapid navigation and setting changes.

**Arguments:**
| Parameter   | Type   | Required | Description |
|------------|--------|----------|-------------|
| iterations | int    | No       | Number of iterations (1-100, default: 5) |
| delay_ms   | int    | No       | Delay between iterations (default: 300ms) |
| test_type  | string | No       | Type of test: "full", "navigation", "settings_changes" |

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "settings_stress_test",
    "args": {
      "iterations": 10,
      "delay_ms": 300,
      "test_type": "full"
    }
  }'
```

---

#### `simulate_page_transition`

Simulate navigating from one page to another with optional return.

**Arguments:**
| Parameter       | Type   | Required | Description |
|-----------------|--------|----------|-------------|
| to              | string | Yes      | Target page |
| from            | string | No       | Original page (for return) |
| delay_ms        | int    | No       | Delay before return (default: 200ms) |
| return_to_from  | bool   | No       | Return to original page (default: false) |

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "simulate_page_transition",
    "args": {
      "to": "settings",
      "from": "home",
      "delay_ms": 500,
      "return_to_from": true
    }
  }'
```

---

#### `simulate_user_flow`

Execute a predefined user interaction flow.

**Arguments:**
| Parameter     | Type   | Required | Description |
|---------------|--------|----------|-------------|
| flow          | string | Yes      | Flow name |
| step_delay_ms | int    | No       | Delay between steps (default: 500ms) |

**Valid Flow Names:**
- `settings_test` - Navigate to settings, change setting, go to model hub, go back to settings
- `rapid_settings_change` - Rapidly change a setting multiple times
- `tab_navigation` - Cycle through all settings tabs

**Example:**
```bash
curl -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{
    "command": "simulate_user_flow",
    "args": {
      "flow": "settings_test",
      "step_delay_ms": 500
    }
  }'
```

---

### System Commands

#### `get_status`

Get application status.

**Arguments:** None

---

#### `get_current_page`

Get the name of the currently visible page.

**Arguments:** None

---

#### `refresh_videos`

Reload the video list from the database.

**Arguments:** None

---

#### `regenerate_embeddings`

Regenerate embeddings for existing markers that don't have them.

**Arguments:** None

---

#### `quit`

Shutdown the application gracefully.

**Arguments:** None

---

## Error Responses

When a command fails, the response will contain an `error` field:

```json
{
  "success": false,
  "error": "Error description"
}
```

---

## Testing Example

A complete testing workflow might look like:

```bash
# 1. Check current settings
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}'

# 2. Navigate to settings/Advanced tab
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "navigate_to_settings_tab", "args": {"tab": "advanced"}}'

# 3. Change vision scanning settings (scan is always enabled)
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "batch_settings_change", "args": {"vision_interval": 15}}'

# 4. Run stress test to check for crashes
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "settings_stress_test", "args": {"iterations": 20, "delay_ms": 200}}'

# 5. Verify settings changed correctly
curl -s -X POST http://127.0.0.1:8765/api/command \
  -H "Content-Type: application/json" \
  -d '{"command": "get_settings_state"}'
```

---

## Architecture Notes

The Control Server runs in a separate thread from the GTK main loop. Since GTK is not thread-safe, commands that interact with the UI are dispatched to the main thread using `g_idle_add()`. This means:

1. Commands are queued and executed when the main loop is idle
2. Response times may vary based on UI activity
3. Some timing-sensitive commands (like stress tests) use `g_timeout_add()` to schedule iterations

See the source code in `src/lucid/control/control_server.cpp` for implementation details.
