#pragma once
#include "date.h"
#include <memory>
#include <string>
namespace yellow::events {
enum class Comparison { Less, LessOrEqual, Greater, GreaterOrEqual, Equal, NotEqual };
enum class LogicalOperation { Or, And };
class Node {
  public:
    virtual ~Node() = default;
    virtual bool Evaluate(const Date &, const std::string &) const = 0;
};
class EmptyNode : public Node {
  public:
    bool Evaluate(const Date &, const std::string &) const override { return true; }
};
class DateComparisonNode : public Node {
    Comparison comparison_;
    Date date_;

  public:
    DateComparisonNode(Comparison c, Date d) : comparison_(c), date_(d) {}
    bool Evaluate(const Date &, const std::string &) const override;
};
class EventComparisonNode : public Node {
    Comparison comparison_;
    std::string event_;

  public:
    EventComparisonNode(Comparison c, std::string e) : comparison_(c), event_(std::move(e)) {}
    bool Evaluate(const Date &, const std::string &) const override;
};
class LogicalOperationNode : public Node {
    LogicalOperation operation_;
    std::shared_ptr<Node> left_, right_;

  public:
    LogicalOperationNode(LogicalOperation, std::shared_ptr<Node>, std::shared_ptr<Node>);
    bool Evaluate(const Date &, const std::string &) const override;
};
} // namespace yellow::events
