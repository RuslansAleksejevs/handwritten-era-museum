#pragma once
#include "data.hpp"
#include "unique_ptr.hpp"
#include <limits>
namespace museum::brown {
namespace RAII {
template <class Provider> class Booking {
    Provider *provider_ = nullptr;
    int id_ = 0;
    void Finish() noexcept {
        if (provider_) {
            provider_->CancelOrComplete(*this);
            provider_ = nullptr;
        }
    }

  public:
    Booking(Provider *provider, int id) : provider_(provider), id_(id) {}
    Booking(const Booking &) = delete;
    Booking &operator=(const Booking &) = delete;
    Booking(Booking &&other) noexcept : provider_(other.provider_), id_(other.id_) {
        other.provider_ = nullptr;
    }
    Booking &operator=(Booking &&other) noexcept {
        if (this != &other) {
            Finish();
            provider_ = other.provider_;
            id_ = other.id_;
            other.provider_ = nullptr;
        }
        return *this;
    }
    ~Booking() { Finish(); }
    int GetId() const { return id_; }
};
} // namespace RAII
class Animal {
  public:
    virtual ~Animal() = default;
    virtual std::string Voice() const { return "Not implemented yet"; }
};
class Tiger : public Animal {
  public:
    std::string Voice() const override { return "Rrrr"; }
};
class Wolf : public Animal {
  public:
    std::string Voice() const override { return "Wooo"; }
};
class Fox : public Animal {
  public:
    std::string Voice() const override { return "Tyaf"; }
};
using Zoo = std::vector<std::unique_ptr<Animal>>;
inline Zoo CreateZoo(std::istream &in) {
    Zoo zoo;
    for (std::string word; in >> word;) {
        if (word == "Tiger")
            zoo.push_back(std::make_unique<Tiger>());
        else if (word == "Wolf")
            zoo.push_back(std::make_unique<Wolf>());
        else if (word == "Fox")
            zoo.push_back(std::make_unique<Fox>());
        else
            throw std::runtime_error("unknown animal");
    }
    return zoo;
}
inline void Process(const Zoo &zoo, std::ostream &out) {
    for (const auto &animal : zoo)
        out << animal->Voice() << '\n';
}
class Expression {
  public:
    virtual ~Expression() = default;
    virtual int Evaluate() const = 0;
    virtual std::string ToString() const = 0;
};
using ExpressionPtr = std::unique_ptr<Expression>;
class Constant final : public Expression {
    int value_;

  public:
    explicit Constant(int value) : value_(value) {}
    int Evaluate() const override { return value_; }
    std::string ToString() const override { return std::to_string(value_); }
};
class BinaryExpression final : public Expression {
    ExpressionPtr left_, right_;
    char op_;

  public:
    BinaryExpression(ExpressionPtr left, ExpressionPtr right, char op)
        : left_(std::move(left)), right_(std::move(right)), op_(op) {
        if (!left_ || !right_)
            throw std::invalid_argument("empty expression");
    }
    int Evaluate() const override {
        const long long a = left_->Evaluate(), b = right_->Evaluate();
        const long long value = op_ == '+' ? a + b : a * b;
        if (value < std::numeric_limits<int>::min() || value > std::numeric_limits<int>::max())
            throw std::overflow_error("expression outside int range");
        return static_cast<int>(value);
    }
    std::string ToString() const override {
        return "(" + left_->ToString() + ")" + op_ + "(" + right_->ToString() + ")";
    }
};
inline ExpressionPtr Value(int value) { return std::make_unique<Constant>(value); }
inline ExpressionPtr Sum(ExpressionPtr a, ExpressionPtr b) {
    return std::make_unique<BinaryExpression>(std::move(a), std::move(b), '+');
}
inline ExpressionPtr Product(ExpressionPtr a, ExpressionPtr b) {
    return std::make_unique<BinaryExpression>(std::move(a), std::move(b), '*');
}
struct Email {
    std::string from, to, body;
};
class Worker {
    std::unique_ptr<Worker> next_;

  protected:
    void PassOn(std::unique_ptr<Email> email) const {
        if (next_)
            next_->Process(std::move(email));
    }

  public:
    virtual ~Worker() = default;
    virtual void Process(std::unique_ptr<Email> email) = 0;
    virtual void Run() { throw std::logic_error("only Reader can run"); }
    void SetNext(std::unique_ptr<Worker> next) { next_ = std::move(next); }
};
class Reader : public Worker {
    std::istream &input_;

