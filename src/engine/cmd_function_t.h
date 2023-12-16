#ifndef ENGINE_CMD_FUNCTION_T_H
#define ENGINE_CMD_FUNCTION_T_H

using CmdFunction = void (*)();

constexpr int FCMD_HUD_COMMAND = 1 << 0;        //!< This is a HUD (client library) command.
constexpr int FCMD_GAME_COMMAND = 1 << 1;       //!< This is a game (server library) command.
constexpr int FCMD_WRAPPER_COMMAND = 1 << 2;    //!< This is a wrapper (GameUI library) command.
constexpr int FCMD_FILTERED_COMMAND = 1 << 3;   //!< Can only be executed by the local client (not a remote server or a config file).
constexpr int FCMD_RESTRICTED_COMMAND = 1 << 4; //!< Can only be executed by the local client if cl_filterstuffcmd is true.

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
