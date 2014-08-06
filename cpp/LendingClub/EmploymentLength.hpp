/*
Created on July 28, 2014

@author:     Gregory Czajkowski

@copyright:  2013 Freedom. All rights reserved.

@license:    Licensed under the Apache License 2.0 http://www.apache.org/licenses/LICENSE-2.0

@contact:    gregczajkowski at yahoo.com
*/

#ifndef __LC_EMPLOYMENT_LENGTH_HPP__
#define __LC_EMPLOYMENT_LENGTH_HPP__

#include "Filter.hpp"
#include "Loan.hpp"
#include "Utilities.hpp"

namespace lc
{

class EmploymentLength : public Filter<EmploymentLength>
{
public:
    static const std::string sqlite_type;
    static const std::string csv_name;
    static const std::string name;

    EmploymentLength(const Arguments& args, unsigned* current = nullptr) : Filter<EmploymentLength>(name, args)
    {
        static const std::vector<FilterValue>* options = create_range(0, 12, 1);
        Filter::initialize(options, current);
    }

    inline FilterValue convert(const std::string& raw_data)
    {
        if (raw_data == "n/a") {
            return 0;
        }
        else if (raw_data == "< 1 year") {
            return 1;
        }
        else if (raw_data == "10+ years") {
            return 11;
        }
        else {
            unsigned result = (raw_data[0] - '0') + 1;
            return result;            
        }
    }

    const std::string get_string_value() const
    {
        auto value = static_cast<unsigned>(get_value());
        if (value == 0) {
            return "< 1 year";
        }
        else if (value == 11) {
            return "10+ years";
        }
        else if (value == 1) {
            return "1 year";
        }
        else {
            return std::string('0' + value, 1) + " years";
        }
    }

    static bool static_apply(const Filter& self, const LCLoan& loan)
    {
        return (loan.emp_length <= self.get_value());
    }

    inline bool apply(const LCLoan& loan) const
    {
        return (loan.emp_length <= get_value());
    }
};

};

#endif // __LC_EMPLOYMENT_LENGTH_HPP__