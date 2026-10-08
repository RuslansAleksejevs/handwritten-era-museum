#include "matrix_input.hpp"
#include "solver.hpp"
#include <fstream>
#include <iomanip>
#include <iostream>
#include <string>

int main(int argc,char** argv) {
    try {
        if (argc<2 || argc>3) {
            std::cerr<<"Usage: inverse MATRIX.txt [THREADS]\nFormat: n followed by n*n finite numbers.\n";
            return 2;
        }
        std::size_t threads=1;
        if (argc==3) {
            const std::string arg=argv[2]; std::size_t used=0;
            if (arg.empty() || arg[0]=='-') throw std::invalid_argument("invalid threads");
            threads=std::stoul(arg,&used);
            if (used!=arg.size()) throw std::invalid_argument("invalid threads");
        }
        std::ifstream file(argv[1]);
        if (!file) throw std::invalid_argument("cannot open matrix file");
        const auto a=preai::read_matrix(file);
        const auto n=a.rows();
        auto result=preai::inverse(a,threads);
        std::cout<<std::setprecision(17);
        for (std::size_t i=0;i<n;++i) {
            for (std::size_t j=0;j<n;++j) std::cout<<(j?" ":"")<<result.inverse(i,j);
            std::cout<<'\n';
        }
        std::cerr<<"absolute_residual_inf="<<result.residual
                 <<" relative_residual_inf="<<result.relative_residual<<'\n';
        return 0;
    } catch (const std::exception& e) { std::cerr<<"error: "<<e.what()<<'\n'; return 1; }
}
