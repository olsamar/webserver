#pragma once

#include <iostream>
#include <string>
#include <vector>

namespace Config
{

	class AConfigBlock
	{
	protected:
		/* data */
		std::string _root;
		std::vector<std::string> _return;
		std::vector<std::string> _error_page;
		int _client_max_body_size;
		bool _is_size_default;

		/* check methods */
		void _check_return_syntax(std::string str);
		void _check_return_code(std::string code);
		void _check_client_max_body_size_syntax(std::string& str);
		void _check_size(std::string& size);

	public:
		AConfigBlock();
		AConfigBlock(const AConfigBlock &other);
		const AConfigBlock &operator=(const AConfigBlock &other);
		virtual ~AConfigBlock();

		/* getters & setters */
		void set_return_value(std::string str);
		void set_root_value(std::string str);
		void set_error_page_value(std::string str);
		void set_client_max_body_size(std::string str);
		int get_client_max_body_size(void) const;
		const std::string& get_root(void) const;
		const std::vector<std::string>& get_return(void) const;
		const std::vector<std::string>& get_error_page(void) const;
	};
} // namespace Config
