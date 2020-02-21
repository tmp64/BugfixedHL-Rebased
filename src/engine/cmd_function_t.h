#ifndef ENGINE_CMD_FUNCTION_T_H
#define ENGINE_CMD_FUNCTION_T_H

using CmdFunction = void (*)();

namespace CmdFlag
{
enum CmdFlag
{
	/**
	*	This is a HUD (client library) command.
	*/
	IS_HUD_CMD = 1 << 0,

	/**
	*	This is a game (server library) command.
	*/
	IS_GAME_CMD = 1 << 1,

	/**
	*	This is a wrapper (GameUI library) command.
	*/
	IS_WRAPPER_CMD = 1 << 2
};
}

/**
*	Represents a command function that can be executed.
*/
struct cmd_function_t
{
	/**
	*	Next function in the list.
	*/
	cmd_function_t *next;

	/**
	*	Name of the command.
	*/
	const char *name;

	/**
	*	Function to execute.
	*	Parameters are accessible through:
	*	server:
	*		@see enginefuncs_t::pfnCmd_Argc
	*		@see enginefuncs_t::pfnCmd_Argv
	*		@see enginefuncs_t::pfnCmd_Args
	*	client:
	*		@see cl_enginefunc_t::Cmd_Argc
	*		@see cl_enginefunc_t::Cmd_Argv
	*/
	CmdFunction function;

	/**
	*	Command flags.
	*/
	int flags;
};

#endif //ENGINE_CMD_FUNCTION_T_H
