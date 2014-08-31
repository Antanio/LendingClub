/*
Created on July 27, 2014

@author:     Gregory Czajkowski

@copyright:  2013 Freedom. All rights reserved.

@license:    Licensed under the Apache License 2.0 http://www.apache.org/licenses/LICENSE-2.0

@contact:    gregczajkowski at yahoo.com
*/

#ifndef __LC_LCGA_HPP__
#define __LC_LCGA_HPP__

#include <vector>
#include <string>
#include <chrono>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <functional>
#include <map>

#include "Arguments.hpp"
#include "LCBT.hpp"

namespace lc 
{

class GATest
{
    typedef std::vector<std::pair<LoanReturn, std::vector<VariantFilter>>> PopulationType;
public:
    GATest(const std::vector<Loan::LoanType>& backtest_filters, LCBT& lcbt) :
        _lcbt(lcbt),
        _args(LCArguments::Get()),
        _iteration(0),
        _iteration_time(0),
        _best_net_apy(0.0)
    {
        unsigned population_size = _args["population_size"].as<unsigned>();
        _population.reserve(population_size);
        _mate_population.reserve(population_size);
        _iterations = _args["iterations"].as<unsigned>();

        std::hash<lc::FilterValue> filter_hash;

        filter_count_visitor count_visitor;
        filter_value_visitor value_visitor;
        filter_set_current_visitor current_visitor;

        for (unsigned i = 0; i < population_size; ++i) {
            std::vector<VariantFilter> filters(backtest_filters.size());
            std::vector<VariantFilter> mate_filters(backtest_filters.size());

            // Create each of the filters
            unsigned j = 0;
            for (auto& filter_type : backtest_filters) {
                auto filter_it = filters.begin() + j;
                construct_variant_filter(filter_type, filter_it);
                auto mate_filter_it = mate_filters.begin() + j;
                construct_variant_filter(filter_type, mate_filter_it);
                ++j;
            }

            unsigned hash_result = 0;
            do {                
                hash_result = 0;

                unsigned j = 0;
                for (auto& filter_type : backtest_filters) {
                    auto filter_it = filters.begin() + j;
                    auto& f = *filter_it;
                    current_visitor.set(randint(0, f.apply_visitor(count_visitor) - 1));
                    f.apply_visitor(current_visitor);

                    hash_result = hash_result ^ (filter_hash(f.apply_visitor(value_visitor) << 1));
                    ++j;
                }
            // Keep randomizing until we find a filter set we haven't used before
            } while (_memoized_filters.find(hash_result) != _memoized_filters.end());

            _population.push_back(std::make_pair(LoanReturn(), filters));
            _mate_population.push_back(std::make_pair(LoanReturn(), mate_filters));

            ++_memoized_filters[hash_result];
        }

        assert(population_size > 0);

        _csv_file.open(_args["csvresults"].as<std::string>().c_str());

        std::vector<std::string> csv_field_names;
        static filter_name_visitor name_visitor;
        for (auto& lc_filter : _population[0].second) {
            _csv_file << lc_filter.apply_visitor(name_visitor) << ',';
        }

        _csv_file << "expected_apy,num_loans,num_defaulted,pct_defaulted,avg_default_loss,net_apy\n";
    }

    void run()
    {
        _iteration_time = _iteration_time.zero();
        
        for (_iteration = 0; _iteration < _iterations; ++_iteration) {
            std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();

            calculate_fitness();
            sort_by_fitness();

            std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

            _iteration_time += end - start;

            print_best();
            mate();
        }
    }

    void calculate_fitness()
    {
        for (auto& citizen : _population) {
            citizen.first = _lcbt.test(citizen.second);
        }
    }

    struct net_apy_cmp
    {
        net_apy_cmp(unsigned config_fitness_sort_num_loans) : config_fitness_sort_num_loans(config_fitness_sort_num_loans) {}

        inline bool operator() (const std::pair<LoanReturn, std::vector<VariantFilter>>& a, const std::pair<LoanReturn, std::vector<VariantFilter>>& b)
        {
            double a_fit = (a.first.num_loans >= config_fitness_sort_num_loans) ? a.first.net_apy : 0.0;
            double b_fit = (b.first.num_loans >= config_fitness_sort_num_loans) ? b.first.net_apy : 0.0;
            return b_fit < a_fit;
        }

        unsigned config_fitness_sort_num_loans;
    };

    void sort_by_fitness()
    {
        auto config_fitness_sort_num_loans = _args["fitness_sort_size"].as<unsigned>();
        std::sort(_population.begin(), _population.end(), net_apy_cmp(config_fitness_sort_num_loans));
    }

