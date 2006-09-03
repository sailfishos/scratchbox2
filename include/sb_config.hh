/*
 * Copyright (c) 2004, 2005 Nokia
 * Author: Timo Savola <tsavola@movial.fi>
 *
 * Licensed under GPL, see COPYING for details.
 */

#include <map>
#include <list>
#include <string>
#include <exception>

namespace sb {

	struct error :
		std::exception
	{
		error(const std::string &);
		error(const std::string &, const std::string &);
		error(const std::string &, const std::string &, const std::string &);
		~error() throw ();

		const char *what() const throw ();

	private:
		std::string message;
	};

	char *strip(char *);
	char *strip_quotes(char *);

	std::list<std::string> split(const std::string &);

	/**
	 * @return true if scratchbox.ocnfig exists (read: we are inside Scratchbox)
	 */
	bool read_config(std::map<std::string, std::string> &) throw (error);

	char **build_argv(const char *, std::list<std::string> &);

}
