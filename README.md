# dsaudio

DeepSeek TTS and ASR in one C application: a native workbench, a command line
tool and an MCP server, all sharing one core.

```
dsaudio          the CUI workbench (TTS, ASR, MCP, settings)
dsaudio-cli      the same core without a window
dsaudio-mcp      the same core over the Model Context Protocol (stdio)
```

Nothing here is Python or Node at runtime. C99, libcurl, simdjson and miniaudio,
all vendored or system.

## Build

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

Needs a C99 compiler, CMake 3.14+, libcurl development headers, and on macOS
Xcode command line tools. Outputs `build/dsaudio`, `build/dsaudio-cli`,
`build/dsaudio-mcp`.

## Use

```sh
dsaudio-cli config                 # where the key comes from
dsaudio-cli tts "hello there"      # -> WAV in the output folder
dsaudio-cli asr clip.wav           # -> transcript on stdout
dsaudio                           # the window
```

The key is resolved in this order:

1. `$DEEPSEEK_API_KEY` / `$DEEPSEEK_BASE_URL`
2. `~/.config/dsaudio/config.json` (written by Settings in the app)
3. `[providers.DeepSeek]` in `~/.kimi-code/config.toml` — the existing key, read
   only, never written back

## Install the MCP server

```sh
curl -fsSL https://github.com/thiensubs/dsaudio/releases/latest/download/install.sh | sh
```

or, from a checkout:

```sh
build/dsaudio-mcp --install
```

Then add to any MCP client:

```json
{
  "mcpServers": {
    "deepseek-audio": { "command": "/Users/you/.local/bin/dsaudio-mcp", "args": [] }
  }
}
```

Windows: `irm .../install.ps1 | iex`.

## Tools

| Tool | What it does |
|---|---|
| `tts_synthesize` | text → audio file + audio content block |
| `asr_transcribe` | WAV/Ogg Opus → transcript |
| `config_get` | base URL, key presence, output folder |
| `health` | liveness + version |

## Documented limits (measured 2026-11-22)

- TTS: 10 000 billed characters, 64 KiB request body, 30 min of audio. A CJK
  character counts as two billed characters.
- ASR: 8 MiB, 120 s, upload within 10 min. WAV (16-bit PCM) or Ogg Opus.
- A response that ends early is treated as a failure, not as short audio.
- ASR language is auto-detected and is unreliable on very short clips.

## Layout

```
src/core/    buffers, simdjson layer, libcurl transport, config, tts, asr,
             audio peaks, playback, job thread, MCP tool catalogue
src/app/     the CUI workbench
src/mcp/     the MCP server
src/cli/     the command line tool
vendor/      CUI, simdjson, miniaudio
```
