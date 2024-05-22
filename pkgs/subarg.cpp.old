#include <regex>
#include <string>
#include <vector>
#include <variant>
#include <iostream>
#include <nlohmann/json.hpp>
#include "subarg.hpp"
#include "minimist.hpp"

using namespace std;
using namespace nlohmann;

json subarg(vector<string> args) {
    int level = 0;
    int index;
    json r;
    vector<string> args_;
    for (int i = 0; i < args.size(); i++) {
        cout << args[i] << endl;
        if (regex_search(args[i], regex("^\\["))) {
            if (level++ == 0) index = i;
        }
        if (regex_search(args[i], regex("\\]$"))) {
            if (--level > 0) continue;
            vector<string> sub(args.begin() + index, args.begin() + i + 1);
            sub[0] = regex_replace(sub[0], regex("^\\["), "");
            if (sub[0].size() < 1) sub.erase(sub.begin());
            size_t n = sub.size() - 1;
            sub[n] = regex_replace(sub[n], regex("\\]$"), "");
            if (sub[n].size() < 1) sub.erase(sub.end() - 1);
            //args_.push_back(subarg(sub));
            cout << "level:" << level << "arg:";
            for (string &s : sub) {
                cout << s << ",";
            }
            cout << endl;
        } else if (level == 0) args_.push_back(args[i]);
    }
    return r;
}