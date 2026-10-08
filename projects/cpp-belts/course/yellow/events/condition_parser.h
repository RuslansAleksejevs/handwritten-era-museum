#pragma once
#include "node.h"
#include <istream>
namespace yellow::events {
std::shared_ptr<Node> ParseCondition(std::istream &);
}
