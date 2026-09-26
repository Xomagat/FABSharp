#include "Compile.h"

void compile(std::vector<std::unique_ptr<Statement>>& statements, std::string name)
{
    llvm::LLVMContext context;
    llvm::Module module("fabsharp_module", context);
    llvm::IRBuilder<> builder(context);

    CodegenContext ctx{context, module, builder};

    bool has_user_main = false;
    for (auto& stmt : statements)
        if (auto fn = dynamic_cast<FunctionDefineStatement*>(stmt.get()))
            if (fn->get_name() == "main")
                has_user_main = true;

    if (!has_user_main)
    {
        auto mainType = llvm::FunctionType::get(builder.getInt32Ty(), false);
        auto mainFunc = llvm::Function::Create(
            mainType, llvm::Function::ExternalLinkage, "main", module);
        auto entry = llvm::BasicBlock::Create(context, "entry", mainFunc);
        builder.SetInsertPoint(entry);

        for (auto& stmt : statements)
            stmt->codegen(ctx);

        builder.CreateRet(builder.getInt32(0));
    }
    else
    {
        for (auto& stmt : statements)
            stmt->codegen(ctx);
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();

    std::string targetTriple = "x86_64-w64-windows-gnu";

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);

    if (!target)
    {
        std::cerr << "Target lookup failed: " << error << std::endl;
        return;
    }

    auto targetMachine = target->createTargetMachine(
    targetTriple, "generic", "", llvm::TargetOptions(), std::optional<llvm::Reloc::Model>());

    module.setDataLayout(targetMachine->createDataLayout());
    module.setTargetTriple(llvm::Triple(targetTriple));

    std::error_code EC;
    llvm::raw_fd_ostream dest(name + ".obj", EC, llvm::sys::fs::OF_None);

    llvm::legacy::PassManager pass;
    targetMachine->addPassesToEmitFile(pass, dest, nullptr, llvm::CodeGenFileType::ObjectFile);
    pass.run(module);
    dest.flush();
    dest.close();
}