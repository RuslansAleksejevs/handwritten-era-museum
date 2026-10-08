#pragma once

#include "matrix.hpp"
#include <cerrno>
#include <cstdlib>
#include <istream>
#include <stdexcept>
#include <string>

namespace preai {
// The text format shared by the serial and MPI programs: a decimal dimension n
// in [1,4096], n*n finite numbers, and nothing else. Whole tokens are parsed,
// so "2.5" is rejected instead of becoming n=2 followed by an entry ".5".
inline std::size_t read_dimension(std::istream& in) {
    std::string token;
    std::size_t n=0;
    if (!(in>>token) || token.find_first_not_of("0123456789")!=std::string::npos)
        throw std::invalid_argument("n must be in [1,4096]");
    for (const char digit:token) {
        n=n*10+static_cast<std::size_t>(digit-'0');
        if (n>4096) throw std::invalid_argument("n must be in [1,4096]");
    }
    if (!n) throw std::invalid_argument("n must be in [1,4096]");
    return n;
}

// strtod accepts representable subnormals, which some stream parsers reject.
inline double read_entry(std::istream& in) {
    std::string token;
    if (!(in>>token)) throw std::invalid_argument("missing or invalid matrix entry");
    char* end=nullptr;
    errno=0;
    const double value=std::strtod(token.c_str(),&end);
    if (end==token.c_str() || end!=token.c_str()+token.size())
        throw std::invalid_argument("invalid matrix entry");
    if (!std::isfinite(value)) throw std::invalid_argument("matrix must be finite");
    if (errno==ERANGE && value==0) throw std::out_of_range("matrix entry underflows double range");
    return value;
}

inline Matrix read_matrix(std::istream& in) {
    const auto n=read_dimension(in);
    Matrix a(n,n);
    for (std::size_t i=0;i<n;++i) for (std::size_t j=0;j<n;++j) a(i,j)=read_entry(in);
    std::string extra;
    if (in>>extra) throw std::invalid_argument("unexpected trailing input");
    if (in.bad()) throw std::runtime_error("matrix input read failed");
    return a;
}
} // namespace preai
