#include "commands/Router.hpp"
#include "storage/AofEngine.hpp"
#include <algorithm>

using namespace std;

namespace miniredis {

Router::Router(Database& db, AofEngine* aof) : db_(db), aof_(aof) {}

bool Router::isWriteCommand(const string& cmd) {
    static const unordered_set<string> write_cmds = {
        "SET", "DEL", "EXPIRE", "INCR", "DECR", "FLUSHDB",
        "HSET", "HDEL", "LPUSH", "RPUSH", "LPOP", "RPOP", "SADD", "SREM"
    };
    return write_cmds.find(cmd) != write_cmds.end();
}

RespValue Router::dispatch(const RespValue& request, bool log_to_aof) {
    if (request.type != RespType::Array) {
        return RespValue::makeError("ERR unknown command format");
    }

    const auto& args = std::get<vector<RespValue>>(request.value);
    if (args.empty()) {
        return RespValue::makeError("ERR empty command");
    }

    string cmd = std::get<string>(args[0].value);
    transform(cmd.begin(), cmd.end(), cmd.begin(), ::toupper);

    RespValue response = RespValue::makeError("ERR unknown command '" + cmd + "'");

    // --- System & Server Commands ---
    if (cmd == "PING") {
        if (args.size() > 1) response = RespValue::makeBulkString(std::get<string>(args[1].value));
        else response = RespValue::makeSimpleString("PONG");
    } 
    else if (cmd == "INFO") {
        string info = "# Server\r\nredis_version:miniredis-1.0.0\r\nprocess_id:1\r\ntcp_port:6379\r\n";
        response = RespValue::makeBulkString(info);
    }
    else if (cmd == "FLUSHDB") {
        db_.flush();
        response = RespValue::makeSimpleString("OK");
    }
    // --- String Commands ---
    else if (cmd == "SET") {
        if (args.size() < 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'set' command");
        } else {
            string key = std::get<string>(args[1].value);
            string val = std::get<string>(args[2].value);

            optional<int64_t> ttl;
            if (args.size() >= 5) {
                string opt = std::get<string>(args[3].value);
                transform(opt.begin(), opt.end(), opt.begin(), ::toupper);
                if (opt == "PX") ttl = stoll(std::get<string>(args[4].value));
                else if (opt == "EX") ttl = stoll(std::get<string>(args[4].value)) * 1000;
            }

            db_.set(key, val, ttl);
            response = RespValue::makeSimpleString("OK");
        }
    } 
    else if (cmd == "GET") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'get' command");
        } else {
            string key = std::get<string>(args[1].value);
            auto val = db_.get(key);
            if (val.has_value()) response = RespValue::makeBulkString(val.value());
            else response = RespValue::makeNull();
        }
    } 
    else if (cmd == "INCR") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'incr' command");
        } else {
            auto res = db_.incr(std::get<string>(args[1].value), 1);
            if (res.has_value()) response = RespValue::makeInteger(res.value());
            else response = RespValue::makeError("ERR value is not an integer or out of range");
        }
    }
    else if (cmd == "DECR") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'decr' command");
        } else {
            auto res = db_.incr(std::get<string>(args[1].value), -1);
            if (res.has_value()) response = RespValue::makeInteger(res.value());
            else response = RespValue::makeError("ERR value is not an integer or out of range");
        }
    }
    // --- Generic Key & Expiration Commands ---
    else if (cmd == "DEL") {
        if (args.size() < 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'del' command");
        } else {
            int deleted = 0;
            for (size_t i = 1; i < args.size(); ++i) {
                if (db_.del(std::get<string>(args[i].value))) deleted++;
            }
            response = RespValue::makeInteger(deleted);
        }
    } 
    else if (cmd == "EXISTS") {
        if (args.size() < 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'exists' command");
        } else {
            int found = 0;
            for (size_t i = 1; i < args.size(); ++i) {
                if (db_.exists(std::get<string>(args[i].value))) found++;
            }
            response = RespValue::makeInteger(found);
        }
    } 
    else if (cmd == "EXPIRE") {
        if (args.size() != 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'expire' command");
        } else {
            string key = std::get<string>(args[1].value);
            int64_t seconds = stoll(std::get<string>(args[2].value));
            bool ok = db_.expire(key, seconds * 1000);
            response = RespValue::makeInteger(ok ? 1 : 0);
        }
    }
    else if (cmd == "TTL") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'ttl' command");
        } else {
            int64_t t = db_.ttl(std::get<string>(args[1].value));
            response = RespValue::makeInteger(t);
        }
    }
    else if (cmd == "KEYS") {
        string pattern = (args.size() > 1) ? std::get<string>(args[1].value) : "*";
        auto key_list = db_.keys(pattern);
        vector<RespValue> resp_arr;
        for (const auto& k : key_list) resp_arr.push_back(RespValue::makeBulkString(k));
        response = RespValue::makeArray(resp_arr);
    }
    // --- Hash Commands ---
    else if (cmd == "HSET") {
        if (args.size() != 4) {
            response = RespValue::makeError("ERR wrong number of arguments for 'hset' command");
        } else {
            bool is_new = db_.hset(std::get<string>(args[1].value), std::get<string>(args[2].value), std::get<string>(args[3].value));
            response = RespValue::makeInteger(is_new ? 1 : 0);
        }
    }
    else if (cmd == "HGET") {
        if (args.size() != 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'hget' command");
        } else {
            auto val = db_.hget(std::get<string>(args[1].value), std::get<string>(args[2].value));
            if (val.has_value()) response = RespValue::makeBulkString(val.value());
            else response = RespValue::makeNull();
        }
    }
    else if (cmd == "HDEL") {
        if (args.size() != 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'hdel' command");
        } else {
            bool ok = db_.hdel(std::get<string>(args[1].value), std::get<string>(args[2].value));
            response = RespValue::makeInteger(ok ? 1 : 0);
        }
    }
    else if (cmd == "HGETALL") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'hgetall' command");
        } else {
            auto res = db_.hgetall(std::get<string>(args[1].value));
            if (!res.has_value()) {
                response = RespValue::makeArray({});
            } else {
                vector<RespValue> arr;
                for (const auto& kv : res.value()) {
                    arr.push_back(RespValue::makeBulkString(kv.first));
                    arr.push_back(RespValue::makeBulkString(kv.second));
                }
                response = RespValue::makeArray(arr);
            }
        }
    }
    // --- List Commands ---
    else if (cmd == "LPUSH") {
        if (args.size() < 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'lpush' command");
        } else {
            vector<string> vals;
            for (size_t i = 2; i < args.size(); ++i) vals.push_back(std::get<string>(args[i].value));
            size_t len = db_.lpush(std::get<string>(args[1].value), vals);
            response = RespValue::makeInteger(len);
        }
    }
    else if (cmd == "RPUSH") {
        if (args.size() < 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'rpush' command");
        } else {
            vector<string> vals;
            for (size_t i = 2; i < args.size(); ++i) vals.push_back(std::get<string>(args[i].value));
            size_t len = db_.rpush(std::get<string>(args[1].value), vals);
            response = RespValue::makeInteger(len);
        }
    }
    else if (cmd == "LPOP") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'lpop' command");
        } else {
            auto val = db_.lpop(std::get<string>(args[1].value));
            if (val.has_value()) response = RespValue::makeBulkString(val.value());
            else response = RespValue::makeNull();
        }
    }
    else if (cmd == "RPOP") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'rpop' command");
        } else {
            auto val = db_.rpop(std::get<string>(args[1].value));
            if (val.has_value()) response = RespValue::makeBulkString(val.value());
            else response = RespValue::makeNull();
        }
    }
    else if (cmd == "LRANGE") {
        if (args.size() != 4) {
            response = RespValue::makeError("ERR wrong number of arguments for 'lrange' command");
        } else {
            int start = stoi(std::get<string>(args[2].value));
            int stop = stoi(std::get<string>(args[3].value));
            auto res = db_.lrange(std::get<string>(args[1].value), start, stop);
            if (!res.has_value()) {
                response = RespValue::makeArray({});
            } else {
                vector<RespValue> arr;
                for (const auto& elem : res.value()) arr.push_back(RespValue::makeBulkString(elem));
                response = RespValue::makeArray(arr);
            }
        }
    }
    // --- Set Commands ---
    else if (cmd == "SADD") {
        if (args.size() < 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'sadd' command");
        } else {
            vector<string> members;
            for (size_t i = 2; i < args.size(); ++i) members.push_back(std::get<string>(args[i].value));
            size_t added = db_.sadd(std::get<string>(args[1].value), members);
            response = RespValue::makeInteger(added);
        }
    }
    else if (cmd == "SREM") {
        if (args.size() < 3) {
            response = RespValue::makeError("ERR wrong number of arguments for 'srem' command");
        } else {
            vector<string> members;
            for (size_t i = 2; i < args.size(); ++i) members.push_back(std::get<string>(args[i].value));
            size_t removed = db_.srem(std::get<string>(args[1].value), members);
            response = RespValue::makeInteger(removed);
        }
    }
    else if (cmd == "SMEMBERS") {
        if (args.size() != 2) {
            response = RespValue::makeError("ERR wrong number of arguments for 'smembers' command");
        } else {
            auto members = db_.smembers(std::get<string>(args[1].value));
            if (!members.has_value()) {
                response = RespValue::makeArray({});
            } else {
                vector<RespValue> arr;
                for (const auto& m : members.value()) arr.push_back(RespValue::makeBulkString(m));
                response = RespValue::makeArray(arr);
            }
        }
    }

    // Write-ahead logging to AOF file if command mutates state
    if (log_to_aof && aof_ && isWriteCommand(cmd) && response.type != RespType::Error) {
        aof_->append(request);
    }

    return response;
}

} // namespace miniredis
