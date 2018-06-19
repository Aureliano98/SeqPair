// autorun.cpp: simple loop for calling makefile (for portability)
// Author: LYL (Aureliano Lee)

#include <cstdlib>
#include <iostream>
#include <sstream>
#include <string>
using namespace std;

namespace {
    void print_usage() {
        cout << "Usage: alpha method [num_thrds=1]" << endl;
    }
}

int main(int argc, char **argv) {
    double alpha = 1; string method; unsigned num_thrds = 1;
    bool is_argv_valid = false;
    if (argc >= 3) {
        alpha = strtod(argv[1], nullptr);
        method = argv[2];
        if (argc >= 4)
            num_thrds = strtoull(argv[3], nullptr, 10);
        if (num_thrds)
            is_argv_valid = true;
    }
    if (!is_argv_valid) {
        print_usage();
        return -1;
    }

    string make = "make";
#ifdef MINGW32_MAKE
    make = "mingw32-make";
#endif
    
    for (auto i : { 32, 64, 128, 256, 500 }) {
        stringstream str;
        str << make << " run_packer n=" << i << " alpha=" << alpha << " method=" << method << " ";
        str << "thrds=" << num_thrds;
        cout << str.str() << endl;
        system(str.str().c_str());
    }
    return 0;
}