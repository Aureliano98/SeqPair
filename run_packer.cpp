// run_packer.cpp: entry to sequence pair project.
// Author: LYL (Aureliano Lee)

#include "xseqpair.h"
#include <chrono>
#include <fstream>
#include <iostream>
#include <string>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include <boost/container/pmr/synchronized_pool_resource.hpp>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/program_options.hpp>
#include "aureliano/timeit.h"
#include "aureliano/toolbox.h"
#include "layout.h"
#include "pack_generator.h"
#include "sa_packer.h"
#include "verification.h"

using namespace std;
using namespace rect_packing;

namespace {

    template<typename Generator, typename Alloc, typename FwdIt>
    void run_packer(SaPacker<Generator> &packer, Layout<Alloc> &layout, 
        FwdIt first_line, FwdIt last_line, ostream &out, unsigned num_thrds, 
        unsigned verbose_level) {
        using namespace rect_packing::verification;
        using change_t = PackGeneratorBase::change_t;

        cout << "Threads: " << num_thrds << "\n";
        cout << packer.options();

        // Change distribution and runtime allocator
        // Note: maybe pool_options can be specified
        PackGeneratorBase::default_change_distribution chg_dist;
        boost::container::pmr::unsynchronized_pool_resource pool_resource;
        boost::container::pmr::polymorphic_allocator<char> pmr_alloc(
            std::addressof(pool_resource));

        double cost = 0;
        auto runtime = aureliano::timeit([&] {
            if (num_thrds <= 1)
                cost = packer(layout, first_line, last_line,
                    chg_dist, pmr_alloc, verbose_level);
            else
                cost = packer(packer.par, layout, first_line, last_line,
                    chg_dist, pmr_alloc, verbose_level, num_thrds);
        });

        cout << "\n";
        cout << "Runtime: " <<
            chrono::duration_cast<chrono::milliseconds>(runtime).count() <<
            "ms" << "\n";
        auto sum_rect_areas = layout.sum_conponent_areas();
        cout << "Sum of rectangle areas: " << sum_rect_areas << "\n";
        auto sln_area = layout.get_area();
        {
            using namespace rect_packing::io;
            cout << "Area: " << sln_area.first * sln_area.second <<
                " " << sln_area << "\n";
        }
        cout << "Utilization: " << 1.0 * sum_rect_areas /
            (sln_area.first * sln_area.second) << "\n";
        auto wirelen = sum_manhattan_distances(layout, first_line, last_line);
        cout << "Wirelength: " << wirelen << "\n";
        cout << "Cost: " << cost << "\n";

        auto alpha = packer.energy_function().alpha;
        if (abs(alpha * sln_area.first * sln_area.second + (1 - alpha) * wirelen - cost) > 
            16 * numeric_limits<double>().epsilon())
            cout << "Wrong answer: incorrect cost." << "\n";
        else if (has_intersection(layout))
            cout << "Wrong answer: layout contains intersections." << "\n";
        else
            cout << "Answer accepted.\n";

        using format_policy = typename Layout<Alloc>::format_policy;
        out << layout.format(format_policy::no_delim);
    }

    void print_usage() {
        cout << "Usage: rect_file, net_file, alpha, method, "
            "result_file [num_thrds=1] [verbose_level=1] [option_file]" << "\n";
    }
}


int main(int argc, char **argv) {
    try {
        bool is_argv_valid = false;
        if (argc < 6) {
            print_usage();
            return EXIT_FAILURE;
        }

        string rect_file = argv[1], net_file = argv[2];
        double alpha = strtod(argv[3], nullptr);
        string method = argv[4];
        string result_file = argv[5];
        unsigned num_thrds = 1;
        if (argc > 6)
            num_thrds = strtoull(argv[6], nullptr, 10);
        unsigned verbose_level = 1;
        if (argc > 7)
            verbose_level = strtoul(argv[7], nullptr, 10);
        string opt_file;
        if (argc > 8)
            opt_file = argv[8];
        if (argc > 9)
            cout << "Warning: extra command-line arguments are ommitted." << "\n";

        for (auto &e : method)
            e = tolower(e);
        if (num_thrds && (method == "lcs" || method == "dag"))
            is_argv_valid = true;
        if (!is_argv_valid) {
            print_usage();
            return EXIT_FAILURE;
        }

        Layout<> layout;
        {
            ifstream in(rect_file);
            if (!in.is_open())
                throw runtime_error("Cannot open file");
            in >> layout;
        }

        vector<pair<size_t, size_t>> nets;
        {
            ifstream in(net_file);
            if (!in.is_open())
                throw runtime_error("Cannot open file");
            size_t i, j;
            while (in >> i >> j) {
                nets.emplace_back(i, j);
            }
            using const_reference = typename decltype(nets)::const_reference;
            if (any_of(nets.cbegin(), nets.cend(), [&](const_reference p) {
                return p.first >= layout.size() || p.second >= layout.size(); }))
                throw invalid_argument("Net index out of range");
        }

        SaPackerBase::options_t opts;
        if (!opt_file.empty()) {
            ifstream in(opt_file);
            if (!in.is_open())
                throw runtime_error("Cannot open file");
            in >> opts; // Params
        } else {
            opts.simulaions_per_temperature = max(30 * layout.size(), static_cast<size_t>(1024));
            if (num_thrds >= 2)
                opts.simulaions_per_temperature *= static_cast<size_t>((num_thrds + 1) / 2);
            if (num_thrds >= 2)
                opts.restart_ratio = 2.3;
        }

        cout << "Rectangles: " << layout.size() << "\n";
        cout << "Alpha: " << alpha << "\n" << "\n";
        SaPackerBase::default_energy_function func(alpha);
        {
            ofstream out(result_file);
            if (method == "dag") {
                cout << "Method: DAG" << "\n";
                auto packer = makeSaPacker<DagPackGenerator<>>(opts, func);
                run_packer(packer, layout, begin(nets), end(nets), out, num_thrds, verbose_level);

            } else if (method == "lcs") {
                cout << "Method: LCS" << "\n";
                auto packer = makeSaPacker<LcsPackGenerator<>>(opts, func);
                run_packer(packer, layout, begin(nets), end(nets), out, num_thrds, verbose_level);
                
            } else {
                assert(false);
            }
        }

    } catch (std::exception &e) {
        cout << e.what() << "\n";
        return EXIT_FAILURE;
    }
    
    return EXIT_SUCCESS;
}