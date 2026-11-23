#include "ds_mcp.h"

#include <stdio.h>

static const ds_mcp_tool kTools[] = {
    { "tts_synthesize",
      "Synthesize text with DeepSeek TTS. Writes the audio to a file and returns it.",
      "{\"type\":\"object\",\"properties\":{\"text\":{\"type\":\"string\"},"
      "\"voice\":{\"type\":\"string\",\"enum\":[\"stella\",\"tide\"],\"default\":\"stella\"},"
      "\"format\":{\"type\":\"string\",\"enum\":[\"wav\",\"pcm\"],\"default\":\"wav\"},"
      "\"filter_markdown\":{\"type\":\"boolean\",\"default\":true},"
      "\"out_path\":{\"type\":\"string\"}},\"required\":[\"text\"]}" },
    { "asr_transcribe",
      "Transcribe a WAV or Ogg Opus file with DeepSeek ASR. The language is detected.",
      "{\"type\":\"object\",\"properties\":{\"file_path\":{\"type\":\"string\"},"
      "\"out_path\":{\"type\":\"string\"}},\"required\":[\"file_path\"]}" },
    { "config_get",
      "Report the base URL, whether a key is configured, and the output folder.",
      "{\"type\":\"object\",\"properties\":{}}" },
    { "health",
      "Liveness check with the server version.",
      "{\"type\":\"object\",\"properties\":{}}" },
};

const ds_mcp_tool *ds_mcp_tools(int *count) {
    if (count) *count = (int)(sizeof kTools / sizeof kTools[0]);
    return kTools;
}

void ds_mcp_tools_list_json(char *out, size_t cap) {
    size_t o = 0;
    o += (size_t)snprintf(out + o, cap - o, "{\"tools\":[");
    for (size_t i = 0; i < sizeof kTools / sizeof kTools[0] && o < cap; i++) {
        if (i) o += (size_t)snprintf(out + o, cap - o, ",");
        o += (size_t)snprintf(out + o, cap - o,
                              "{\"name\":\"%s\",\"description\":\"%s\",\"inputSchema\":%s}",
                              kTools[i].name, kTools[i].description, kTools[i].schema);
    }
    snprintf(out + o, cap - o, "]}");
}
