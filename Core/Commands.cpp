#include "Core.h"


#define MAX_CMDLINE		1024
#define MAX_ARGS		256


/*-----------------------------------------------------------------------------
	Command execution
-----------------------------------------------------------------------------*/


static char lineBuffer[MAX_CMDLINE];
static const char *c_argv[MAX_ARGS];
static int  c_argc;


static void GetArgs(const char *str, bool expandVars)
{
	guard(GetArgs);
	// preparing
	c_argc = 0;
	char *d = lineBuffer;
	for (int i = 0; i < MAX_ARGS; i++) c_argv[i] = "";
	// skip leading spaces
	while (*str == ' ') str++;
	// parsing line
	while (char c = *str++)
	{
		if (c_argc == MAX_ARGS)
		{
			appNotify("GetArgs: MAX_ARGS hit");
			return;
		}
		c_argv[c_argc++] = d;
		//?? need to check lineBuffer overflow
		//?? need to treat '\t' as ' '
		if (c == '\"')
		{
			// quoted argument
			while (c = *str++)
			{
				if (c == '\"')
				{
					if (*str != '\"') break;		// allow quote doubling inside strings
					str++;
				}
//				if (c == '$' && expandVars) expandVars	//?? expand vars "${var_name}" (bash-style)
				*d++ = c;
			}
			if (!c)
			{
				//!! Warn: unmatched quote
				*d = 0;
				return;
			}
		}
//		else if (c == '$' && expandVars)
//		{
			//!! expand cvar value: name till space or 0
//		}
		else
		{
			// unquoted argument
			*d++ = c;
			while (c = *str)
			{
				str++;			// NOTE: when c==0, we will not get here
				if (c == ' ') break;
				*d++ = c;
				// NOTE: if '"' will appear without space (i.e. abc"def), this will not split arg
			}
		}
		*d++ = 0;
		// skip spaces
		while (*str == ' ') str++;
	}
	*d = 0;
	unguard;
}


bool ExecuteCommand(const char *str, const CSimpleCommand *CmdList, int numCommands)
{
	guard(ExecuteSimpleCommand);
	GetArgs(str, false);
	for (int i = 0; i < numCommands; i++, CmdList++)
		if (!stricmp(CmdList->name, c_argv[0]))
		{
			if (!CmdList->func) return true;		// NULL function
			guard(cmd)
			CmdList->func(c_argc, c_argv);
			return true;
			unguardf(("%s", CmdList->name))
		}
	return false;
	unguard;
}
