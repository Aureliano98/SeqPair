// sa_packer.h: SaPacker (optimizer) using SA.
// Author: LYL (Aureliano Lee)

#pragma once
#include <atomic>
#include <cmath>
#include <iostream>
#include <mutex>
#include <random>
#include <thread>
#include <vector>
#include <boost/container/pmr/polymorphic_allocator.hpp>
#include <boost/container/pmr/unsynchronized_pool_resource.hpp>
#include "layout.h"
#include "pack_generator.h"

namespace rect_packing {
    // Wirelength.
    template<typename Alloc, typename FwdIt>
    double sum_manhattan_distances(const Layout<Alloc> &layout,
        FwdIt first, FwdIt last) {
        using namespace std;
        int64_t twice = 0;
        for (auto i = first; i != last; ++i) {
            auto c0 = (layout.x()[get<0>(*i)] << 1) + layout.widths()[get<0>(*i)];
            auto c1 = (layout.x()[get<1>(*i)] << 1) + layout.widths()[get<1>(*i)];
            twice += abs(c1 - c0);
        }
        for (auto i = first; i != last; ++i) {
            auto c0 = (layout.y()[get<0>(*i)] << 1) + layout.heights()[get<0>(*i)];
            auto c1 = (layout.y()[get<1>(*i)] << 1) + layout.heights()[get<1>(*i)];
            twice += abs(c1 - c0);
        }
        return twice / 2.0;
    }

    // Default packing cost.
    template<typename Alloc, typename FwdIt>
    double packing_cost(const Layout<Alloc> &layout, FwdIt first, FwdIt last,
        int w, int h, double alpha) {
        auto area = w * h;
        auto len = sum_manhattan_distances(layout, first, last);
        return alpha * area + (1 - alpha) * len;
    }

    // Base of SaPacker with default types.
    struct SaPackerBase {
        // Evaluation function (packing_cost with binded alpha). 
        struct default_energy_function {
            default_energy_function() :
                default_energy_function(1.0) { }
            default_energy_function(double alpha) : alpha(alpha) {}

            template<typename Alloc, typename FwdIt>
            double operator()(const Layout<Alloc> &layout, FwdIt first,
                FwdIt last, int w, int h) const {
                return packing_cost(layout, first, last, w, h, alpha);
            }

            double alpha;
        };

        // Options for simulated annealing.
        struct options_t {
            options_t() = default;
            options_t(double init_accept_prob, std::size_t reps_per_t, 
                double dec_ratio, double restart_r, double stop_accept_prob) :
                initial_accepting_probability(init_accept_prob),
                decreasing_ratio(dec_ratio),
                simulaions_per_temperature(reps_per_t),
                restart_ratio(restart_r),
                stopping_accepting_probability(stop_accept_prob) { }

            double initial_accepting_probability = 0.9;
            std::size_t simulaions_per_temperature = 1024;
            double decreasing_ratio = 0.99;
            double restart_ratio = 2;
            double stopping_accepting_probability = 0.05;
        };
    };

    std::istream &operator>>(std::istream &in, typename SaPackerBase::options_t &opts) {
        //in >> opts.initial_simulations;
        in >> opts.initial_accepting_probability;
        in >> opts.simulaions_per_temperature;
        in >> opts.decreasing_ratio;
        in >> opts.restart_ratio;
        in >> opts.stopping_accepting_probability;
        return in;
    }

    std::ostream &operator<<(std::ostream &out, const typename SaPackerBase::options_t &opts) {
        using namespace std;
        cout << "initial_accepting_probability: " << opts.initial_accepting_probability << "\n";
        cout << "simulations_per_temperature: " << opts.simulaions_per_temperature << "\n";
        cout << "decreasing_ratio: " << opts.decreasing_ratio << "\n";
        cout << "restart_ratio: " << opts.restart_ratio << "\n";
        cout << "stopping_accepting_probability: " << opts.stopping_accepting_probability << "\n";
        return out;
    }

    // Simulated annealing packer.
    template<typename Generator, typename EFunc = 
        typename SaPackerBase::default_energy_function>
    class SaPacker : public SaPackerBase {
        using self_t = SaPacker;
        using base_t = SaPackerBase;

