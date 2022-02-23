#pragma once

#include  <cstddef>

namespace HTTP {
	class RequestHandlerDelegate {

	public:
		virtual ~RequestHandlerDelegate() {}

		virtual size_t receive(char *buffer, size_t buffer_size) = 0;
		virtual void send(const void *buffer, size_t buffer_size) = 0;
		virtual void close() = 0;
	};
}
