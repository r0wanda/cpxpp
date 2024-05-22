#include <regex>
#include <cmath>
#include <vector>
#include <string>
#include <random>
#include <sstream>
#include <variant>
#include <iomanip>
#include <iostream>
#include <optional>
#include <exception>
#include <functional>
#include <nlohmann/json.hpp>
#include "shellq.hpp"

/*
 * Copy of https://github.com/ljharb/shell-quote/
 * The original code was licensed under the MIT License -> https://github.com/ljharb/shell-quote/blob/main/LICENSE
 * This code is *almost* fully functional, the only error i could find is the incorrect parsing of escaped single quotes (which should not be common)
 * eg. When this is the input -> "a \"b c\" \\$def 'it\\'s great'"
 * The output is this -> ["a","b c","$def","it\\s great"]
 * When the expected output is this -> ["a", "b c", "$def", "it's great"]
 */

using namespace std;
using json = nlohmann::json;

// constants
const string CONTROL = "(?:\\|\\||\\&\\&|;;|\\|\\&|\\<\\(|\\<\\<\\<|>>|>\\&|<\\&|[&;()|<>])";
const regex controlRE("^" + CONTROL + "$");
const string META = "|&;()<> \\t";
const string SINGLE_QUOTE = "\"((\\\\\"|[^\"])*?)\"";
const string DOUBLE_QUOTE = "\'((\\\\\'|[^\'])*?)\'";
const regex hashRE("^#$");
const string SQ = "'";
const string DQ = "\"";
const string DS = "$";
string TOKEN;
regex startsWithToken;

void initToken() {
    stringstream tokenStrm;
    unsigned long long mult = pow(16, 8);
    random_device randDev;
    mt19937_64 rng(randDev());
    uniform_int_distribution<unsigned long long> rDist(0, mult);
    tokenStrm << hex;
    for (int i = 0; i < 4; i++) {
        tokenStrm << rDist(rng);
    }
    TOKEN = tokenStrm.str();
    startsWithToken = regex("^" + TOKEN);
}
pair<vector<vector<string>>, vector<int>> matchAll(string s, regex r, int lastIndex) {
    vector<smatch> matches;
    smatch c;
    int gP = lastIndex;
    vector<int> mIdx;
    string::const_iterator it(s.cbegin() + lastIndex);
    while (regex_search(it, s.cend(), c, r)) {
        matches.push_back(c);
        lastIndex = c.position() + c.length();
        if (lastIndex == c.position()) {
            lastIndex++;
        }
        mIdx.push_back(gP + c.position());
        it += lastIndex;
        gP += lastIndex;
        c = smatch();
    }
    vector<vector<string>> sm2s;
    for (smatch &m : matches) {
        vector<string> sv;
        for (auto &ms : m) {
            sv.push_back(ms);
        }
        sm2s.push_back(sv);
    }
    return pair<vector<vector<string>>, vector<int>>(sm2s, mIdx);
}

