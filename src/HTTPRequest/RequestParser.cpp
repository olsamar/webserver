#include "RequestParser.hpp"
#include <algorithm> // for std::distance
#include <utility> // for std::make_pair
#include <cstdlib> // for atoi
#include <cctype> // for ::toupper
#include  <climits> // for INT_MAX

#include "HTTPRequestMethods.hpp"
#include "../HTTP/Exceptions/RequestException.hpp"
#include "../Utility/Utility.hpp"
#include "../Constants.hpp"


namespace HTTPRequest {

    RequestParser::Dispatch RequestParser::_dispatch_table[] = {
        {REQUEST_LINE, &RequestParser::_parse_request_line},
        {HEADER, &RequestParser::_parse_header},
        {PAYLOAD, &RequestParser::_parse_payload},
        {CHUNKED_PAYLOAD, &RequestParser::_decode_chunked},
        {MULTIPART_PAYLOAD, &RequestParser::_parse_multipart_payload},
        {TRAILER, &RequestParser::_parse_trailer_header_fields},
        {FINISHED, NULL}
    };

    RequestParser::RequestParser(HTTPRequest::RequestMessage* http_request, HTTPResponse::ResponseMessage* http_response)
        : _current_parsing_state(REQUEST_LINE)
        , _payload_bytes_left_to_parse(0)
        , _boundary("")
        , _http_request_message(http_request)
        , _http_response_message(http_response){}

    RequestParser::~RequestParser(){}

    void RequestParser::parse_HTTP_request(char* buffer, size_t bytes_read) {
        size_t bytes_accumulated = 0;
        while (bytes_accumulated != bytes_read) {
            bool can_be_parsed = false;
            std::string line;
            if (_current_parsing_state == CHUNKED_PAYLOAD && _chunk_size > 0) {
                line = _request_reader.read_chunk(_chunk_size, buffer, bytes_read, &bytes_accumulated, &can_be_parsed);
            }
            else if (_current_parsing_state == PAYLOAD) {
                line = _request_reader.read_payload(buffer, bytes_read, &bytes_accumulated, &can_be_parsed);
                size_t line_size = line.size();
                _payload_bytes_left_to_parse -= line_size;
                can_be_parsed = true;
                if (_payload_bytes_left_to_parse < 0) {
                    _throw_request_exception(HTTPResponse::BadRequest);
                }
            }
            else {
                line = _request_reader.read_line(buffer, bytes_read, &bytes_accumulated, &can_be_parsed);
                if (_current_parsing_state == MULTIPART_PAYLOAD) {
                    _payload_bytes_left_to_parse -= line.size();
                }
                if (can_be_parsed) {
                    line.erase(line.size() - 2, 2);
                }
            }
            if (can_be_parsed == true) {
                _handle_request_message_part(line);
                if (_current_parsing_state == PAYLOAD && _boundary == "" && _http_request_message->get_message_body().size() == 0) { // validating headers only once, right after we've finished parsing them
                    _validate_headers();
                }
            }
            else {
                return;
            }
        }
        if (_payload_bytes_left_to_parse == 0 && _current_parsing_state == PAYLOAD) {
            _current_parsing_state = FINISHED;
        }
    }

    void RequestParser::_handle_request_message_part(std::string& line) {
        Dispatch *message = _dispatch_table;
        for (size_t i = 0; message[i].parsing_state != FINISHED; ++i) {
            if (message[i].parsing_state == _current_parsing_state) {
                (this->*(message[i].ptr))(line);
                return;
            }
        }
    }

    bool RequestParser::is_parsing_finished() {
        return _current_parsing_state == FINISHED;
    }

    void RequestParser::_throw_request_exception(HTTPResponse::StatusCode error_status) {
        _current_parsing_state = FINISHED;
        throw Exception::RequestException(error_status);
    }

