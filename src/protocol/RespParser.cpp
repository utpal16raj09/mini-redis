#include "protocol/RespParser.hpp"
#include <sstream>

using namespace std;

namespace miniredis {

// Factory method creating a SimpleString RESP object (+OK\r\n)
RespValue RespValue::makeSimpleString(const string& str) {
    return RespValue{RespType::SimpleString, str};
}

// Factory method creating an Error RESP object (-ERR ...\r\n)
RespValue RespValue::makeError(const string& err) {
    return RespValue{RespType::Error, err};
}

// Factory method creating an Integer RESP object (:100\r\n)
RespValue RespValue::makeInteger(int64_t val) {
    return RespValue{RespType::Integer, val};
}

// Factory method creating a BulkString RESP object ($6\r\nfoobar\r\n)
RespValue RespValue::makeBulkString(const string& str) {
    return RespValue{RespType::BulkString, str};
}

// Factory method creating a Null RESP object ($-1\r\n)
RespValue RespValue::makeNull() {
    return RespValue{RespType::Null, string("")};
}

// Factory method creating an Array RESP object (*2\r\n...)
RespValue RespValue::makeArray(const vector<RespValue>& arr) {
    return RespValue{RespType::Array, arr};
}

// Converts a RespValue struct into its valid network byte representation per RESP specifications.
string RespValue::serialize() const {
    string result;
    switch (type) {
        case RespType::SimpleString:
            result = "+" + get<string>(value) + "\r\n";
            break;
        case RespType::Error:
            result = "-" + get<string>(value) + "\r\n";
            break;
        case RespType::Integer:
            result = ":" + to_string(get<int64_t>(value)) + "\r\n";
            break;
        case RespType::BulkString: {
            const auto& str = get<string>(value);
            result = "$" + to_string(str.size()) + "\r\n" + str + "\r\n";
            break;
        }
        case RespType::Null:
            result = "$-1\r\n";
            break;
        case RespType::Array: {
            const auto& arr = get<vector<RespValue>>(value);
            result = "*" + to_string(arr.size()) + "\r\n";
            for (const auto& elem : arr) {
                result += elem.serialize();
            }
            break;
        }
    }
    return result;
}

// Parses standard RESP formatted streams from an input buffer string reference.
optional<RespValue> RespParser::parse(string& input) {
    if (input.empty()) return nullopt;

    // Search for line terminator CRLF (\r\n)
    size_t crlf = input.find("\r\n");
    if (crlf == string::npos) return nullopt; // Line not yet completely received

    char prefix = input[0];
    string line = input.substr(1, crlf - 1);

    // 1. Simple String (+STRING\r\n)
    if (prefix == '+') {
        input.erase(0, crlf + 2);
        return RespValue::makeSimpleString(line);
    } 
    // 2. Error (-ERROR\r\n)
    else if (prefix == '-') {
        input.erase(0, crlf + 2);
        return RespValue::makeError(line);
    } 
    // 3. Integer (:12345\r\n)
    else if (prefix == ':') {
        input.erase(0, crlf + 2);
        return RespValue::makeInteger(stoll(line));
    } 
    // 4. Bulk String ($LEN\r\nVALUE\r\n)
    else if (prefix == '$') {
        int len = stoi(line);
        if (len == -1) {
            input.erase(0, crlf + 2);
            return RespValue::makeNull();
        }
        // Verify buffer has enough data for full bulk string payload + final CRLF
        if (input.size() < crlf + 2 + len + 2) {
            return nullopt; // Need more bytes from network
        }
        string bulk_str = input.substr(crlf + 2, len);
        input.erase(0, crlf + 2 + len + 2);
        return RespValue::makeBulkString(bulk_str);
    } 
    // 5. Array (*COUNT\r\nELEMENTS...)
    else if (prefix == '*') {
        int count = stoi(line);
        if (count == -1) {
            input.erase(0, crlf + 2);
            return RespValue::makeNull();
        }
        
        string temp_input = input.substr(crlf + 2);
        vector<RespValue> elements;
        elements.reserve(count);

        // Recursively parse array elements
        for (int i = 0; i < count; ++i) {
            auto elem = parse(temp_input);
            if (!elem.has_value()) {
                return nullopt; // Waiting for complete array frame
            }
            elements.push_back(elem.value());
        }

        input = temp_input;
        return RespValue::makeArray(elements);
    }

    return nullopt;
}

} // namespace miniredis
