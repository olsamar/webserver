#pragma once

#include <string>
#include <sys/types.h>// for ssize_t

namespace HTTPRequest {
	class RequestReader
	{
	private:
		std::string _accumulator;
		size_t _length_counter;

		bool _is_end_of_line(char character);
		bool _is_end_of_chunk(char *buffer, size_t bytes_accumulated);

	public:
		RequestReader();
		~RequestReader();

		std::string read_line(char* buffer, size_t bytes_read, size_t* bytes_accumulated, bool* can_be_parsed);
		std::string read_chunk(ssize_t chunk_size, char *buffer, size_t bytes_read, size_t *bytes_accumulated, bool *can_be_parsed);
		std::string read_payload(char *buffer, size_t bytes_read, size_t *bytes_accumulated, bool *can_be_parsed);
	};
}
