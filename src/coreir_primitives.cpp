#include "coreir_primitives.hpp"

#include <coreir/ir/namespace.h>
#include <coreir/ir/value.h>

#include <jitsim/circuit.hpp>

namespace JITSim {

using namespace std;

int getNumBytes(int bits) {
  if (bits % 8 == 0) {
    return bits / 8;
  } else {
    return bits / 8 + 1;
  }
}

Primitive BuildReg(CoreIR::Module *mod)
{
  int width = 0;
  for (const auto & val : mod->getGenArgs()) {
    if (val.first == "width") {
      width = val.second->get<int>();
    }
  }

  return Primitive(true, getNumBytes(width),
    { "in" }, {},
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[0], llvm::Type::getIntNPtrTy(env.getContext(), width));
      llvm::Value *output = env.getIRBuilder().CreateLoad(addr, "output");

      return std::vector<llvm::Value *> { output };
    },
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *input = args[0];
      llvm::Value *addr = env.getIRBuilder().CreateBitCast(args[1], llvm::Type::getIntNPtrTy(env.getContext(), width));
      env.getIRBuilder().CreateStore(input, addr);
    }
  );
}

Primitive BuildAdd(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sum = env.getIRBuilder().CreateAdd(lhs, rhs, "sum");
      
      return std::vector<llvm::Value *> { sum };
    }
  );
}

Primitive BuildMul(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *prod = env.getIRBuilder().CreateMul(lhs, rhs, "prod");

      return std::vector<llvm::Value *> { prod };
    }
  );
}      

Primitive BuildEq(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpEQ(lhs, rhs, "eq_comp");

      return std::vector<llvm::Value *> { comp };
    }
  );
}      
      
Primitive BuildNeq(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *comp = env.getIRBuilder().CreateICmpNE(lhs, rhs, "neq_comp");
      return std::vector<llvm::Value *> { comp };
    }
  );
}      

Primitive BuildMux(CoreIR::Module *mod)
{
  return Primitive(
    [](auto &env, auto &args, auto &inst)
    {
      llvm::Value *lhs = args[0];
      llvm::Value *rhs = args[1];
      llvm::Value *sel = args[2];

      llvm::Value *if_cond =
        env.getIRBuilder().CreateICmpEQ(sel,
                                        llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 0)),
                                        "ifcond");

      llvm::Value *result =
        env.getIRBuilder().CreateSelect(if_cond, lhs, rhs, "result");

      return std::vector<llvm::Value *> { result };
    }
  );
}      
      
Primitive BuildMem(CoreIR::Module *mod)
{
  int width = 0; 
  int depth = 0;

  for (const auto & val : mod->getGenArgs()) {
    if (val.first == "width") {
      width = val.second->get<int>();
    } else if (val.first == "depth") {
      depth = val.second->get<int>();
    }
  }

  return Primitive(true, getNumBytes(width*depth),
    { "waddr", "wdata", "wen" }, { "raddr" },
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *raddr = args[0];
      llvm::Value *state_addr = args[1];

      llvm::Value *cast_addr = 
        env.getIRBuilder().CreateBitCast(state_addr,
                                         llvm::Type::getIntNPtrTy(env.getContext(), width));
      llvm::Value *addr = env.getIRBuilder().CreateGEP(cast_addr, raddr, "addr");
      llvm::Value *rdata = env.getIRBuilder().CreateLoad(addr, "rdata");
      
      return std::vector<llvm::Value *> { rdata };
    },
    [width](auto &env, auto &args, auto &inst)
    {
      llvm::Value *waddr = args[0];
      llvm::Value *wdata = args[1];
      llvm::Value *wen = args[2];
      llvm::Value *state_addr = args[3];

      llvm::Value *cast_addr = 
        env.getIRBuilder().CreateBitCast(state_addr,
                                         llvm::Type::getIntNPtrTy(env.getContext(), width));
      llvm::Value *addr = env.getIRBuilder().CreateGEP(cast_addr, waddr, "addr");

      llvm::Value *if_cond =
        env.getIRBuilder().CreateICmpEQ(wen,
                                        llvm::ConstantInt::get(env.getContext(), llvm::APInt(1, 1)),
                                        "ifcond");

      llvm::BasicBlock *then_bb = env.addBasicBlock("then", false);
      llvm::BasicBlock *else_bb = env.addBasicBlock("else", false);

      env.getIRBuilder().CreateCondBr(if_cond, then_bb, else_bb);

      // Emit then block.
      env.getIRBuilder().SetInsertPoint(then_bb);
      env.getIRBuilder().CreateStore(wdata, addr);
      env.getIRBuilder().CreateBr(else_bb);

      env.getIRBuilder().SetInsertPoint(else_bb);
    }
  );
}      
      
static unordered_map<string,function<Primitive (CoreIR::Module *mod)>> InitializeMapping()
{
  unordered_map<string,function<Primitive (CoreIR::Module *mod)>> m;
  m["coreir.reg"] = BuildReg;
  m["coreir.add"] = BuildAdd;
  m["coreir.mul"] = BuildMul;
  m["coreir.eq"] = BuildEq;
  m["coreir.neq"] = BuildNeq;
  m["coreir.mux"] = BuildMux;
  m["coreir.mem"] = BuildMem;

  return m;
}

Primitive BuildCoreIRPrimitive(CoreIR::Module *mod)
{
  static const unordered_map<string,function<Primitive (CoreIR::Module *mod)>> prim_map =
    InitializeMapping();
  string fullname = mod->getNamespace()->getName() + "." + mod->getName();

  if (prim_map.count(fullname) == 0) {
    cerr << "Unsupported primitive " << fullname << endl;
    assert(false);
  }

  auto iter = prim_map.find(fullname);
  assert(iter != prim_map.end());

  return iter->second(mod);
}

}
