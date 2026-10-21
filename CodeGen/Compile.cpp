#include "Compile.h"

void compile(std::vector<std::unique_ptr<Statement>>& statements,
             const std::string& obj_path, const CompileOptions& opts)
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
        {
            if (auto* c = dynamic_cast<ClassDefineStatement*>(stmt.get()))
                c->declare(ctx);
            stmt->codegen(ctx);
        }

        builder.CreateRet(builder.getInt32(0));
    }
    else
    {
        for (auto& stmt : statements)
        {
            if (auto* c = dynamic_cast<ClassDefineStatement*>(stmt.get()))
                c->declare(ctx);
            stmt->codegen(ctx);
        }
    }

    llvm::InitializeNativeTarget();
    llvm::InitializeNativeTargetAsmPrinter();
    llvm::InitializeNativeTargetAsmParser();

#ifdef _WIN32
    std::string targetTriple = "x86_64-w64-windows-gnu";
#else
    std::string targetTriple = "x86_64-unknown-linux-gnu";
#endif

    std::string error;
    auto target = llvm::TargetRegistry::lookupTarget(targetTriple, error);
    if (!target)
        throw std::runtime_error("Target lookup failed: " + error);

    auto targetMachine = target->createTargetMachine(
        targetTriple, opts.cpu, "", llvm::TargetOptions(),
        std::optional<llvm::Reloc::Model>(llvm::Reloc::PIC_));

    module.setDataLayout(targetMachine->createDataLayout());
    module.setTargetTriple(llvm::Triple(targetTriple));

    if (llvm::verifyModule(module, &llvm::errs()))
        throw std::runtime_error("LLVM module verification failed!");

    llvm::LoopAnalysisManager LAM;
    llvm::FunctionAnalysisManager FAM;
    llvm::CGSCCAnalysisManager CGAM;
    llvm::ModuleAnalysisManager MAM;

    llvm::PassBuilder PB(targetMachine);
    PB.registerModuleAnalyses(MAM);
    PB.registerCGSCCAnalyses(CGAM);
    PB.registerFunctionAnalyses(FAM);
    PB.registerLoopAnalyses(LAM);
    PB.crossRegisterProxies(LAM, FAM, CGAM, MAM);

    llvm::OptimizationLevel level = llvm::OptimizationLevel::O2;
    if      (opts.size_opt == 's') level = llvm::OptimizationLevel::Os;
    else if (opts.size_opt == 'z') level = llvm::OptimizationLevel::Oz;
    else switch (opts.opt_level)
    {
        case 0: level = llvm::OptimizationLevel::O0; break;
        case 1: level = llvm::OptimizationLevel::O1; break;
        case 3: level = llvm::OptimizationLevel::O3; break;
        default: break;
    }

    llvm::ModulePassManager MPM = (level == llvm::OptimizationLevel::O0)
        ? PB.buildO0DefaultPipeline(level)
        : PB.buildPerModuleDefaultPipeline(level);

    if (opts.verbose) std::cout << "[fab] optimizing\n";
    MPM.run(module, MAM);

    std::error_code EC;

    if (!opts.ir_path.empty())
    {
        llvm::raw_fd_ostream ir(opts.ir_path, EC, llvm::sys::fs::OF_Text);
        if (EC) throw std::runtime_error("Cannot open " + opts.ir_path + ": " + EC.message());
        module.print(ir, nullptr);
    }

    auto emit = [&](const std::string& path, llvm::CodeGenFileType type)
    {
        std::error_code ec;
        llvm::raw_fd_ostream dest(path, ec, llvm::sys::fs::OF_None);
        if (ec) throw std::runtime_error("Cannot open " + path + ": " + ec.message());

        llvm::legacy::PassManager pass;
        if (targetMachine->addPassesToEmitFile(pass, dest, nullptr, type))
            throw std::runtime_error("Target cannot emit this file type!");
        pass.run(module);
        dest.flush();
    };

    if (!opts.asm_path.empty())
        emit(opts.asm_path, llvm::CodeGenFileType::AssemblyFile);

    if (!opts.skip_obj)
        emit(obj_path, llvm::CodeGenFileType::ObjectFile);
}