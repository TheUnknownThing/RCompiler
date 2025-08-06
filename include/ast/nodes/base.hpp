#pragma once

namespace nc {
class BaseNode {
public:
  virtual ~BaseNode() = default;
  virtual void accept(class Visitor &visitor) = 0;
};

class BaseVisitor {
public:
  virtual void visit(BaseNode &node) = 0;
};
} // namespace nc