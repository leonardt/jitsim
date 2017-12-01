#ifndef JITSIM_PRIMITIVE_HPP_INCLUDED
#define JITSIM_PRIMITIVE_HPP_INCLUDED

#include <functional>
#include <llvm/IR/Value.h>
#include <llvm/IR/Function.h>

namespace JITSim {

struct Primitive {
public:
  bool is_stateful;
  std::function<llvm::Value *(FunctionEnvironment &env, const std::vector<Value *> &)> make_compute_output;
  std::function<void (FunctionEnvironment &env, const std::vector<Value *> &)> make_update_state;
  std::function<void (ModuleEnvironment &env)> make_def;
  Primitive(bool is_stateful_,
            std::function<llvm::Value *()> make_inst_,
            std::function<llvm::Function *()> make_def_)
    : is_stateful(is_stateful_), make_inst(make_inst_), make_def(make_def_)
  {}
};

}

#endif
