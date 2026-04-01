# CLAUDE.md

## Project overview

Botnik is an LLM-driven Wayland desktop manager built with C++ and Qt6. The core idea: instead of a traditional window manager, the user interacts with a chat prompt. A local LLM (via Ollama) interprets requests and will eventually control all desktop operations through MCP tool calls.

## Build

```sh
cmake -B build
cmake --build build
```

## Run

```sh
# Standalone (from TTY or as Wayland session)
./build/botnik

# Nested under X11
QT_QPA_PLATFORM=xcb ./build/botnik
```

## Code structure

- `CMakeLists.txt` — Qt6: Core, Gui, Quick, WaylandCompositor, Network
- `src/main.cpp` — Entry point. Creates compositor, chat model, Ollama client, loads QML.
- `src/compositor.h/.cpp` — `QWaylandCompositor` subclass with a single `QWaylandOutput`.
- `src/chatmodel.h/.cpp` — `QAbstractListModel` for chat messages (role + content).
- `src/ollamaclient.h/.cpp` — HTTP streaming client for Ollama's `/api/chat` endpoint.
- `qml/Main.qml` — Fullscreen chat UI. Solarized dark theme.

## Key design decisions

- The compositor's QML scene is the entire UI. No traditional window chrome or tiling logic.
- The LLM will control what appears on screen via MCP tools (not yet implemented).
- Local model (currently qwen2.5:7b) handles simple commands; escalation to a cloud model for complex requests is planned but not yet built.
- Qt6 WaylandCompositor chosen so the compositor, UI, and future client surface embedding all stay in one framework.

## Dependencies

- Qt6 (Core, Gui, Quick, WaylandCompositor, Network)
- Ollama running locally on port 11434