  public:
    explicit Reader(std::istream &input) : input_(input) {}
    void Process(std::unique_ptr<Email> email) override { PassOn(std::move(email)); }
    void Run() override {
        for (;;) {
            auto email = std::make_unique<Email>();
            if (!std::getline(input_, email->from))
                break;
            if (!std::getline(input_, email->to) || !std::getline(input_, email->body))
                throw std::invalid_argument("incomplete email");
            PassOn(std::move(email));
        }
    }
};
class Filter : public Worker {
  public:
    using Function = std::function<bool(const Email &)>;

  private:
    Function predicate_;

  public:
    explicit Filter(Function f) : predicate_(std::move(f)) {}
    void Process(std::unique_ptr<Email> email) override {
        if (predicate_(*email))
            PassOn(std::move(email));
    }
};
class Copier : public Worker {
    std::string to_;

  public:
    explicit Copier(std::string to) : to_(std::move(to)) {}
    void Process(std::unique_ptr<Email> email) override {
        std::unique_ptr<Email> copy;
        if (email->to != to_) {
            copy = std::make_unique<Email>(*email);
            copy->to = to_;
        }
        PassOn(std::move(email));
        if (copy)
            PassOn(std::move(copy));
    }
};
class Sender : public Worker {
    std::ostream &out_;

  public:
    explicit Sender(std::ostream &out) : out_(out) {}
    void Process(std::unique_ptr<Email> email) override {
        out_ << email->from << '\n' << email->to << '\n' << email->body << '\n';
        if (!out_)
            throw std::runtime_error("email output failed");
        PassOn(std::move(email));
    }
};
class PipelineBuilder {
    std::unique_ptr<Worker> head_;
    Worker *tail_;
    void Append(std::unique_ptr<Worker> next) {
        auto *p = next.get();
        tail_->SetNext(std::move(next));
        tail_ = p;
    }

  public:
    explicit PipelineBuilder(std::istream &in)
        : head_(std::make_unique<Reader>(in)), tail_(head_.get()) {}
    PipelineBuilder &FilterBy(Filter::Function f) {
        Append(std::make_unique<Filter>(std::move(f)));
        return *this;
    }
    PipelineBuilder &CopyTo(std::string to) {
        Append(std::make_unique<Copier>(std::move(to)));
        return *this;
    }
    PipelineBuilder &Send(std::ostream &out) {
        Append(std::make_unique<Sender>(out));
        return *this;
    }
    std::unique_ptr<Worker> Build() { return std::move(head_); }
};
struct IBook {
    virtual ~IBook() = default;
    virtual const std::string &GetName() const = 0;
    virtual const std::string &GetContent() const = 0;
};
using BookPtr = std::shared_ptr<const IBook>;
struct IBooksUnpacker {
    virtual ~IBooksUnpacker() = default;
    virtual BookPtr UnpackBook(const std::string &name) = 0;
};
struct ICache {
    struct Settings {
        std::size_t max_memory;
    };
    virtual ~ICache() = default;
    virtual BookPtr GetBook(const std::string &name) = 0;
};
// Unpacking is serialized under the same mutex: correctness and one unpack per hit race,
// with no claim of parallel decompression throughput.
class LruCache final : public ICache {
    struct Entry {
        BookPtr book;
        std::list<std::string>::iterator order;
    };
    std::shared_ptr<IBooksUnpacker> unpacker_;
    std::size_t limit_, used_ = 0;
    std::mutex mutex_;
    std::list<std::string> order_;
    std::unordered_map<std::string, Entry> books_;

  public:
    LruCache(std::shared_ptr<IBooksUnpacker> unpacker, Settings settings)
        : unpacker_(std::move(unpacker)), limit_(settings.max_memory) {
        if (!unpacker_)
            throw std::invalid_argument("missing unpacker");
    }
    BookPtr GetBook(const std::string &name) override {
        std::lock_guard<std::mutex> lock(mutex_);
        if (auto it = books_.find(name); it != books_.end()) {
            order_.splice(order_.end(), order_, it->second.order);
            return it->second.book;
        }
        auto book = unpacker_->UnpackBook(name);
        if (!book)
            throw std::runtime_error("unpacker returned null");
        const auto size = book->GetContent().size();
        if (size > limit_) {
            books_.clear();
            order_.clear();
            used_ = 0;
            return book;
        }
        while (used_ > limit_ - size) {
            auto it = books_.find(order_.front());
            used_ -= it->second.book->GetContent().size();
            books_.erase(it);
            order_.pop_front();
        }
        order_.push_back(name);
        try {
            books_.emplace(name, Entry{book, std::prev(order_.end())});
        } catch (...) {
            order_.pop_back();
            throw;
        }
        used_ += size;
        return book;
    }
};
inline std::unique_ptr<ICache> MakeCache(std::shared_ptr<IBooksUnpacker> unpacker,
                                         ICache::Settings settings) {
    return std::make_unique<LruCache>(std::move(unpacker), settings);
}
} // namespace museum::brown
