#pragma once

namespace nc {

class BaseVisitor;

class BaseNode {
public:
  virtual ~BaseNode() = default;
  virtual void accept(BaseVisitor &visitor) = 0;
};

class BaseVisitor {
public:
  virtual void visit(BaseNode &node) = 0;
};
} // namespace nc