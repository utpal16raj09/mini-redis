#ifndef MINI_REDIS_RESP_PARSER_HPP
#define MINI_REDIS_RESP_PARSER_HPP

#include <string>
#include <vector>
#include <variant>
#include <optional>

using namespace std;

namespace miniredis {

// Enum defining supported data types in the RESP (Redis Serialization Protocol) format.
enum class RespType {
    SimpleString, // Starts with '+' e.g. "+OK\r\n"
    Error,        // Starts with '-' e.g. "-ERR unknown command\r\n"
    Integer,      // Starts with ':' e.g. ":1000\r\n"
    BulkString,   // Starts with '$' e.g. "$6\r\nfoobar\r\n"
    Array,        // Starts with '*' e.g. "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n"
    Null          // Represents null values e.g. "$-1\r\n"
};

// Represents a parsed RESP object which can hold different C++ types based on RespType.
struct RespValue {
    RespType type;
    variant<string, int64_t, vector<RespValue>> value;

    // Factory static methods for convenient construction of RESP values
    static RespValue makeSimpleString(const string& str);
    static RespValue makeError(const string& err);
    static RespValue makeInteger(int64_t val);
    static RespValue makeBulkString(const string& str);
    static RespValue makeNull();
    static RespValue makeArray(const vector<RespValue>& arr);

    // Serializes this RespValue object back into standard RESP protocol byte string.
    string serialize() const;
};

// RespParser handles stateful parsing of raw TCP byte streams into RespValue objects.
class RespParser {
public:
    // Attempts to parse one complete RESP message from the input buffer.
    // If a full command/message is present, returns the RespValue and removes the consumed bytes from input_buffer.
    // Returns nullopt if the input_buffer doesn't contain a complete frame yet (waiting for more TCP data).
    static optional<RespValue> parse(string& input_buffer);
};

} // namespace miniredis

#endif // MINI_REDIS_RESP_PARSER_HPP