    void print_best()
    {
        auto& best_results = _population[0].first;
        auto& best_filters = _population[0].second;

        double expected_apy = 0.0, pct_defaulted = 0.0, net_apy = 0.0, avg_default_loss = 0.0;
        unsigned num_defaulted = 0, loans_per_month = 0;
        if (best_results.num_loans >= _args["fitness_sort_size"].as<unsigned>()) {
            loans_per_month = best_results.loans_per_month;
            expected_apy = best_results.expected_apy;
            num_defaulted = best_results.num_defaulted;
            pct_defaulted = best_results.pct_defaulted;
            avg_default_loss = best_results.avg_default_loss;
            net_apy = best_results.net_apy;
        }

        static filter_name_visitor name_visitor;
        static filter_stringify_visitor stringify_visitor;

        if (net_apy > _best_net_apy) {

            for (auto& lc_filter : best_filters) {
                auto filter_name = lc_filter.apply_visitor(name_visitor);
                auto filter_val_str = lc_filter.apply_visitor(stringify_visitor);
                _csv_file << filter_val_str << ',';
            }

            _csv_file << expected_apy << ',' << best_results.num_loans << ',' << num_defaulted << ','
               << pct_defaulted << ',' << avg_default_loss << ',' << net_apy << '\n';
            std::flush(_csv_file);
            _best_net_apy = net_apy;
        }

        std::string filters = "";
        for (auto& lc_filter : best_filters) {
            auto filter_name = lc_filter.apply_visitor(name_visitor);;
            auto filter_val_str = lc_filter.apply_visitor(stringify_visitor);
            filters += filter_name + " is " + filter_val_str + ',';
        }

        std::cout << "Best Filter: " << filters << '\n';
        std::cout << "[iteration " << (_iteration + 1) << '/' << _iterations << ' ' << std::setprecision(4) << _iteration_time.count() / (_iteration + 1);
        std::cout << " sec/iter] Matched " << best_results.num_loans << '/' << _lcbt.total_loans() << " loans ";
        std::cout << "(" << loans_per_month << "/mo.) test at " << std::setprecision(4) << expected_apy << "% APY. ";
        std::cout << std::setprecision(4) << num_defaulted << " loans defaulted (" << pct_defaulted << "%, $";
        std::cout << avg_default_loss << " avg loss) " << net_apy << "% net APY\n";
    }

    void copy_population(const PopulationType& from_population, PopulationType& to_population)
    {
        static filter_set_current_visitor set_current_visitor;
        static filter_get_current_visitor get_current_visitor;

        unsigned i = 0;
        for (auto& lc_pair : from_population) {
            unsigned j = 0;
            for (auto& lc_filter : lc_pair.second) {
                set_current_visitor.set(lc_filter.apply_visitor(get_current_visitor));
                to_population[i].second[j].apply_visitor(set_current_visitor);
                ++j;
            }
            ++i;
        }
    }

    void mate()
    {
        // Save the elite
        auto num_elite = boost::numeric_cast<unsigned>(_args["elite_rate"].as<double>() * _population.size());
        auto mate_size = boost::numeric_cast<unsigned>(std::floor(_population.size() / 5.0));
        auto mutation_possibility = boost::numeric_cast<unsigned>(1.0 / _args["mutation_rate"].as<double>());
        copy_population(_population, _mate_population);

        static filter_set_current_visitor set_current_visitor;
        static filter_get_current_visitor get_current_visitor;
        static filter_count_visitor count_visitor;
        static filter_value_visitor value_visitor;

        std::hash<lc::FilterValue> filter_hash;

        for (size_t i = num_elite; i < _population.size(); ++i) {

            unsigned hash_result = 0;

            do {
                auto& lc_filters = _mate_population[i].second;

                hash_result = 0;

                for (size_t j = 0, size = lc_filters.size(); j < size; ++j) {
                    // Mate with 20 % of population
                    auto partner = randint(0, mate_size);

                    auto& lc_filter = lc_filters[j];
                    set_current_visitor.set(_population[partner].second[j].apply_visitor(get_current_visitor));
                    lc_filter.apply_visitor(set_current_visitor);

                    // Mutate! Once in a blue moon
                    if (!randint(0, mutation_possibility)) {
                        set_current_visitor.set(randint(0, lc_filter.apply_visitor(count_visitor) - 1));
                        lc_filter.apply_visitor(set_current_visitor);
                    }

                    hash_result = hash_result ^ (filter_hash(lc_filter.apply_visitor(value_visitor)) << 1);
                }
            // Keep randomizing until we find a filter set we haven't used before
            } while (_memoized_filters.find(hash_result) != _memoized_filters.end());

            ++_memoized_filters[hash_result];
        }

        copy_population(_mate_population, _population);
    }

    LCBT&                                                       _lcbt;
    const Arguments&						                    _args;
    std::vector<VariantFilter>					                _filters;
    PopulationType                                              _population;
    PopulationType                                              _mate_population;
    unsigned                                                    _iteration;
    unsigned                                                    _iterations;
    std::chrono::duration<double>                               _iteration_time;
    std::ofstream                                               _csv_file;
    double                                                      _best_net_apy;
    std::map<unsigned, unsigned>                                _memoized_filters;
};

};

#endif // __LC_LCGA_HPP__