    public:
        using typename base_t::options_t;
        using typename base_t::default_energy_function;
        using generator_t = typename Generator::unbuffered_generator_t;
        using energy_function_t = EFunc;
        struct sequenced_policy { };
        struct parallel_policy { };
        static constexpr sequenced_policy seq = sequenced_policy();
        static constexpr parallel_policy par = parallel_policy();
        
    protected:
        using generator_allocator_type = typename generator_t::allocator_type;
        using generator_default_change_distribution = 
            typename generator_t::default_change_distribution;

    public:
        explicit SaPacker(const options_t &opts = options_t(),
            const energy_function_t &func = energy_function_t(),
            const generator_allocator_type &alloc = generator_allocator_type()) : 
            _opts(_checked_option(opts)), _energy_func(func), _eng(SEQPAIR_RANDOM_SEED()), 
            _generator(alloc) { }

        const options_t &options() const {
            return _opts;
        }

        void set_options(const options_t &opts) {
            _opts = _checked_option(opts);
        }

        const energy_function_t &energy_function() const {
            return _energy_func;
        }

        void set_energy_function(const energy_function_t &func) {
            _energy_func = func;
        }

        const generator_t &generator() const {
            return _generator;
        }

        // Generates the solution and writes it to layout.
        template<typename LayoutAlloc, typename FwdIt,
            typename ChgDist = generator_default_change_distribution,
            typename Alloc = generator_allocator_type>
            double operator()(Layout<LayoutAlloc> &layout, FwdIt first_line, FwdIt last_line,
                ChgDist &&chg_dist = ChgDist(), Alloc &&alloc = Alloc(), 
                unsigned verbose_level = 1) {
            using namespace std;
            if (layout.empty())
                return 0;

            size_t num_simulations = 0;

            // Deferred generator construction from layout.
            _generator.construct(layout.widths(), layout.heights(), _eng); 
            auto best_gen = _generator;
            auto res = _generator.make_resource();
            
            // Initial loop for determining starting temperature.
            auto local_layout = layout;
            auto best_layout = layout;
            double min_energy = numeric_limits<double>().max(), 
                max_energy = numeric_limits<double>().min();
            double curr_energy, last_energy;
            double sum_energies = 0, sum_sqrs = 0;   // For stddev
            
            if (verbose_level) cout << "\n";
            constexpr size_t init_sims = 64;  
            for (size_t i = 0; i != init_sims; ++i) {
                int w, h;
                std::tie(w, h) = _generator(local_layout, _eng, res, chg_dist, alloc);
                curr_energy = _energy_func(local_layout, first_line, last_line, w, h);
                ++num_simulations;
                if (curr_energy < min_energy) {
                    detail::unguarded_copy_layout(local_layout, best_layout);
                    detail::unguarded_copy_generator(_generator, best_gen);
                    min_energy = curr_energy;
                }
                sum_energies += curr_energy;
                sum_sqrs += curr_energy * curr_energy;
                max_energy = max(max_energy, curr_energy);
                last_energy = curr_energy;
                _generator.shuffle(_eng);
            }
            
            auto stddev = sqrt((sum_sqrs - sum_energies * sum_energies / init_sims) / 
                (init_sims - 1));
            double temp = (stddev + numeric_limits<double>().epsilon()) / 
                log(1.0 / _opts.initial_accepting_probability);

            if (verbose_level) {
                cout << "Starting temperature: " << temp << "\n";
                cout << "Starting min energy: " << min_energy << "\n";
                cout << "Starting max energy: " << max_energy << "\n";
                cout << "Stddev: " << stddev << "\n";
                if (verbose_level >= 2)
                    cout << "\n";
            }

            // Main simulation process.
            constexpr double temp_guard = 1.0;
            uniform_real_distribution<> rand_double(0, 1);
            size_t num_restarts = 0;

            for (;;) {
                size_t num_acceptions = 0;
                double my_sum_energies = 0;

                for (size_t i = 0; i != _opts.simulaions_per_temperature; ++i) {
                    int w, h;
                    std::tie(w, h) = _generator(local_layout, _eng, res, chg_dist, alloc);
                    ++num_simulations;
                    auto new_energy = _energy_func(local_layout, first_line, last_line, w, h);
                    my_sum_energies += new_energy;

                    if (new_energy < curr_energy ||
                        rand_double(_eng) < exp((curr_energy - new_energy) / temp)) {
                        if (new_energy < min_energy) {
                            detail::unguarded_copy_layout(local_layout, best_layout);
                            detail::unguarded_copy_generator(_generator, best_gen);
                            min_energy = new_energy;
                        }
                        curr_energy = new_energy;
                        ++num_acceptions;
                    } else {
                        _checked_undo(std::forward<ChgDist>(chg_dist));
                    }
                }
                
                if (verbose_level >= 2) {
                    cout << "Temperature: " << temp << ", average energy: " <<
                        my_sum_energies / _opts.simulaions_per_temperature <<
                        ", acception rate: " << static_cast<double>(num_acceptions) /
                        _opts.simulaions_per_temperature << "\n";
                }

                // Terminate criterion
                if (static_cast<double>(num_acceptions) < 
                    _opts.stopping_accepting_probability * _opts.simulaions_per_temperature ||
                    temp < temp_guard)  // Usually this doens't happen, in certain cases this is necessary
                    break;

                // Restart if necessary
                // Note: based on average or current? (experiment shows that average-based 
                // restart is better)
                if (my_sum_energies / _opts.simulaions_per_temperature >
                    _opts.restart_ratio * min_energy) {
                    detail::unguarded_copy_layout(best_layout, local_layout);  // Not compulsory
                    detail::unguarded_copy_generator(best_gen, _generator);
                    curr_energy = min_energy;
                    ++num_restarts;
                }

                // Drop temperature
                temp *= _opts.decreasing_ratio;
            }

            // Output results
            if (verbose_level) {
                cout << "\n";
                cout << "Finishing temperature: " << temp << "\n";
                cout << "Finishing energy: " << curr_energy << "\n";
                cout << "Total simulations: " << num_simulations << "\n";
                cout << "Total restarts: " << num_restarts << "\n";
            }
            layout = std::move(best_layout);
            return min_energy;
        }

