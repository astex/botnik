# Botnik

An LLM-driven Wayland desktop manager. Instead of traditional window management, Botnik presents a chat prompt. A locally-running LLM interprets your requests and controls the desktop — opening apps, arranging windows, managing files — via tool calls.

## Current state

v0.1: A minimal Qt6 Wayland compositor that renders a fullscreen chat interface and streams responses from a local LLM via Ollama.

## Prerequisites

- Qt6 with Wayland Compositor module (`sudo apt install qt6-wayland-dev`)
- [Ollama](https://ollama.com) with a model pulled (e.g. `ollama pull qwen2.5:7b`)

## Build and run

```sh
cmake -B build
cmake --build build
./build/botnik
```

When running nested inside an existing X11 session:

```sh
QT_QPA_PLATFORM=xcb ./build/botnik
```

## Headless mode

Run botnik without a GUI window using the `--headless` flag or `BOTNIK_HEADLESS=1` environment variable:

```sh
./build/botnik --headless
```

In headless mode, the compositor runs against Qt's offscreen platform. Chat interaction happens via stdin/stdout: type a message on stdin (one line per message), and the assistant's response streams to stdout. Sending EOF (Ctrl-D or closing the pipe) triggers a clean exit.

## Testing

A headless test script verifies the build and basic functionality:

```sh
bash scripts/test-headless.sh
```

Exit code 0 means all tests passed. The script:

1. Builds botnik (`cmake -B build && cmake --build build`)
2. Tests startup and clean shutdown via EOF
3. Tests that the process stays alive while stdin is open
4. If Ollama is reachable, sends a chat message and verifies a response arrives

Tests that require Ollama are skipped (not failed) when Ollama is not running.

## Architecture

- **Compositor** (`src/compositor.*`) — Minimal `QWaylandCompositor` subclass. Owns the display output.
- **Chat UI** (`qml/Main.qml`) — Fullscreen QML chat log with text input. Solarized dark theme.
- **Chat model** (`src/chatmodel.*`) — `QAbstractListModel` backing the message list.
- **Ollama client** (`src/ollamaclient.*`) — Streams responses from a local Ollama instance via HTTP.

The design intent is that the LLM will eventually control all window management through MCP tools. The compositor is a canvas the model paints on.
