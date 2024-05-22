#include "minimist.hpp"
#include <map>
#include <regex>
#include <string>
#include <vector>
#include <variant>
#include <optional>
#include <functional>
#include <nlohmann/json.hpp>

/*
 * Original program: https://github.com/minimistjs/minimist
 * The original code was licensed under the MIT License -> https://github.com/minimistjs/minimist/blob/main/LICENSE
 */

using namespace std;
using json = nlohmann::json;

json minimist(vector<string> args, optional<MinimistOpts> _opts) {
    MinimistOpts opts = _opts.value_or(MinimistOpts());

    struct {
        map<string, bool> bools;
        map<string, bool> strings;
        bool allBools = false;
        function<bool(string)> *unknownFn;
    } flags;

    flags.unknownFn = opts.unknown;

    if (opts.boolean.has_value()) {
        variant<bool, string, vector<string>> boolOpt = opts.boolean.value();
        if (get_if<bool>(&boolOpt) != nullptr) {
            flags.allBools = true;
        } else {
            string *_bool = get_if<string>(&boolOpt);
            vector<string> *_bools = get_if<vector<string>>(&boolOpt);
            if (_bools != nullptr) {
                vector<string> bools = *_bools;
                for (const string &key : bools) {
                    if (key.size() < 1) continue;
                    flags.bools[key] = true;
                }
            } else if (_bool != nullptr) {
                if (_bool->size() > 0) flags.bools[*_bool] = true;
            }
        }
    }

    map<string, vector<string>> aliases;

    auto isBooleanKey = [&flags, &aliases](string key) {
        if (flags.bools.find(key) != flags.bools.end()) return true;
        if (aliases.find(key) == aliases.end()) return false;
        bool found = false;
        for (string &x : aliases[key]) {
            if (flags.bools.find(x) != flags.bools.end()) {
                found = true;
                break;
            }
        }
        return found;
    };

    if (opts.alias.has_value()) {
        for (auto &&[first, second] : opts.alias.value()) {
            string *str = get_if<string>(&second);
            vector<string> *vec = get_if<vector<string>>(&second);
            if (str != nullptr) aliases[first] = vector<string>{*str};
            else if (vec != nullptr) aliases[first] = *vec;
            
            for (string &s : aliases[first]) {
                vector<string> r;
                vector<string> v = {first};
                v.insert(v.end(), aliases[first].begin(), aliases[first].end());
                for (string &st : v) {
                    if (s != st) r.push_back(st);
                }
                aliases[s] = r;
            }
        }
    }

    if (opts.string.has_value()) {
        vector<string> *_strs = get_if<vector<string>>(&opts.string.value());
        string *str = get_if<string>(&opts.string.value());
        vector<string> strs;
        if (_strs != nullptr) {
            strs.insert(strs.end(), _strs->begin(), _strs->end());
        } else {
            strs.push_back(*str);
        }
        for (string &s : strs) {
            flags.strings[s] = true;
            if (aliases.find(s) != aliases.end()) {
                for (string &st : aliases[s]) {
                    flags.strings[st] = true;
                }
            }
        }
    }

    map<string, variant<string, int, bool>> defaults = opts.def.value_or(map<string, variant<string, int, bool>>());
    json argv;
    argv["_"] = vector<string>();

    auto argDefined = [&flags, &aliases](string key, string arg) {
        regex re("^--[^=]+$");
        return (flags.allBools && regex_search(arg, re))
                || flags.strings.find(key) != flags.strings.end()
                || flags.bools.find(key) != flags.bools.end()
                || aliases.find(key) != aliases.end();
    };
    auto setKey = [&isBooleanKey](json &obj, vector<string> keys, variant<string, int, bool> value) {
        json *o = &obj;
        if (keys.size() < 1) return;
        for (size_t i = 0; i < keys.size() - 1; i++) {
            string key = keys[i];
            if (!o->contains(key)) (*o)[key] = json();
            o = &o->at(key);
        }
        string lastKey = keys[keys.size() - 1];
        string *sv = get_if<string>(&value);
        int *iv = get_if<int>(&value);
        bool *bv = get_if<bool>(&value);
        if (!o->contains(lastKey) || isBooleanKey(lastKey) || o->at(lastKey).is_boolean()) {
            if (sv != nullptr) (*o)[lastKey] = *sv;
            else if (iv != nullptr) (*o)[lastKey] = *iv;
            else if (bv != nullptr) (*o)[lastKey] = *bv;
        } else if (o->at(lastKey).is_array()) {
            if (sv != nullptr) (*o)[lastKey].push_back(*sv);
            else if (iv != nullptr) (*o)[lastKey].push_back(*iv);
            else if (bv != nullptr) (*o)[lastKey].push_back(*bv);
        } else {
            if (sv != nullptr) (*o)[lastKey] = {(*o)[lastKey], *sv};
            else if (iv != nullptr) (*o)[lastKey] = {(*o)[lastKey], *iv};
            else if (bv != nullptr) (*o)[lastKey] = {(*o)[lastKey], *bv};
        }
    };
    auto isNumber = [](string n) {
        try {
            stoi(n);
            return true;
        } catch (...) {
            return false;
        }
    };
    auto split = [](string s, string delim) {
        vector<string> v;
        size_t pos;
        string tok;
        while ((pos = s.find(delim)) != string::npos) {
            tok = s.substr(0, pos);
            v.push_back(tok);
            s.erase(0, pos + delim.size());
        }
        v.push_back(s);
        return v;
    };
    auto setArg = [&argDefined, &isNumber, &setKey, &split, &flags, &aliases, &argv](string key, variant<string, int, bool> val, optional<string> arg) {
        if (arg.has_value() && arg.value().size() > 0 && flags.unknownFn != nullptr && !argDefined(key, arg.value())) {
            if ((*flags.unknownFn)(arg.value()) == false) return;
        }
        string *n = get_if<string>(&val);
        if (flags.strings.find(key) == flags.strings.end() && n != nullptr && isNumber(*n)) {
            val = stoi(*n);
        }
        setKey(argv, split(string(key), "."), val);
        if (aliases.find(key) != aliases.end()) {
            vector<string> v = aliases[key];
            for (string &x : v) {
                setKey(argv, split(string(x), "."), val);
            }
        }
    };

    auto indexOf = [](vector<string> v, string s) {
        long i = 0;
        for (; i < (long)v.size(); i++) {
            if (v[i] == s) break;
        }
        if ((long)v.size() <= i || v[i] != s) return -1L;
        return i;
    };
    for (auto &&[first, second] : flags.bools) {
        setArg(first, false, nullopt);
    }
    for (auto &&[first, second] : defaults) {
        if (!isBooleanKey(first)) continue;
        setArg(first, defaults[first], nullopt);
    }
    vector<string> notFlags;
    int ddIdx = indexOf(args, "--");
    if (ddIdx >= 0) {
        notFlags = vector<string>(args.begin() + ddIdx, args.end());
        args = vector<string>(args.begin(), args.begin() + ddIdx);
    }

    for (size_t i = 0; i < args.size(); i++) {
        string arg = args[i];
        string key;
        string next;

        if (regex_search(arg, regex("^--.+="))) {
            smatch m;
            regex_match(arg, m, regex("^--([^=]+)=([\\s\\S]*)$"));
            key = m[1];
            string value = m[2];
            optional<bool> bv;
            if (isBooleanKey(key)) {
                bv = value != "false";
            }
            variant<string, int, bool> v;
            if (bv.has_value()) v = bv.value();
            else v = value;
            setArg(key, v, arg);
        } else if (regex_search(arg, regex("^--no-.+"))) {
            smatch m;
            regex_match(arg, m, regex("^--no-(.+)"));
            key = m[1];
            setArg(key, false, arg);
        } else if (regex_search(arg, regex("^--.+"))) {
            smatch m;
            regex_match(arg, m, regex("^--(.+)"));
            key = m[1];
            if (args.size() < i + 1) next = args[i + 1];
            else next = "";
            if (next.size() > 0 && regex_search(next, regex("^(-|--)[^-]")) && !isBooleanKey(key) && !flags.allBools) {
                setArg(key, next, arg);
                i++;
            } else if (regex_search(next, regex("^(true|false)$"))) {
                setArg(key, next == "true", arg);
                i++;
            } else {
                variant<string, int, bool> v;
                if (flags.strings.find(key) == flags.strings.end()) v = true;
                else v = "";
                setArg(key, v, arg);
            }
        } else if (regex_search(arg, regex("^-[^-]+"))) {
            vector<string> letters;
            string sArg = string(arg.begin() + 1, arg.end() - 1);
            for (char &c : sArg) {
                letters.push_back(string(1, c));
            }

            bool broken = false;
            for (size_t j = 0; j < letters.size(); j++) {
                next = string(arg.begin() + j + 2, arg.end());

                if (next == "-") {
                    setArg(letters[j], next, arg);
                    continue;
                }
                if (regex_search(letters[j], regex("[A-Za-z]")) && next[0] == '=') {
                    setArg(letters[j], string(next.begin() + 1, next.end()), arg);
                    broken = true;
                    break;
                }
                if (regex_search(letters[j], regex("[A-Za-z]")) && regex_search(next, regex("-?\\d+(\\.\\d*)?(e-?\\d+)?$"))) {
                    setArg(letters[j], next, arg);
                    broken = true;
                    break;
                }
                if (letters.size() > j + 1 && letters[j + 1].size() > 0 && regex_match(letters[j + 1], regex("\\W"))) {
                    setArg(letters[j], string(arg.begin() + j + 2, arg.end()), arg);
                    broken = true;
                    break;
                } else {
                    variant<string, int, bool> v;
                    if (flags.strings.find(letters[j]) == flags.strings.end()) v = true;
                    else v = "";
                    setArg(letters[j], v, arg);
                }
            }
            key = string(arg.end() - 1, arg.end());
            if (!broken && key != "-") {
                bool argsPpEx = args.size() > i + 1 && args[i + 1].size() > 0;
                if (argsPpEx && !regex_search(args[i + 1], regex("^(-|--)[^-]")) && !isBooleanKey(key)) {
                    setArg(key, args[i + 1], arg);
                    i++;
                } else if (argsPpEx && regex_search(args[i + 1], regex("^(true|false)$"))) {
                    setArg(key, args[i + 1] == "true", arg);
                    i++;
                } else {
                    variant<string, int, bool> v;
                    if (flags.strings.find(key) == flags.strings.end()) v = true;
                    else v = "";
                    setArg(key, v, arg);
                }
            }
        } else {
            if (flags.unknownFn == nullptr || flags.unknownFn == nullptr || (*flags.unknownFn)(arg) != false) {
                if (flags.strings.find("_") != flags.strings.end()) argv["_"].push_back(flags.strings["_"]);
                else if (isNumber(arg)) argv["_"].push_back(stoi(arg));
                else argv["_"].push_back(arg);
            }
            if (opts.stopEarly) {
                vector<string> v = argv["_"];
                v.insert(v.end(), args.begin() + i + 1, args.end());
                argv["_"] = v;
                break;
            }
        }
    }

    auto hasKey = [](json obj, vector<string> keys) {
        json o = obj;
        vector<string> ks(keys.begin(), keys.end() - 1);
        for (string &k : ks) {
            o = o[k] || json();
        }
        string key = keys[keys.size() - 1];
        return o.contains(key);
    };
    for (auto &&[first, second] : defaults) {
        vector<string> sp = split(string(first), ".");
        if (!hasKey(argv, sp)) {
            setKey(argv, sp, defaults[first]);
            if (aliases.find(first) != aliases.end()) {
                for (string &v : aliases[first]) {
                    setKey(argv, split(string(v), "."), defaults[first]);
                }
            }
        }
    }

    if (opts.dashdash) {
        argv["--"] = notFlags;
    } else {
        for (string &s : notFlags) {
            argv["_"].push_back(string(s));
        }
    }
    return argv;
}