        // Generates the solution and writes it to layout.
        template<typename LayoutAlloc, typename FwdIt,
            typename ChgDist = generator_default_change_distribution,
            typename Alloc = generator_allocator_type>
            double operator()(sequenced_policy, Layout<LayoutAlloc> &layout, 
                FwdIt first_line, FwdIt last_line,
                ChgDist &&chg_dist = ChgDist(), Alloc &&alloc = Alloc(),
                unsigned verbose_level = 1) {
            return this->operator()(layout, first_line, last_line, 
                std::forward<ChgDist>(chg_dist), std::forward<Alloc>(alloc), verbose_level);
        }

        // Generates the solution and writes it to layout.
        // When using parallel version, it is recommended to use 
        // restart_ratio = 2 + 0.3 * (n - 1) / 7 .
        template<typename LayoutAlloc, typename FwdIt,
            typename ChgDist = generator_default_change_distribution,
            typename Alloc = generator_allocator_type>
            double operator()(parallel_policy, Layout<LayoutAlloc> &layout,
                FwdIt first_line, FwdIt last_line,
                ChgDist &&chg_dist = ChgDist(), Alloc &&alloc = Alloc(),
                unsigned verbose_level = 1) {
            auto num_thrds = max(thread::hardware_concurrency(), 2u);
            return this->operator()(parallel_policy(), layout, first_line, last_line, 
                std::forward<ChgDist>(chg_dist), std::forward<Alloc>(alloc), verbose_level, num_thrds);
        }

