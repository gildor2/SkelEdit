struct CSimpleCommand
{
	const char	*name;
	void (*func)(int argc, const char **argv);	// NULL to ignore command
};

bool ExecuteCommand(const char *str, const CSimpleCommand *CmdList, int numCommands);
