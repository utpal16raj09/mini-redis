#ifndef MINI_REDIS_RESP_PARSER_HPP
#define MINI_REDIS_RESP_PARSER_HPP

#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <cstdint>

using namespace std;

namespace miniredis {

// Enum defining the standard Redis protocol message types.
enum class RespType {
    SimpleString, // '+' (e.g. "+OK\r\n")
    Error,        // '-' (e.g. "-ERR unknown command\r\n")
    Integer,      // ':' (e.g. ":100\r\n")
    BulkString,   // '$' (e.g. "$5\r\nhello\r\n")
    Array,        // '*' (e.g. "*2\r\n$3\r\nGET\r\n$3\r\nkey\r\n")
    Null          // '$-1\r\n' (Null reply)
};

// Represents a parsed Redis message or command.
struct RespValue {
    RespType type{RespType::Null};
    variant<string, int64_t, vector<RespValue>> value;

    // Serializes this message into standard RESP string wire format.
    string serialize() const;

    // Helper constructors for generating standard RESP responses
    static RespValue makeSimpleString(const string& str);
    static RespValue makeError(const string& err);
    static RespValue makeInteger(int64_t val);
    static RespValue makeBulkString(const string& str);
    static RespValue makeNull();
    static RespValue makeArray(const vector<RespValue>& arr);
};

// Stream parser for converting raw network string buffers into parsed RespValue commands.
class RespParser {
public:
    // Parses a complete RESP command from input_buffer.
    // Removes parsed bytes from input_buffer upon success.
    // Returns nullopt if the buffer doesn't have a complete frame yet.
    static optional<RespValue> parse(string& input_buffer);
};

} // namespace miniredis

#endif // MINI_REDIS_RESP_PARSER_HPP