        // Generates the solution and writes it to layout.
        // When using parallel version, it is recommended to use 
        // restart_ratio = 2 + 0.3 * (n - 1) / 7 .
        template<typename LayoutAlloc, typename FwdIt,
            typename ChgDist, typename Alloc>
            double operator()(parallel_policy, Layout<LayoutAlloc> &layout, 
                FwdIt first_line, FwdIt last_line, ChgDist &&chg_dist, Alloc &&alloc,
                unsigned verbose_level, unsigned num_thrds) {
            using namespace std;
            if (layout.empty())
                return 0;
            if (num_thrds < 2)
                return this->operator()(layout, first_line, last_line, 
                    std::forward<ChgDist>(chg_dist), std::forward<Alloc>(alloc),
                    verbose_level);

            size_t num_simulations = 0;

            // Deferred generator construction from layout.
            _generator.construct(layout.widths(), layout.heights(), _eng);
            auto best_gen = _generator;
            auto res = _generator.make_resource();

            // Initial loop for determining starting temperature.
            auto main_layout = layout;
            auto best_layout = layout;
            atomic<double> min_energy = numeric_limits<double>().max();
            double max_energy = numeric_limits<double>().min();
            double curr_energy, last_energy;
            double sum_energies = 0, sum_sqrs = 0;   // For stddev

            constexpr size_t init_sims = 64;
            for (size_t i = 0; i != init_sims; ++i) {
                int w, h;
                std::tie(w, h) = _generator(main_layout, _eng, res, chg_dist, alloc);
                curr_energy = _energy_func(main_layout, first_line, last_line, w, h);
                ++num_simulations;
                if (curr_energy < min_energy) {
                    detail::unguarded_copy_layout(main_layout, best_layout);
                    detail::unguarded_copy_generator(_generator, best_gen);
                    min_energy = curr_energy;
                }
                sum_energies += curr_energy;
                sum_sqrs += curr_energy * curr_energy;
                max_energy = max(max_energy, curr_energy);
                last_energy = curr_energy;
                _generator.shuffle(_eng);
            }

            auto stddev = sqrt((sum_sqrs - sum_energies * sum_energies / init_sims) /
                (init_sims - 1));
            atomic<double> temp = (stddev + numeric_limits<double>().epsilon()) /
                log(1.0 / _opts.initial_accepting_probability);

            if (verbose_level) {
                cout << "\n";
                cout << "Starting temperature: " << temp << "\n";
                cout << "Starting min energy: " << min_energy << "\n";
                cout << "Starting max energy: " << max_energy << "\n";
                cout << "Stddev: " << stddev << "\n";
                if (verbose_level >= 2)
                    cout << "\n";
            }

            // Main simulation process.
            // Shared constants
            const auto simulations_per_thrd = (_opts.simulaions_per_temperature + num_thrds - 2) /
                (num_thrds - 1);
            // Below may be different from _opts.simulations_per_temperature
            const auto actual_simulations_per_temp = simulations_per_thrd * (num_thrds - 1);  
                        
            // Shared variables
            mutex cout_mutex, best_sln_mutex, sync_mutex;
            condition_variable ctrl_cond, feedback_cond;
            vector<bool> thrd_is_ready(num_thrds - 1, true);
            size_t num_finished_thrds = 0;
            bool stop_simulation = false;
            atomic<size_t> loop_num_acceptions = 0;
            vector<generator_t> thrd_generators(num_thrds - 1, _generator);
            vector<double> thrd_curr_energies(num_thrds - 1, curr_energy);
            vector<double> thrd_avg_energies(num_thrds - 1, 0);
            vector<Layout<LayoutAlloc>> thrd_layouts(num_thrds - 1, main_layout);
            vector<decltype(res)> thrd_resources(num_thrds - 1, res);
            vector<energy_function_t> thrd_energy_funcs(num_thrds - 1, _energy_func);
            vector<decay_t<ChgDist>> thrd_chg_dists(num_thrds - 1, chg_dist);

            vector<thread> thrds;
            thrds.reserve(num_thrds - 1);
            for (auto i = num_thrds - 1; i--; ) {
                thrds.emplace_back([&, i, curr_energy] {
                    // No memory allocation for these stuff in the thread, so using 
                    // customized allocators is OK
                    auto &my_layout = thrd_layouts[i];   
                    auto &my_res = thrd_resources[i];  
                    auto &my_energy_func = thrd_energy_funcs[i];
                    auto &my_chg_dist = thrd_chg_dists[i];

                    // Alias for clarity
                    auto my_curr_energy = curr_energy;

                    // Thread local variables
                    boost::container::pmr::unsynchronized_pool_resource my_pool_resource;
                    boost::container::pmr::polymorphic_allocator<char> my_alloc(
                        std::addressof(my_pool_resource));  // Each thread allocates its memory
                    default_random_engine my_eng(SEQPAIR_RANDOM_SEED());
                    uniform_real_distribution<> rand_double(0, 1);

                    for (;;) {
                        // Get my generator (this changes in different rounds)
                        auto &my_gen = thrd_generators[i];
                        assert(!my_gen.empty());

                        // Wait for continue / stop signal
                        {
                            unique_lock<mutex> lk(sync_mutex);  
                            while (!(thrd_is_ready[i] || stop_simulation))
                                ctrl_cond.wait(lk);
                            if (stop_simulation) 
                                break;
                        }

                        // Simulation
                        size_t my_num_acceptions = 0;
                        double my_sum_energies = 0;
                        for (size_t j = 0; j != simulations_per_thrd; ++j) {
                            int w, h;
                            std::tie(w, h) = my_gen(my_layout, my_eng, my_res, my_chg_dist, my_alloc);
                            auto new_energy = my_energy_func(my_layout, first_line, last_line, w, h);
                            my_sum_energies += new_energy;

                            if (new_energy < my_curr_energy ||
                                rand_double(my_eng) < exp((my_curr_energy - new_energy) / temp)) {
                                if (new_energy < min_energy) {
                                    lock_guard<mutex> lg(best_sln_mutex);
                                    if (new_energy < min_energy) {  // Double check
                                        detail::unguarded_copy_layout(my_layout, best_layout);
                                        detail::unguarded_copy_generator(my_gen, best_gen);
                                        min_energy = new_energy;
                                    }
                                }
                                my_curr_energy = new_energy;
                                ++my_num_acceptions;
                            } else {
                                _checked_undo(my_gen, std::forward<ChgDist>(my_chg_dist));
                            }
                        }

                        // Feedback to main thread
                        thrd_is_ready[i] = false;   // Can only be set true again by the main thread
                        thrd_curr_energies[i] = my_curr_energy;
                        thrd_avg_energies[i] = my_sum_energies / simulations_per_thrd;
                        loop_num_acceptions += my_num_acceptions;
                        
                        unique_lock<mutex> lk(sync_mutex);
                        if (++num_finished_thrds == num_thrds - 1) {
                            lk.unlock();    // Manual unlock
                            feedback_cond.notify_one();
                        }
                    }
                }); // Thread lambda
            }
            
            // Main thread
            constexpr double temp_guard = 1.0;
            vector<generator_t> next_priv_generators(num_thrds - 1, _generator);
            vector<double> next_priv_curr_energies(num_thrds - 1, 0);
            vector<double> dist_func(num_thrds - 1, 0);
            uniform_real_distribution<> random(0, 1);
            size_t num_restarts = 0;

            // Initial signal
            ctrl_cond.notify_all();

            for (;;) {
                // ...
                // Wait till all threads complete simulation
                {
                    unique_lock<mutex> lk(sync_mutex);
                    while ((num_finished_thrds != num_thrds - 1))
                        feedback_cond.wait(lk);
                    num_simulations += actual_simulations_per_temp;

                    if (verbose_level >= 2) {
                        cout << "Temperature: " << temp << ", ";
                        cout << "average energy: " << accumulate(thrd_curr_energies.cbegin(),
                            thrd_curr_energies.cend(), 0.0) / thrd_curr_energies.size() <<
                            ", acception rate: " << static_cast<double>(loop_num_acceptions) /
                            actual_simulations_per_temp << "\n";
                    }

                    // Termination criterion
                    if (static_cast<double>(loop_num_acceptions) <
                        _opts.stopping_accepting_probability * actual_simulations_per_temp ||
                        temp < temp_guard) {
                        stop_simulation = true;
                        ctrl_cond.notify_all();
                        break;
                    }

                    // Compute the distribution function of Boltzmann distribution
                    copy(thrd_curr_energies.data(), thrd_curr_energies.data() + 
                        thrd_curr_energies.size(), dist_func.data());
                    auto avg = accumulate(dist_func.cbegin(), dist_func.cend(), 0.0) / dist_func.size();
                    for (auto &e : dist_func)
                        e -= avg;   // This is for avoiding overflow / underflow of exponents
                    for (auto &e : dist_func)
                        e = exp(-e / temp);
                    partial_sum(dist_func.cbegin(), dist_func.cend(), dist_func.begin());
                    auto total = dist_func.back();
                    for (auto &e : dist_func)
                        e /= total;

                    // Select generators for next round, restart if necessary
                    for (size_t i = 0; i != thrd_generators.size(); ++i) {
                        // Select thrd_generators[k] 
                        auto k = lower_bound(dist_func.cbegin(), dist_func.cend(),
                            random(_eng)) - dist_func.cbegin();
                        assert(k != dist_func.size());
                        if (verbose_level >= 3) {
                            cout << k << " th generator, energy: " << 
                                thrd_curr_energies[k];
                        }

                        // Note: base on average or current energy?
                        if (thrd_curr_energies[k] > _opts.restart_ratio * min_energy) {
                            // If average is too high, restart it
                            detail::unguarded_copy_generator(best_gen, next_priv_generators[i]);
                            next_priv_curr_energies[i] = min_energy;
                            ++num_restarts;
                            if (verbose_level >= 3)
                                cout << " restarted\n";
                        } else {
                            // Average is OK, use selected generator
                            detail::unguarded_copy_generator(thrd_generators[k], 
                                next_priv_generators[i]);   // Cannot move here
                            next_priv_curr_energies[i] = thrd_curr_energies[k];
                            if (verbose_level >= 3)
                                cout << " accepted\n";
                        }
                    }

                    swap(thrd_generators, next_priv_generators);
                    swap(thrd_curr_energies, next_priv_curr_energies);

                    // Drop temperature
                    temp = temp * _opts.decreasing_ratio;

                    fill(thrd_is_ready.begin(), thrd_is_ready.end(), true);
                    num_finished_thrds = 0;
                    loop_num_acceptions = 0;
                }

                // Signal
                ctrl_cond.notify_all();
            }

            for (auto &&t : thrds)
                t.join();

            // Output results
            if (verbose_level) {
                cout << "\n";
                cout << "Finishing temperature: " << temp << "\n";
                cout << "Finishing average energy: " << 
                    accumulate(thrd_curr_energies.cbegin(), thrd_curr_energies.cend(), 0.0) / 
                    thrd_curr_energies.size() << "\n";
                cout << "Total simulations: " << num_simulations << "\n";
                cout << "Total restarts: " << num_restarts << "\n";
            }
            layout = std::move(best_layout);
            return min_energy;
        }

