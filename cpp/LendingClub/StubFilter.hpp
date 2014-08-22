/*
Created on July 28, 2014

@author:     Gregory Czajkowski

@copyright:  2013 Freedom. All rights reserved.

@license:    Licensed under the Apache License 2.0 http://www.apache.org/licenses/LICENSE-2.0

@contact:    gregczajkowski at yahoo.com
*/

#ifndef __LC_STUBFILTER_HPP__
#define __LC_STUBFILTER_HPP__

#include "Filter.hpp"
#include "Loan.hpp"
#include "Utilities.hpp"

namespace lc {

class StubFilter : public Filter 
{
public:
    //static const std::string sqlite_type;
    //static const std::string name;

    StubFilter(unsigned* current = nullptr) :
        Filter("Stub") 
    {
    }

    static bool static_apply(const Filter& self, const LCLoan& loan) 
    {
        return true;
    }
    inline bool apply(const LCLoan& loan)  const
    {
        return true;
    }
};

};

#endif // __LC_STUBFILTER_HPP__
