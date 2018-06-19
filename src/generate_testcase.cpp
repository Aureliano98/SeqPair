// generate_testcase.cpp: generates testcases from command-line arguments.
// Author: LYL (Aureliano Lee)

#include "xseqpair.h"
#include <iostream>
#include <fstream>
#include <random>
#include <string>
#include "rect.h"
#include "verification.h"

using namespace std;
namespace rp = rect_packing;

// Usage: num_rects, num_lines, min_len, max_len, rect_file, net_file
int main(int argc, char **argv) {
    try {
        bool is_argv_valid = false;
        size_t num_rects = 0, num_lines = 0;
        int min_len = 0, max_len = 0;
        string rect_file, line_file;
        if (argc == 7) {
            num_rects = strtoull(argv[1], nullptr, 10);
            num_lines = strtoull(argv[2], nullptr, 10);
            min_len = strtol(argv[3], nullptr, 10);
            max_len = strtol(argv[4], nullptr, 10);
            rect_file = argv[5];
            line_file = argv[6];
            if (num_rects && min_len && min_len <= max_len && !rect_file.empty() &&
                !line_file.empty())
                is_argv_valid = true;
            if (num_rects < 2 * num_lines) {
                is_argv_valid = false;
                cout << "Error: num_rects < 2 * num_lines." << endl;
            }
        }

        if (!is_argv_valid) {
            cout << "Usage: num_rects, num_lines, min_len, max_len, rect_file, net_file" << endl;
            return EXIT_FAILURE;
        }

        ofstream rect_out(rect_file), line_out(line_file);
        if (!rect_out.is_open() || !line_out.is_open())
            throw runtime_error("Cannot open file");
        default_random_engine eng(SEQPAIR_RANDOM_SEED());
        {
            auto layout = rp::verification::make_random_layout(num_rects, min_len, max_len, eng);
            using format_policy = decltype(layout)::format_policy;
            rect_out << layout.format(format_policy::no_delim) << endl;
        }
        {
            vector<pair<size_t, size_t>> nets(num_lines);
            bool b;
            tie(ignore, b) = rp::verification::random_scatter_to_pairs(num_rects, num_lines, 
                nets.begin(), eng);
            assert(b);
            size_t i, j;
            for (auto &&p : nets) {
                tie(i, j) = p;
                line_out << i << " " << j << "\n";
            }
        }

    } catch (std::exception &e) {
        cout << e.what() << endl;
        return EXIT_FAILURE;
    }

    return 0;
}