    protected: 
        // Checks option.
        bool _is_option_valid(const options_t &opts) const noexcept {
            return opts.initial_accepting_probability > 0 &&
                opts.initial_accepting_probability < 1 &&
                opts.simulaions_per_temperature &&
                opts.decreasing_ratio > 0 &&
                opts.decreasing_ratio < 1 &&
                opts.restart_ratio > 1 &&
                opts.stopping_accepting_probability > 0 &&
                opts.stopping_accepting_probability <= 1;
        }

        // Throws on invalid option.
        const options_t &_checked_option(const options_t &opts) const {
            if (!_is_option_valid(opts))
                throw std::invalid_argument("Invalid argument");
            return opts;
        }

        // Invokes generator_t::rollback and checks the return value.
        template<typename ChgDist>
        bool _checked_undo(ChgDist &&chg_dist) {
            return _checked_undo(_generator, chg_dist);
        }

        // Invokes generator_t::rollback and checks the return value.
        template<typename ChgDist>
        bool _checked_undo(generator_t &gen, ChgDist &&chg_dist) const {
            auto b = gen.rollback();
            assert(detail::may_change_be_none(std::forward<ChgDist>(chg_dist)) || b);
            return b;
        }

        // Note: actually _energy_func had better be stored in boost::compressed_pair
        options_t _opts;
        energy_function_t _energy_func; 
        std::default_random_engine _eng;
        generator_t _generator;
    };

    // Helper function for constructing SaPacker.
    template<typename Generator, typename EFunc =
        typename SaPackerBase::default_energy_function, typename... Types>
        SaPacker<Generator, std::decay_t<EFunc>>
        makeSaPacker(const typename SaPackerBase::options_t &opts =
            typename SaPackerBase::options_t(), EFunc &&func = EFunc(),
            Types &&...args) {
        return SaPacker<Generator, std::decay_t<EFunc>>(opts,
            std::forward<EFunc>(func), std::forward<Types>(args)...);
    }
}