    void RequestParser::_parse_request_line(std::string& line) {
        if (line.empty()) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        std::vector<std::string> segments = Utility::_split_line(line, ' ');
        if (segments.size() != 3) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        if (_is_method_supported(segments[0])) {
            _http_request_message->set_method(segments[0]);
        }
        _http_request_message->set_request_uri(segments[1]);

        URIParser uri_parser(segments[1]);
        URIData uri_data;
        try{
            uri_parser.parse(uri_data);
        }
        catch(const Exception::RequestException& e){
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        _http_request_message->set_uri(uri_data);

        _http_request_message->set_HTTP_version(segments[2]);
        _current_parsing_state = HEADER;
    }

    bool RequestParser::_is_method_supported(const std::string& method) {
        if (method.size() > _longest_method_size()) {
            _throw_request_exception(HTTPResponse::NotImplemented);
        }
        if (HTTPRequestMethods.find(method) == HTTPRequestMethods.end()) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        return true;
    }

    size_t RequestParser::_longest_method_size() {
        size_t longest_size = 0;
        for (std::set<std::string>::iterator it = HTTPRequestMethods.begin(); it != HTTPRequestMethods.end(); it++) {
            size_t string_size = (*it).size();
            if ( string_size > longest_size) {
                longest_size = string_size;
            }
        }
        return longest_size;
    }

    void RequestParser::_parse_header(std::string& line) {
        if (line == "\r\n" || line == "") {
            _current_parsing_state = PAYLOAD;
            return;
        }
        std::vector<std::string> segments = Utility::_split_line(line, ':');
        if (segments.size() < 2) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        if (Utility::contains_whitespace(segments[0])) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        std::string uppercased_header_name = _convert_header_name_touppercase(segments[0]);
        std::pair<std::string, std::string> header_field(uppercased_header_name, Utility::_trim(segments[1]));
        _http_request_message->set_header_field(header_field);
    }

    std::string RequestParser::_convert_header_name_touppercase(std::string& header_name) {
        std::transform(header_name.begin(), header_name.end(),header_name.begin(), ::toupper);
        std::replace_if (header_name.begin(), header_name.end(), Utility::is_hyphen, '_');
        return header_name;
    }

    void RequestParser::_validate_headers() {
        _define_payload_length_type();
        _check_multipart_content_type();
    }
        
    void RequestParser::_define_payload_length_type() {
        std::map<std::string, std::string> headers_map = _http_request_message->get_headers();
        std::map<std::string, std::string>::iterator transfer_encoding_iter = headers_map.find("TRANSFER_ENCODING");
        if (_http_request_message->has_header_field("CONTENT_LENGTH")) {
            if (!(_http_request_message->has_header_field("TRANSFER_ENCODING"))) {
                _payload_length_type = CONTENT_LENGTH;
                _set_content_length();
            }
            else {
                _parse_transfer_encoding(transfer_encoding_iter->second);
                if (_payload_length_type == CHUNKED) {
                    _current_parsing_state = CHUNKED_PAYLOAD;
                    _chunk_size = -1;
                    _decoded_body_length = 0;
                    _decoded_body = "";
                }
            }
        }
        else if (_http_request_message->has_header_field("TRANSFER_ENCODING")) { // if headers contain Transfer-Encoding without Content-length
            _parse_transfer_encoding(transfer_encoding_iter->second);
            if (_payload_length_type != CHUNKED) {
                _throw_request_exception(HTTPResponse::LengthRequired);
            }
        }
        else {
            if (_http_request_message->get_method() == "POST") {
                _throw_request_exception(HTTPResponse::LengthRequired);
            }
        }
    }

    void RequestParser::_set_content_length() {
        std::string content_length_value = _http_request_message->get_headers().find("CONTENT_LENGTH")->second;
        if (content_length_value.find_first_of(',', 0) != std::string::npos) {
            std::vector<std::string> values = Utility::_split_line(content_length_value, ',');
            std::string first_value = Utility::_trim(values[0]);
            for (size_t i = 1; i < values.size(); ++i) {
                if (first_value != Utility::_trim(values[i])) { // checking that the all the values are the same
                    _throw_request_exception(HTTPResponse::BadRequest);
                }
            }
            content_length_value = first_value;
            _http_request_message->update_header_field("CONTENT_LENGTH", content_length_value);
        }
        for (std::string::iterator it = content_length_value.begin(); it != content_length_value.end(); ++it) {
            if (!isdigit(*it)) {
                _throw_request_exception(HTTPResponse::BadRequest); // should throw for negative values as well
            }
        }
        _payload_bytes_left_to_parse = std::atoi(content_length_value.c_str());
        if (_payload_bytes_left_to_parse > Constants::PAYLOAD_MAX_LENGTH) {
            _throw_request_exception(HTTPResponse::ContentTooLarge);
        }
    }

    void RequestParser::_check_multipart_content_type() {
        if (!(_http_request_message->has_header_field("CONTENT_TYPE"))) {
            return;
        }
        std::string content_type_value = _http_request_message->get_header_value("CONTENT_TYPE");
        if (Utility::is_found(content_type_value, "multipart")) {
            _set_multipart_boundary(content_type_value);
            _current_parsing_state = MULTIPART_PAYLOAD;
        }
    }
    
    void RequestParser::_set_multipart_boundary(std::string& content_type_value) {
        std::vector<std::string> parts = Utility::_split_line(content_type_value, ' ');
        std::vector<std::string>::iterator iter = parts.begin();
        for (; iter != parts.end(); iter++) {
            std::string boundary_marker = "boundary=";
            size_t boundary_marker_size = boundary_marker.size();
            if (Utility::is_found(*iter, boundary_marker)) {
                size_t boundary_parameter_value_size = (*iter).size() - boundary_marker_size;
                if (boundary_parameter_value_size > 70) { // boundary parameter cannot exceed 70 characters
                    _throw_request_exception(HTTPResponse::BadRequest);
                }
                _boundary = "--" + (*iter).substr(boundary_marker_size, boundary_parameter_value_size);
            }
        }
    }

    // need to find the position of the 'chunked' in transfer-Encodeing as rfc demands to throw the 400 Error if 'chunked'is not the final encoding
    void RequestParser::_parse_transfer_encoding(std::string& coding_names_list) {
        std::vector<std::string> encodings = Utility::_split_line(coding_names_list, ','); //splitting the header value as there might be multiple encodings
        ssize_t encodings_num = encodings.size();
        ssize_t chunked_position = _find_chunked_encoding_position(encodings, encodings_num);
        if (chunked_position == - 1) {
            std::cout << "Chunked not found\n";
            _payload_length_type = NOT_FOUND;
        }
        else if (chunked_position == encodings_num - 1) {
            _payload_length_type = CHUNKED;
        }
        else {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
    }

    
    ssize_t RequestParser::_find_chunked_encoding_position(std::vector<std::string>& encodings, size_t encodings_num) {
        for (size_t i = 0; i < encodings_num; ++i) {
            if (Utility::_trim(encodings[i]) == "chunked") {
                return i;
            }
        }
        return -1;
    }

    void RequestParser::_parse_multipart_payload(std::string& line) {
        if (line == _boundary) {
            return;
        }
        if (line == _boundary + "--") {
            _current_parsing_state = FINISHED;
            return;
        }
        _parse_header(line);
    }

    void RequestParser::_parse_payload(std::string& line) {
        if (line != _boundary + "--\r\n") {
            _http_request_message->set_payload(line);
        }
        if (_payload_bytes_left_to_parse == 0 &&  _current_parsing_state != TRAILER) {
            _current_parsing_state = FINISHED;
        }
    }


    void RequestParser::_decode_chunked(std::string& line) {
        if (_chunk_size >= 0 ) {
            _decoded_body.append(line); // in this case we're dealing with the payload data
            size_t line_size = line.size();
            _chunk_size -= line_size;
            _decoded_body_length += line_size;
            if (_chunk_size == 0) {
                _chunk_size = -1; // after handling the data we have to make sure we set the new chunk_size in the next iteration
            }
        }
        else { // the chunk_size is reset to -1 before the chunk_length will be defined
            _set_chunk_size(line);
            if (_is_last_chunk()) {
                if (_http_request_message->has_header_field("TRAILER")) {
                    _check_disallowed_trailer_header_fields();
                    _current_parsing_state = TRAILER;
                }
                _assign_decoded_body_length_to_content_length();
                _parse_payload(_decoded_body);
                _remove_chunked_from_transfer_encoding(); // this is what rfc demands
            }
        }
    }

    bool RequestParser::_is_last_chunk() {
        return _chunk_size == 0;
    }

    void RequestParser::_assign_decoded_body_length_to_content_length() {
        std::string content_length_header_name = "CONTENT_LENGTH";
        const std::string content_length_value = Utility::to_string(_decoded_body_length);
        if (_http_request_message->has_header_field(content_length_header_name)) {
            _http_request_message->update_header_field(content_length_header_name, content_length_value);
        }
        else {
            std::pair<std::string, std::string> header_field(content_length_header_name, content_length_value);
            _http_request_message->set_header_field(header_field);
        }
    }

    void RequestParser::_set_chunk_size(std::string& line) {
        std::string extracted_number = Utility::get_number_in_string(line);
        if (extracted_number == "") {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        else {
            std::stringstream ss; // converting hex to the ssize_t
            ss << std::hex << extracted_number;
            ss >> _chunk_size;
            if (_chunk_size > Constants::PAYLOAD_MAX_LENGTH) {
                _throw_request_exception(HTTPResponse::BadRequest);
            }
        }
    }

    void RequestParser::_remove_chunked_from_transfer_encoding() {
        const std::map<std::string, std::string>& headers = _http_request_message->get_headers();
        std::string name = "TRANSFER_ENCODING";
        std::map<std::string, std::string>::const_iterator transfer_encoding_iter = headers.find(name);
        std::string value = transfer_encoding_iter->second;
        // chunked must always be the last parameter of transfer encoding. We're erasing the last part of the string which must be the length of "chunked"
        const std::string part_to_erase = "chunked";
        value.erase(value.end() - part_to_erase.size(), value.end());
        _http_request_message->update_header_field(name, value);
    }

// this is the list of the header fields that are not allowed to be placed in Trailer headers
    void RequestParser::_check_disallowed_trailer_header_fields() {
       const std::string& trailer_value = _http_request_message->get_header_value("TRAILER");
       if (Utility::is_found(trailer_value, "Transfer-Encoding")
            || Utility::is_found(trailer_value, "Content-Length")
            || Utility::is_found(trailer_value, "Host")
            || Utility::is_found(trailer_value, "Cache-Control")
            || Utility::is_found(trailer_value, "Max-Forwards")
            || Utility::is_found(trailer_value, "Max-Authorization")
            || Utility::is_found(trailer_value, "Set-Cookie")
            || Utility::is_found(trailer_value, "Content-Encoding")
            || Utility::is_found(trailer_value, "Content-Type")
            || Utility::is_found(trailer_value, "Content-Range")
            || Utility::is_found(trailer_value, "Trailer")) {
                _throw_request_exception(HTTPResponse::BadRequest);
        }
    }

    void RequestParser::_parse_trailer_header_fields(std::string &line) {
        if (line == "\r\n" || line == "") {
            _current_parsing_state = FINISHED;
            return;
        }
        std::vector<std::string> segments = Utility::_split_line_in_two(line, ':');
        if (Utility::contains_whitespace(segments[0])) {
            _throw_request_exception(HTTPResponse::BadRequest);
        }
        const std::string& trailer_value = _http_request_message->get_header_value("TRAILER");
        if (Utility::is_found(trailer_value, segments[0])) {
            std::string uppercased_header_name = _convert_header_name_touppercase(segments[0]);
            std::pair<std::string, std::string> header_field(uppercased_header_name, Utility::_trim(segments[1]));
            _http_request_message->set_header_field(header_field);
        }
    }
}
