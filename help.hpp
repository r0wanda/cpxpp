#pragma once
#include <string>
#include <iostream>

using namespace std;

const string helptxt = "Usage: cpx <source> <dest> [options]\n\n"
"    Copy files, watching for changes.\n\n"
"        <source>  The glob of target files.\n"
"        <dest>    The path of a destination directory.\n\n"
"Options:\n"
"    -c, --command <command>   A command text to transform each file.\n"
"    -C, --clean               Clean files that matches <source> like pattern in\n"
"                              <dest> directory before the first copying.\n"
"    -L, --dereference         Follow symbolic links when copying from them.\n"
"    -h, --help                Print usage information.\n"
"    --include-empty-dirs      The flag to copy empty directories which is\n"
"                              matched with the glob.\n"
"    --no-initial              The flag to not copy at the initial time of watch.\n"
"                              Use together '--watch' option.\n"
"    -p, --preserve            The flag to copy attributes of files.\n"
"                              This attributes are uid, gid, atime, and mtime.\n"
"    -t, --transform <name>    A module name to transform each file. cpx lookups\n"
"                                the specified name via \"require()\".\n"
"    -u, --update              The flag to not overwrite files on destination if\n"
"                              the source file is older.\n"
"    -v, --verbose             Print copied/removed files.\n"
"    -V, --version             Print the version number.\n"
"    -w, --watch               Watch for files that matches <source>, and copy\n"
"                              the file to <dest> every changing.\n\n"
"Examples:\n\n"
"    cpx \"src/**/*.{html,png,jpg}\" app\n"
"    cpx \"src/**/*.css\" app --watch --verbose\n\n"
"See Also:\n"
"    https://github.com/mysticatea/cpx\n";

void help() {
    cout << helptxt;
}
