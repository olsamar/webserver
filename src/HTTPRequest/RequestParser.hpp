#ifndef REQUESTPARSER_HPP
#define REQUESTPARSER_HPP

#include <string>
#include <set>
#include <vector>
#include <iostream> // TODO: remove
#include <sys/types.h>// for ssize_t

#include "RequestReader.hpp"
#include "RequestMessage.hpp"
#include "../HTTPResponse/ResponseMessage.hpp"
#include "../HTTPResponse/StatusCodes.hpp"
#include "URI/URIParser.hpp"

namespace HTTPRequest {

    class RequestParser {

    private:
        enum State
        {
            REQUEST_LINE,
            HEADER,
            MESSAGE_BODY,
            FINISHED
        };

        enum MessageBodyLength
        {
            CHUNCKED,
            CONTENT_LENGTH,
            NOT_FOUND
        };

        RequestReader _request_reader;
        State _current_parsing_state;
        MessageBodyLength _message_body_length;

        void _handle_request_message_part(std::string& line);
        void _parse_request_line(std::string& line);
        void _parse_header(std::string& line);
        void _determine_message_body_length();
        int _set_content_length();
        ssize_t _find_chuncked_encoding_position(std::vector<std::string> &encodings, size_t encodings_num);
        void _delete_obolete_content_length_header();
        void _parse_message_body(std::string &line);

        bool _is_method_supported(const std::string &method);
        size_t _longest_method_size();
        void _throw_request_exception(HTTPResponse::StatusCode error_status);

        std::vector<std::string> _split_line(const std::string& line, const char delimiter);
        std::string _trim(const std::string& s);
        bool contains_whitespace(std::string &str);
        
        struct Dispatch {
            State parsing_state;
            void (RequestParser::*ptr)(std::string& line);
        };

        static Dispatch _dispatch_table[];


    public:
        HTTPRequest::RequestMessage* _http_request_message;
        HTTPResponse::ResponseMessage* _http_response_message;

        RequestParser(HTTPRequest::RequestMessage* http_request, HTTPResponse::ResponseMessage* http_response);
        RequestParser(const RequestParser& other);
        ~RequestParser();

        void parse_HTTP_request(char* buffer, size_t bytes_read);
        bool is_parsing_finished();
    };
}

#endif
