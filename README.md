# OctPlayer

A Qt Quick media player skeleton for FFmpeg + SDL3.

## Boundaries

- `player/api`: stable public types and `IPlayer`.
- `player/core`: session, state machine, commands, queues, clock and sync policy.
- `player/backend/ffmpeg`: FFmpeg-only implementation details.
- `player/output/audio`: SDL3 audio adapter.
- `player/output/video`: latest-frame mailbox.
- `player/platform/qt`: Qt adapter and `QQuickFramebufferObject` renderer.
- `app`: minimal QML host.

## Status

This is an early playable implementation, not a finished production player. Local FFmpeg demux, software audio/video decode, SDL audio output, RGBA video upload, PTS scheduling, basic seek/flush, and session generation checks are wired. Remaining work includes robust buffering and error recovery, complete format/color handling, hardware surfaces, subtitles, and broader playback tests.

## Dependencies

- CMake 3.21+
- C++20
- Qt 6: Core, Gui, Quick, Qml, OpenGL
- SDL3
- FFmpeg: libavformat, libavcodec, libavutil, libswresample, libswscale
- pkg-config

## Build

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j
./build/app/octplayer-demo
```

For Qt 6, force the OpenGL scene graph backend when required by the platform:

```bash
QT_QUICK_BACKEND=opengl ./build/app/octplayer-demo
```

## Next implementation steps

1. Add playback tests for pause/resume, repeated seek, EOF, and stale-session frames/events.
2. Verify A/V synchronization with varied frame rates, audio-only, video-only, and offset streams.
3. Add robust recovery for decoder/sink errors and non-seekable/network inputs.
4. Support color metadata, direct YUV/NV12 texture upload, and hardware decode surfaces.
5. Add subtitles and stream selection.