string getVar(env_t *env, string pre, string key) {
    function<string(string)> *sf = get_if<function<string(string)>>(env);
    function<json(string)> *jf = get_if<function<json(string)>>(env);
    json *j = get_if<json>(env);
    optional<string> sr;
    optional<json> jr;
    if (sf != nullptr) sr = (*sf)(key);
    else if (jf != nullptr) jr = (*jf)(key);
    else if (j != nullptr && j->contains(key)) sr = (*j)[key];
    string r;
    bool noV = !sr.has_value() && !jr.has_value();
    if (noV && key.size() < 1) {
        r = "";
    } else if (noV) {
        r = "$";
    }
    if (sr.has_value()) r = sr.value();
    else if (jr.has_value()) {
        return pre + TOKEN + jr.value().dump() + TOKEN;
    }
    return pre + r;
}
variant<json, vector<smatch>> parseInternal(string str, env_t *env, optional<json> _opts) {
    json opts = _opts.value_or(json({}));
    const string BS = opts.contains("escape") ? opts["escape"] : "\\";
    const string BAREWORD = "(\\" + BS + "['\"" + META + "]|[^\\s'\"" + META + "])+";
    regex chunker("(" + CONTROL + ")|(" + BAREWORD + "|" + SINGLE_QUOTE + "|" + DOUBLE_QUOTE + ")+");

    pair<vector<vector<string>>, vector<int>> matches = matchAll(str, chunker, 0);
    if (matches.first.size() < 1) return vector<smatch>();

    if (env == nullptr) {
        env_t e = json();
        env = &e;
    }

    bool commented = false;
    json r = json::array();
    int idx = 0;
    for (vector<string> &match : matches.first) {
        if (match.size() < 1) continue;
        string s = match[0];
        if (s.size() < 1 || commented) continue;
        json jOp;
        jOp["op"] = s;

        // Hand-written scanner/parser for Bash quoting rules:
		//
		// 1. inside single quotes, all characters are printed literally.
		// 2. inside double quotes, all characters are printed literally
		//    except variables prefixed by '$' and backslashes followed by
		//    either a double quote or another backslash.
		// 3. outside of any quotes, backslashes are treated as escape
		//    characters and not printed (unless they are themselves escaped)
		// 4. quote context can switch mid-token if there is no whitespace
		//     between the two quote contexts (e.g. all'one'"token" parses as
		//     "allonetoken")
        string quote = "";
        bool esc = false;
        stringstream out;
        bool isGlob = false;
        size_t i = 0;
        auto parseEnvVar = [&i, &s, &env]() {
            i++;
            size_t varend;
            string varname;
            char ch = s[i];
            if (ch == '{') {
                i++;
                if (s[i] == '}') {
                    throw runtime_error("Bad substitution: " + string(s.begin() + i - 2, s.begin() + i + 1));
                }
                varend = s.find_first_of('}', i);
                if (varend == string::npos) {
                    throw runtime_error("Bad substitution: " + string(s.begin() + i, s.end()));
                }
                varname = string(s.begin() + i, s.begin() + varend);
                i = varend;
            } else if (regex_search(string(1, ch), regex("[*@#?$!_-]"))) {
                varname = ch;
                i++;
            } else {
                string slicedFromI(s.begin() + i, s.end() - 1);
                smatch m;
                if (!regex_match(slicedFromI, m, regex("[^\\w\\d_]"))) {
                    varname = slicedFromI;
                    i = s.size();
                } else {
                    varname = string(slicedFromI.begin(), slicedFromI.begin() + m.position() - 1);
                    i += m.position() - 1;
                }
            }
            return getVar(env, "", varname);
        };

        if (regex_search(s, controlRE)) {
            r.push_back(jOp);
            goto cont;
        }

        for (i = 0; i < s.size(); i++) {
            char c = s[i];
            isGlob = isGlob || (quote.size() < 1 && (c == '*' || c == '?'));
            string cs(1, c);
            if (esc) {
                out << c;
                esc = false;
            } else if (quote.size() > 0) {
                if (string(1, c) == quote) {
                    quote = "";
                } else if (quote == SQ) {
                    out << c;
                } else {
                    cs = string(1, c);
                    if (cs == BS) {
                        i++;
                        c = s[i];
                        cs = string(1, c);
                        if (cs == DQ || cs == BS || cs == DS) {
                            out << c;
                        } else {
                            out << BS << c;
                        }
                    } else if (cs == DS) {
                        out << parseEnvVar();
                    } else {
                        out << c;
                    }
                }
            } else if (cs == DQ || cs == SQ) {
                quote = c;
            } else if (regex_search(cs, controlRE)) {
                r.push_back(jOp);
                goto cont;
            } else if (regex_search(cs, hashRE)) {
                commented = true;
                json cO;
                cO["comment"] = string(str.begin() + matches.second[idx] + i + 1, str.end());
                if (out.str().size() > 0) {
                    json jA = json::array();
                    jA.push_back(out.str());
                    jA.push_back(cO);
                    r.push_back(jA);
                    goto cont;
                } else {
                    json jA = json::array();
                    jA.push_back(cO);
                    r.push_back(jA);
                    goto cont;
                }
            } else if (cs == BS) {
                esc = true;
            } else if (cs == DS) {
                out << parseEnvVar();
            } else {
                out << c;
            }
        }
        
        if (isGlob) {
            json jG;
            jG["op"] = "glob";
            jG["pattern"] = out.str();
            r.push_back(jG);
            goto cont;
        }
        
        r.push_back(out.str());

        cont:;
        idx++;
    }
    return r;
}

variant<json, vector<smatch>> parseShq(string s, env_t *env = nullptr, optional<json> opts = nullopt) {
    if (TOKEN.size() < 1) initToken();
    variant<json, vector<smatch>> mapped = parseInternal(s, env, opts);
    json *jA = get_if<json>(&mapped);
    vector<smatch> *sA = get_if<vector<smatch>>(&mapped);
    if (sA != nullptr) return *sA;
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
    auto splitRe = [](string s, string delim) {
        vector<string> v;
        size_t pos;
        string tok;
        smatch m;
        while (regex_search(s, m, regex(delim))) {
            pos = m.position();
            tok = s.substr(0, pos);
            v.push_back(tok);
            s.erase(0, pos + m.length());
        }
        v.push_back(s);
        return v;
    };
    json jR = json::array();
    for (json &r : *jA) {
        if (!r.is_string()) {
            jR.push_back(r);
            continue;
        }
        vector<string> xs = splitRe(r, "(" + TOKEN + ".*?" + TOKEN + ")");
        if (xs.size() == 1) {
            jR.push_back(xs[0]);
        } else {
            for (string &x : xs) {
                if (x.size() < 1) continue;
                if (regex_search(x, regex(startsWithToken))) {
                    jR.push_back(json::parse(split(x, TOKEN)[1]));
                } else {
                    jR.push_back(x);
                }
            }
        }
    }
    return jR;
}
