/*
 * Copyright (c) 2004, 2005 Nokia
 * Author: Timo Savola <tsavola@movial.fi>
 *
 * Licensed under GPL, see COPYING for details.
 */

#include "sb_config.hh"
#include <fstream>
#include <cstdlib>

using namespace std;

#define CONFIG_PATH   "/targets/links/scratchbox.config"
#define MAX_LINE_SIZE 4096

namespace sb {

	error::error(const string &a) :
		message(a)
	{
	}

	error::error(const string &a, const string &b) :
		message(a)
	{
		message += b;
	}

	error::error(const string &a, const string &b, const string &c) :
		message(a)
	{
		message += b;
		message += c;
	}

	error::~error()
		throw ()
	{
	}

	const char *error::what() const
		throw ()
	{
		return message.c_str();
	}

	char *strip(char *const str)
	{
		size_t start, end;
		for (start = 0; str[start] && isspace(str[start]); ++start);
		for (end = strlen(str) - 1; end > start && isspace(str[end]); --end);
		str[end + 1] = '\0';
		return &str[start];
	}

	char *strip_quotes(char *const str)
	{
		const size_t start = (str[0] == '"') ? 1 : 0;

		const size_t end = strlen(str) - 1;
		if (str[end] == '"')
			str[end] = '\0';

		return &str[start];
	}

	list<string> split(const string &str)
	{
		list<string> ls;

		for (string::size_type index = 0; true; ++index) {
			string::size_type start = index;
			index = str.find(':', start);

			ls.push_back(str.substr(start, index - start));

			if (index == string::npos)
				break;
		}

		return ls;
	}

	bool read_config(map<string,string> &config)
		throw (error)
	{
		const char *path = getenv("SBOX_SCRATCHBOX_CONFIG");
		if (!path)
			path = CONFIG_PATH;

		ifstream file(path);
		if (!file.is_open())
			return false;

		char buf[MAX_LINE_SIZE];
		while (!file.getline(buf, sizeof (buf)).eof()) {
			if (file.bad())
				throw error("unable to read ", path);

			char *const com = strchr(buf, '#');
			if (com)
				com[0] = '\0';

			char *const equ = strchr(buf, '=');
			if (equ) {
				equ[0] = '\0';
				config[strip(buf)] = strip_quotes(strip(&equ[1]));
			}
		}

		return true;
	}

	char **build_argv(const char *command, list<string> &args)
	{
		char **argv = new char *[1 + args.size() + 1];

		argv[0] = const_cast<char *> (command);

		for (list<string>::iterator i = args.begin(); i != args.end(); ++i)
			argv[1 + distance(args.begin(), i)] = const_cast<char *> (i->c_str());

		argv[1 + args.size()] = 0;

		return argv;
	}

}
