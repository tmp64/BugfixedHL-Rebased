#include <amxmodx>
#include <bugfixedapi>

#pragma semicolon 1
#pragma ctrlchar '\'

#define PLUGIN "BHL API Example"
#define VERSION "1.0.0"
#define AUTHOR "BHL"

public plugin_init()
{
	register_plugin(PLUGIN, VERSION, AUTHOR);

	new is_ready = bhl_is_api_ready();
	server_print("[BHL API] Is Ready = %d", is_ready);

    if (is_ready)
    {
        new major, minor, patch;

        if (bhl_get_server_version(major, minor, patch))
            server_print("[BHL API] Version = %d.%d.%d", major, minor, patch);
        else
            server_print("[BHL API] Version get failed");
    }
}
