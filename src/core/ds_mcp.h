// ds_mcp.h - the tool catalogue, shared by the MCP server and the app's self test.
#ifndef DS_MCP_H
#define DS_MCP_H

#include "ds_core.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ds_mcp_tool {
    const char *name;
    const char *description;
    const char *schema;      /* a JSON object */
} ds_mcp_tool;

const ds_mcp_tool *ds_mcp_tools(int *count);
/* The tools/list result object, without the JSON-RPC envelope. */
void ds_mcp_tools_list_json(char *out, size_t cap);

#ifdef __cplusplus
}
#endif

#endif
