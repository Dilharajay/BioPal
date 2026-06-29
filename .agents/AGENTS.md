# BioPal - Project Rules and Learnings

## UART CLI Design
- When building or modifying a UART CLI for this project, avoid using blocking `delay()` calls or `Serial.print()` commands in a background loop that interrupt the user's prompt. 
- The CLI state machine should handle characters interactively, echo them, and properly handle backspaces (`\b` and `127`).
- Always implement a clean exit logic (e.g., checking `Serial.available()`) for continuous streaming commands (like `stream` or `monitor`) so the terminal prompt can be restored without getting stuck.

## ESP32 Core Logging
- The Arduino ESP32 core generates verbose background logs (like `WiFiClient.cpp` timeouts) that can flood the serial terminal.
- To disable these core logs globally, ensure `-DCORE_DEBUG_LEVEL=0` is set in `platformio.ini` under `build_flags`.
- Only enable application `Logger` outputs when the user explicitly enables debug mode via the CLI, defaulting to `LOG_LEVEL_NONE` otherwise.

## Backend Stack
- The web dashboard backend has been migrated from Node.js to Python (Flask).
- Do not attempt to use or install `npm` packages for the server. Rely on `requirements.txt` and `python server.py`.
- Server-Sent Events (SSE) are managed using Python's `queue` and Flask generator endpoints (`yield`).
