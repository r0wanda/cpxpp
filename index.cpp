#include <set>
#include <iostream>
#include <optional>
#include <variant>
#include <type_traits>
#include <nlohmann/json.hpp>
#include "help.hpp"
#include "pkgs/shellq.hpp"
#include "pkgs/minimist.hpp"

using namespace std;
using json = nlohmann::json;

int main(int argc, char **argv) {
    vector<string> _args;
    for (int i = 0; i < argc; i++) {
        if (i < 1) continue;
        _args.push_back(string(argv[i]));
    }

    set<string> unknowns;
    MinimistOpts m;
    m.alias = {
        {"c", "command"},
        {"C", "clean"},
        {"h", "help"},
        {"includeEmptyDirs", "include-empty-dirs"},
        {"L", "dereference"},
        {"p", "preserve"},
        {"t", "transform"},
        {"u", "update"},
        {"v", "verbose"},
        {"V", "version"},
        {"w", "watch"}
    };
    m.boolean = vector<string>{
        "clean",
        "dereference",
        "help",
        "include-empty-dirs",
        "initial",
        "preserve",
        "update",
        "verbose",
        "version",
        "watch"
    };
    m.def = {
        {"initial", true}
    };
    function<bool(string)> unknownFn = [&unknowns](string arg) {
        if (arg.size() < 1) return false;
        if (arg[0] == '-') unknowns.insert(arg);
        return true;
    };
    m.unknown = &unknownFn;
    json args = minimist(_args, m);

    int code = 0;
    bool _sh = false;

    if (!args.contains("_") || !args["_"].is_array() || args["_"].size() < 2) _sh = true;
    string source = _sh ? "" : args["_"][0];
    string dest = _sh ? "" : args["_"][1];

    if (unknowns.size() > 0) {
        cerr << "Unknown option(s): ";
        size_t i = 0;
        for (string o : unknowns) {
            cerr << o;
            if (i < unknowns.size() - 1) cerr << ", ";
            else cerr << endl;
            i++;
        }
        code = 1;
    } else if (args.contains("help") && args["help"] == true) {
        help();
    } else if (args.contains("version") && args["version"]) {
        cout << "1.0.0" << endl;
    } else if (source.size() < 1 || dest.size() < 1 || _sh) {
        help();
        cerr <<  "Missing either source or dest options" << endl;
        code = 1;
    } else {
        initToken();
        json env = {
            {"PWD", "/home/robot"}
        };
        env_t _env = env;
        auto res = parseShq("a \"b c\" \\$def 'it\\'s great'", &_env, nullopt);
        json j = *get_if<json>(&res);
        cout << j.dump() << endl;
    }

    return code;
}