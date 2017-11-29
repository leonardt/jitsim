#include <jitsim/JIT.hpp>

namespace JITSim {

using namespace llvm;
using namespace llvm::orc;

JIT::JIT()
: TM(EngineBuilder().selectTarget()), DL(TM->createDataLayout()),
  ObjectLayer([]() { return std::make_shared<SectionMemoryManager>(); }),
  CompileLayer(ObjectLayer, SimpleCompiler(*TM))
{
  llvm::sys::DynamicLibrary::LoadLibraryPermanently(nullptr);
}

TargetMachine &JIT::getTargetMachine() { return *TM; }

JIT::ModuleHandle JIT::addModule(std::unique_ptr<Module> M) {
  // Build our symbol resolver:
  // Lambda 1: Look back into the JIT itself to find symbols that are part of
  //           the same "logical dylib".
  // Lambda 2: Search for external symbols in the host process.
  auto Resolver = createLambdaResolver(
    [&](const std::string &Name) {
      if (auto Sym = CompileLayer.findSymbol(Name, false))
        return Sym;
      return JITSymbol(nullptr);
    },  
    [](const std::string &Name) {
      if (auto SymAddr = RTDyldMemoryManager::getSymbolAddressInProcess(Name))
        return JITSymbol(SymAddr, JITSymbolFlags::Exported);
      return JITSymbol(nullptr);
    }); 

  // Add the set to the JIT with the resolver we created above and a newly
  // created SectionMemoryManager.
  return cantFail(CompileLayer.addModule(std::move(M), std::move(Resolver)));
}

JITSymbol JIT::findSymbol(const std::string Name) {
  std::string MangledName;
  raw_string_ostream MangledNameStream(MangledName);
  Mangler::getNameWithPrefix(MangledNameStream, Name, DL);
  return CompileLayer.findSymbol(MangledNameStream.str(), true);
}   

JITTargetAddress JIT::getSymbolAddress(const std::string Name) {
  return cantFail(findSymbol(Name).getAddress());
}  

void JIT::removeModule(ModuleHandle H) {
  cantFail(CompileLayer.removeModule(H));
}

}