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

## Architecture

- **Compositor** (`src/compositor.*`) — Minimal `QWaylandCompositor` subclass. Owns the display output.
- **Chat UI** (`qml/Main.qml`) — Fullscreen QML chat log with text input. Solarized dark theme.
- **Chat model** (`src/chatmodel.*`) — `QAbstractListModel` backing the message list.
- **Ollama client** (`src/ollamaclient.*`) — Streams responses from a local Ollama instance via HTTP.

The design intent is that the LLM will eventually control all window management through MCP tools. The compositor is a canvas the model paints on.
