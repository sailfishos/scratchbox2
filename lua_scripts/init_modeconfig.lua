-- Copyright (c) 2009,2012 Nokia Corporation.
-- Author: Lauri T. Aarnio
--
-- License: LGPL-2.1
--
-- Read in {all_needed_modes}/config.lua files and insert
-- rules and settings to the rule tree db.

for m_index,m_name in pairs(all_modes) do
	-- Read in config.lua
	enable_cross_gcc_toolchain = true
	exec_policy_selection = nil
	local config_file_name = session_dir .. "/share/scratchbox2/modes/"..m_name.."/config.lua"
	do_file(config_file_name)

	-- Modename => argvmods type table
	ruletree.catalog_set("use_gcc_argvmods", m_name,
		ruletree.new_boolean(enable_cross_gcc_toolchain))

	-- Exec policy selection table
	if exec_policy_selection ~= nil then
		local epsrule_list_index = ruletree.objectlist_create(#exec_policy_selection)
		for i = 1, #exec_policy_selection do
			local epsrule = exec_policy_selection[i]
			local ruletype = 0
			local selectorstr = 0
			-- Recycling:
			-- SB2_RULETREE_FSRULE_SELECTOR_PATH               101
			-- SB2_RULETREE_FSRULE_SELECTOR_PREFIX             102
			-- SB2_RULETREE_FSRULE_SELECTOR_DIR                103
			if epsrule.path then
				ruletype = 101
				selectorstr = epsrule.path
			elseif epsrule.prefix then
				ruletype = 102
				selectorstr = epsrule.prefix
			elseif epsrule.dir then
				ruletype = 103
				selectorstr = epsrule.dir
			else
				print("-- Skipping eps rule ["..i.."]")
			end
			if ruletype ~= 0 then
				local offs = ruletree.add_exec_policy_selection_rule_to_ruletree(
					ruletype, selectorstr, epsrule.exec_policy_name,
					0)
				ruletree.objectlist_set(epsrule_list_index, i-1, offs)
			end
		end
		ruletree.catalog_set("exec_policy_selection", m_name,
			epsrule_list_index)
	else
		error("No exec policy selection table in "..config_file_name)
	end
end

