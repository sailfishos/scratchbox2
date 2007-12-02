function sbox_argvenvp(binaryname, argv, envp)
	local new_argv = {}
	local new_envp = {}

	new_argv = argv
	new_envp = envp
	return 0, #new_argv, new_argv, #new_envp, new_envp
end
