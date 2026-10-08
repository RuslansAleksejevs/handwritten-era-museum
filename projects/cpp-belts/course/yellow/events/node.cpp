#include "node.h"
#include <stdexcept>
namespace yellow::events {
template <class T> bool Compare(Comparison c, const T &a, const T &b) {
    switch (c) {
    case Comparison::Less:
        return a < b;
    case Comparison::LessOrEqual:
        return !(b < a);
    case Comparison::Greater:
        return b < a;
    case Comparison::GreaterOrEqual:
        return !(a < b);
    case Comparison::Equal:
        return a == b;
    case Comparison::NotEqual:
        return !(a == b);
    }
    throw std::logic_error("unknown comparison");
}
bool DateComparisonNode::Evaluate(const Date &d, const std::string &) const {
    return Compare(comparison_, d, date_);
}
bool EventComparisonNode::Evaluate(const Date &, const std::string &e) const {
    return Compare(comparison_, e, event_);
}
LogicalOperationNode::LogicalOperationNode(LogicalOperation op, std::shared_ptr<Node> l,
                                           std::shared_ptr<Node> r)
    : operation_(op), left_(std::move(l)), right_(std::move(r)) {
    if (!left_ || !right_)
        throw std::invalid_argument("null condition operand");
}
bool LogicalOperationNode::Evaluate(const Date &d, const std::string &e) const {
    if (operation_ == LogicalOperation::And)
        return left_->Evaluate(d, e) && right_->Evaluate(d, e);
    return left_->Evaluate(d, e) || right_->Evaluate(d, e);
}
} // namespace yellow::events
