#include "solver.hpp"
#include <iostream>
#include <random>
#include <stdexcept>

void require(bool ok,const char* message) { if (!ok) throw std::runtime_error(message); }
template<class F> void must_fail(F f) {
    bool failed=false;
    try { f(); } catch (const std::domain_error&) { failed=true; }
    require(failed,"singular matrix should be rejected");
}
int main() {
    using namespace preai;
    require(std::abs(normalized_residual(1e-16,1e308,1e-308)/1e-16-1)<1e-14,
            "normalization must not underflow before dividing by the inverse norm");
    require(std::abs(normalized_residual(1e308,1e308,1e308)/1e-308-1)<1e-14,
            "normalization must not overflow the product of norms");
    std::mt19937 rng(2019); std::uniform_real_distribution<double> u(-1,1);
    for (auto threads:{1u,2u,4u}) {
        Matrix swap(2,2); swap(0,1)=swap(1,0)=1;
        auto s=inverse(swap,threads);
        require(s.inverse(0,1)==1 && s.inverse(1,0)==1,"column pivot regression");
        Matrix singular(2,2); singular(0,0)=1; singular(0,1)=2; singular(1,0)=2; singular(1,1)=4;
        must_fail([&]{inverse(singular,threads);});
        must_fail([&]{inverse(Matrix(3,3),threads);});
        for (auto n:{1u,2u,3u,7u,16u,31u}) for (int trial=0;trial<12;++trial) {
            Matrix a(n,n);
            for (unsigned i=0;i<n;++i) for (unsigned j=0;j<n;++j) a(i,j)=u(rng)+(i==j?n:0);
            auto out=inverse(a,threads);
            require(out.residual<1e-12,"right inverse residual too large");
            require(inverse_residual(out.inverse,a)<1e-12,"left inverse residual too large");
        }
        for (auto scale:{1e-150,1e150}) {
            Matrix a(2,2); a(0,0)=2*scale; a(0,1)=scale; a(1,0)=scale; a(1,1)=2*scale;
            auto out=inverse(a,threads);
            require(std::abs(out.inverse(0,0)*scale-2.0/3)<1e-14,"scaling regression");
        }
        Matrix huge(2,2);
        huge(0,0)=huge(1,1)=5e307; huge(0,1)=huge(1,0)=2.5e307;
        const auto out=inverse(huge,threads);
        // This particular matrix has condition number 3 in the infinity norm.
        require(out.residual>0 && out.relative_residual>0 &&
                std::abs(out.relative_residual/(out.residual/3)-1)<1e-13,
                "large-scale relative residual must retain significant digits");
    }
    Matrix a(2,3),b(3,4); a(0,1)=2;b(1,3)=3;
    require(multiply(a,b)(0,3)==6,"rectangular multiplication");
    bool shape=false;
    try {inverse(a);} catch(const std::invalid_argument&) {shape=true;}
    require(shape,"nonsquare input accepted");
    ColumnWorkers pool(3);
    bool propagated=false;
    try { pool.run(8,[](std::size_t j){if(j==2)throw std::runtime_error("worker failure");}); }
    catch(const std::runtime_error&) { propagated=true; }
    require(propagated,"worker failure lost");
    pool.run(8,[](std::size_t){});
    std::cout<<"PASS: pivots, singular termination, 216 seeded systems, scaling, rectangular arithmetic, worker recovery\n";
}
