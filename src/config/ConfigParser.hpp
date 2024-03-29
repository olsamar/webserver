#pragma once

#include <string>
#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>

#include "ServerBlock.hpp"
#include "ConfigData.hpp"

namespace Config
{

	class ConfigParser
	{
	private:
		/* data */
		ConfigData *config_data;
		std::vector<std::string> server_tokens;
		enum Directives
		{
			LISTEN,
			SERVER_NAME,
			BODY_SIZE,
			ERROR_PAGE,
			RETURN,
			ROOT,
			LIMIT_EXCEPT,
			AUTOINDEX,
			ROUTE,
			EXT,
			INDEX_PAGE,
			UPLOAD
		};

		/* methods */
		void parse_server_block(std::string& server_token, ServerBlock &server);
		bool find_location(std::string line);
		int find_directive(std::string& line);
		void parse_location_block(std::string& line, std::istringstream &stream, ServerBlock &server);
		void parse_server_directive(std::string& line, ServerBlock &server, int e_num);
		void parse_location_directive(std::string& line, LocationBlock &location, int e_num);
		void parse_limit_except(std::string& line, LocationBlock &location, std::istringstream &stream);

	public:
		ConfigParser(ConfigData *config_data, std::vector<std::string> server_tokens);
		~ConfigParser();

		void parse(void);
	};
} // namespace Config
