#ifndef ConfigData_HPP
# define ConfigData_HPP

#include <vector>
#include <iostream>
#include "ServerBlock.hpp"

// namespace Config {
	class ConfigData
	{
	private:
		/* data */
		std::vector<ServerBlock> _servers;
	public:
		ConfigData(/* args */);
		~ConfigData();
		ConfigData(const ConfigData& other);
		const ConfigData &operator=(const ConfigData& other);

		std::vector<ServerBlock> &get_servers(void);
		void	print_servers_info(void);
		void	print_listen_info(ServerBlock &server);
		void	print_server_name(ServerBlock &server);
		void	print_roots(ServerBlock &server);
		void	print_returns(ServerBlock &server);
		void	print_error_pages(ServerBlock &server);
		void	print_multiple_locations_info(ServerBlock &server);
		void	print_limit_except(LocationBlock &location);
	};
// }
#endif