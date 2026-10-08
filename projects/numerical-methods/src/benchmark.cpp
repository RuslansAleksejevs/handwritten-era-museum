#include "solver.hpp"
#include <chrono>
#include <iomanip>
#include <iostream>
#include <random>

int main() {
    std::cout<<"size,threads,trial,seconds,residual_inf\n"<<std::setprecision(10);
    for (std::size_t n:{16,64,128}) {
        std::mt19937 rng(2019);std::uniform_real_distribution<double> u(-1,1);
        preai::Matrix a(n,n);
        for(std::size_t i=0;i<n;++i)for(std::size_t j=0;j<n;++j)a(i,j)=u(rng)+(i==j?n:0);
        for(std::size_t threads:{1,2,4})for(int trial=0;trial<3;++trial){
            const auto start=std::chrono::steady_clock::now();
            auto result=preai::inverse(a,threads);
            const double seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();
            std::cout<<n<<','<<threads<<','<<trial<<','<<seconds<<','<<result.residual<<'\n';
        }
    }
}
