#ifndef WEBSERVER_HPP
#define WEBSERVER_HPP

#include "HTTP/Server.hpp"

class Webserver
{
private:
	/* data */
public:
	Webserver(/* args */);
	~Webserver();
	void start();
};

#endif