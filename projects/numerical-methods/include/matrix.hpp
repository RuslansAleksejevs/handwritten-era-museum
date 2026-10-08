#pragma once

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace preai {
class Matrix {
    std::size_t rows_, cols_;
    std::vector<double> data_;
public:
    Matrix(std::size_t rows, std::size_t cols) : rows_(rows), cols_(cols) {
        if (!rows || !cols || rows > std::numeric_limits<std::size_t>::max() / cols)
            throw std::invalid_argument("invalid matrix dimensions");
        data_.resize(rows * cols);
    }
    std::size_t rows() const noexcept { return rows_; }
    std::size_t cols() const noexcept { return cols_; }
    double& operator()(std::size_t r, std::size_t c) {
        if (r>=rows_ || c>=cols_) throw std::out_of_range("matrix index");
        return data_[r * cols_ + c];
    }
    double operator()(std::size_t r, std::size_t c) const {
        if (r>=rows_ || c>=cols_) throw std::out_of_range("matrix index");
        return data_[r * cols_ + c];
    }
    static Matrix identity(std::size_t n) {
        Matrix out(n,n);
        for (std::size_t i=0;i<n;++i) out(i,i)=1;
        return out;
    }
    void swap_columns(std::size_t a, std::size_t b) {
        for (std::size_t r=0;r<rows_;++r) std::swap((*this)(r,a),(*this)(r,b));
    }
};

inline Matrix multiply(const Matrix& a, const Matrix& b) {
    if (a.cols()!=b.rows()) throw std::invalid_argument("incompatible matrix shapes");
    Matrix c(a.rows(),b.cols());
    for (std::size_t i=0;i<a.rows();++i)
        for (std::size_t k=0;k<a.cols();++k)
            for (std::size_t j=0;j<b.cols();++j) c(i,j)+=a(i,k)*b(k,j);
    return c;
}

inline double infinity_norm(const Matrix& a) {
    double norm=0;
    for (std::size_t i=0;i<a.rows();++i) {
        double row=0;
        for (std::size_t j=0;j<a.cols();++j) row+=std::abs(a(i,j));
        if (!std::isfinite(row)) throw std::overflow_error("norm or residual exceeds double range");
        norm=std::max(norm,row);
    }
    return norm;
}

// Evaluate residual / (norm_a * norm_inverse) without an overflowing product
// or an underflowing intermediate quotient. Only the final result may underflow.
inline double normalized_residual(double residual, double norm_a, double norm_inverse) {
    if (!std::isfinite(residual) || residual < 0 || !std::isfinite(norm_a) ||
        norm_a <= 0 || !std::isfinite(norm_inverse) || norm_inverse <= 0)
        throw std::invalid_argument("finite residual and positive finite norms required");
    int er=0, ea=0, ei=0;
    const double r=std::frexp(residual,&er);
    const double a=std::frexp(norm_a,&ea);
    const double inv=std::frexp(norm_inverse,&ei);
    return std::ldexp(r/a/inv,er-ea-ei);
}

inline double inverse_residual(const Matrix& a, const Matrix& inv) {
    Matrix p=multiply(a,inv);
    if (p.rows()!=p.cols()) throw std::invalid_argument("residual requires square product");
    for (std::size_t i=0;i<p.rows();++i) p(i,i)-=1;
    return infinity_norm(p);
}
} // namespace preai
