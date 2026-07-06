using System.Text;
using System.Globalization;
using static AST;

// ═══════════════════════════════════════════════════════════════════════════════
//  FabCCompiler  —  Fab AST → C++17 source
//
//  Pipeline:
//    FabProgram (AST)  →  FabCCompiler.Compile()  →  C++ source string
//    C++ source string →  FabCRunner.Build()       →  native binary (g++/clang++)
//
//  Design decisions:
//  • All Fab values are FabVal (tagged struct) defined in fab_runtime.h (C++).
//  • fab_runtime.h is loaded from disk (next to executable) and inlined into
//    every generated .cpp file — zero external dependencies at runtime.
//  • Strings use std::string; collections use std::vector — RAII, no manual free.
//  • Numbers are always double internally (matching the interpreter).
//  • Structs/classes become C++ structs with FabVal fields.
//  • Collection literals use vararg helpers from the runtime header.
// ═══════════════════════════════════════════════════════════════════════════════

public class FabCCompiler
{
    private readonly StringBuilder _sb = new();
    private int _indent;
    private const string IU = "    ";

    // unique name counters
    private int _tmp;
    private string T() => $"__t{_tmp++}";

    // first-pass metadata
    private readonly List<string> _funcForwards = new();
    private readonly HashSet<string> _usedLibs = new();
    private readonly HashSet<string> _structNames = new();
    private readonly HashSet<string> _classNames = new();

    // className → FabClasses AST node (for constructor/method lookup)
    private readonly Dictionary<string, FabClasses> _classInfo = new();

    // lowercase className → original className (ParseTypeString lowercases user type names)
    private readonly Dictionary<string, string> _classNamesLower = new();

    // variable name → declared class type (so ExprMember can dispatch correctly)
    // Scoped per-function: pushed/popped via EmitFuncDef wrapper
    private readonly Dictionary<string, string> _varTypes = new();

    // Built-in static classes always available (no use statement needed)
    private static readonly HashSet<string> _builtinStaticClasses = new()
        { "Color", "Convert", "ipaddress", "regex" };

    // Statements collected from use "file.fab" imports (parsed at compile time)
    private readonly List<FabBase> _importedStatements = new();

    // namespace alias → canonical lib name (from 'namespace alias = lib;')
    private readonly Dictionary<string, string> _libAliases = new();

    // Name of the class whose methods are currently being emitted
    // Used by ExprCall to redirect bare method calls to ClassName_method(__self)
    private string? _currentClass = null;

    // runtime header content (set by FabCRunner before calling Compile)
    public static string RuntimeHeader { get; set; } = "// fab_runtime.h not embedded\n";

    // ══════════════════════════════════════════════════════════════════════════
    //  Public API
    // ══════════════════════════════════════════════════════════════════════════

    public string Compile(FabProgram program)
    {
        // ── Pre-pass: load any use "file.fab" libraries and collect their AST ──
        LoadFileImports(program.Statements);

        // All statements = program's own + everything from imported files
        // (the built-in error types — Error/TypeError/... — are NOT Fab
        // classes anymore: they're a native C++ struct hierarchy baked
        // straight into fab_runtime.h, see ExprNew/EmitStmt(FabThrow)/EmitTry)
        var allStatements = _importedStatements.Concat(program.Statements).ToList();

        CollectDecls(allStatements);
        CollectLibTypeDecls();

        // Strip '#pragma once' — valid only in header files, not when inlined into .cpp
        var runtimeText = System.Text.RegularExpressions.Regex.Replace(
            RuntimeHeader, @"^\s*#\s*pragma\s+once\s*\r?\n?", "", System.Text.RegularExpressions.RegexOptions.Multiline);
        Raw(runtimeText);
        Raw("\n");

        EmitForwardDecls(allStatements);
        EmitLibTypeDecls();
        EmitTypeDecls(allStatements);
        EmitGlobalDecls(allStatements);
        EmitFunctions(allStatements);
        EmitLibClassMethods();

        L("int main() {");
        _indent++;
        if (_usedLibs.Contains("random") || _usedLibs.Contains("date"))
            L("std::srand((unsigned)std::time(nullptr));");
        L("fab_main();");
        L("return 0;");
        _indent--;
        L("}");

        return _sb.ToString();
    }

    // ── Native error type hierarchy (Error, TypeError, ValueError, ...) ────────
    // Mirrors AST.FabErrorTypes. 'new TypeError("msg")' compiles to a real C++
    // object of the mapped struct type (defined in fab_runtime.h), so
    // 'catch (Error& e)' polymorphically catches every built-in subtype via
    // genuine C++ inheritance — not a FabVal tag check.
    private static readonly Dictionary<string, string> _errorTypeCppNames = new()
    {
        ["Error"] = "FabError",
        ["TypeError"] = "FabErr_TypeError",
        ["ValueError"] = "FabErr_ValueError",
        ["IndexError"] = "FabErr_IndexError",
        ["ArgumentError"] = "FabErr_ArgumentError",
        ["IOError"] = "FabErr_IOError",
        ["NotFoundError"] = "FabErr_NotFoundError",
    };

    private static bool IsErrorCppType(string cppTypeName) =>
        cppTypeName == "FabError" || cppTypeName.StartsWith("FabErr_");

    // ── Load all use "file.fab" statements and parse them into _importedStatements ──
    // Recurses into files imported by already-imported files, so transitive
    // 'use "other.fab";' chains (and everything they in turn declare/use) are
    // fully pulled into the compilation unit. _loadedFilePaths guards against
    // loading the same file twice and against circular imports (A uses B uses A).
    private readonly HashSet<string> _loadedFilePaths = new();

    private void LoadFileImports(IEnumerable<FabBase> stmts)
    {
        foreach (var s in stmts)
        {
            if (s is not FabUse u) continue;

            string? path = u.FilePath;

            if (path == null)
            {
                // Bare 'use libName;' (no quotes). If it's not one of the
                // compiler's builtin function/type libraries, look for a
                // matching header-less source file under 'libs/' — same
                // resolution rule as the interpreter (AST.FabUse.ResolveLibFile):
                // 'libs/<name>' or 'libs/<name>.fab'.
                if (AST.FabLibRegistry.HasLib(u.LibName) || AST.FabLibRegistry.HasLibTypes(u.LibName))
                    continue;

                path = AST.FabUse.ResolveLibFile(u.LibName);
                if (path == null)
                    continue; // genuinely unknown — surfaces as an error elsewhere
            }
            else if (!File.Exists(path))
            {
                // Resolve quoted path: try as-is, then relative to exe directory
                string sysPath = Path.Combine(AppContext.BaseDirectory, path);
                if (File.Exists(sysPath)) path = sysPath;
                else throw new Exception(
                    $"[fab:compile] Library file not found: '{u.FilePath}'\n" +
                    $"  Searched: {Path.GetFullPath(u.FilePath)}\n" +
                    $"  Searched: {Path.GetFullPath(sysPath)}");
            }

            string fullPath = Path.GetFullPath(path);
            if (!_loadedFilePaths.Add(fullPath))
                continue; // already loaded — avoids duplicate/circular imports

            string code = File.ReadAllText(path);
            var tokens = new Lexer().Lex(code).ToList();
            var parsed = new Parser(tokens).ParseStatements();
            _importedStatements.AddRange(parsed);

            // Recurse: this file may itself do 'use "other.fab";' or 'use other;'
            LoadFileImports(parsed);
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Inheritance resolution (mirrors AST.FabClasses.Eval, which the compiler
    //  never invokes since it works statically over the AST instead of running it)
    // ══════════════════════════════════════════════════════════════════════════

    private readonly HashSet<FabClasses> _inheritanceResolved = new();

    private void ResolveInheritance(FabClasses cl)
    {
        if (!_inheritanceResolved.Add(cl)) return; // already merged

        if (cl.BaseClassName == null)
        {
            cl.Fields = cl.OwnFields;
            cl.Methods = cl.OwnMethods;
            cl.Constructor = cl.OwnConstructor;
            return;
        }

        if (!_classInfo.TryGetValue(cl.BaseClassName, out var baseDef))
            throw new Exception($"Base class '{cl.BaseClassName}' is not defined (must be declared before '{cl.Name}')");

        ResolveInheritance(baseDef); // ensure base is fully resolved first

        var mergedFields = new List<FabFieldDef>(baseDef.Fields);
        foreach (var f in cl.OwnFields)
        {
            int idx = mergedFields.FindIndex(bf => bf.Name == f.Name);
            if (idx >= 0) mergedFields[idx] = f;
            else mergedFields.Add(f);
        }

        var mergedMethods = new Dictionary<string, FabFuncDef>(baseDef.Methods);
        foreach (var kv in cl.OwnMethods)
            mergedMethods[kv.Key] = kv.Value;

        cl.Fields = mergedFields;
        cl.Methods = mergedMethods;
        cl.Constructor = cl.OwnConstructor ?? baseDef.Constructor;
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Pass 1 — collect metadata
    // ══════════════════════════════════════════════════════════════════════════

    private void CollectDecls(IEnumerable<FabBase> stmts)
    {
        foreach (var s in stmts)
        {
            switch (s)
            {
                case FabUse u: _usedLibs.Add(u.LibName); break;
                case FabUsingNamespace un: _usedLibs.Add(un.LibName); break;
                case FabNamespaceAlias na: _libAliases[na.Alias] = na.Target; break;
                case FabStructDefStmt sd: _structNames.Add(sd.Def.Name); break;
                case FabClasses cl:
                    ResolveInheritance(cl);
                    _classNames.Add(cl.Name);
                    _classInfo[cl.Name] = cl;
                    _classNamesLower[cl.Name.ToLower()] = cl.Name;
                    break;
                case FabNamespaceDef nd:
                    foreach (var f in nd.Functions) _funcForwards.Add($"FabVal {nd.Name}_{f.Name}({FuncParms(f.Params)});");
                    foreach (var cl in nd.Classes)
                    {
                        ResolveInheritance(cl);
                        _classNames.Add(cl.Name);
                        _classInfo[cl.Name] = cl;
                        _classNamesLower[cl.Name.ToLower()] = cl.Name;
                    }
                    foreach (var st in nd.Structs) _structNames.Add(st.Def.Name);
                    break;
            }
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Library-exposed types (FabLibRegistry.LibClasses / LibStructs)
    //  Mirrors CollectDecls/EmitTypeDecls/EmitFunctions, but for classes and
    //  structs registered against a builtin library (see FabLibRegistry) rather
    //  than declared in the program's own source. A library only contributes
    //  its types once 'use <libName>;' (or an equivalent import) actually
    //  appears in the program — tracked the same way as everything else via
    //  _usedLibs, which CollectDecls already populates from FabUse/FabUsingNamespace.
    // ══════════════════════════════════════════════════════════════════════════

    private void CollectLibTypeDecls()
    {
        foreach (var lib in _usedLibs)
        {
            if (AST.FabLibRegistry.LibStructs.TryGetValue(lib, out var structs))
                foreach (var sd in structs.Values)
                    _structNames.Add(sd.Name);

            if (AST.FabLibRegistry.LibClasses.TryGetValue(lib, out var classes))
                foreach (var cl in classes.Values)
                {
                    ResolveInheritance(cl);
                    _classNames.Add(cl.Name);
                    _classInfo[cl.Name] = cl;
                    _classNamesLower[cl.Name.ToLower()] = cl.Name;
                }
        }
    }

    private void EmitLibTypeDecls()
    {
        foreach (var lib in _usedLibs)
        {
            if (AST.FabLibRegistry.LibStructs.TryGetValue(lib, out var structs))
                foreach (var sd in structs.Values)
                    EmitStructDef(sd);

            if (AST.FabLibRegistry.LibClasses.TryGetValue(lib, out var classes))
                foreach (var cl in classes.Values)
                    EmitClassDef(cl);
        }
    }

    private void EmitLibClassMethods()
    {
        foreach (var lib in _usedLibs)
            if (AST.FabLibRegistry.LibClasses.TryGetValue(lib, out var classes))
                foreach (var cl in classes.Values)
                    EmitClassMethods(cl);
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Forward declarations
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitForwardDecls(IEnumerable<FabBase> stmts)
    {
        Raw("// ── forward declarations ──\n");
        foreach (var s in stmts)
            if (s is FabFuncDef fd)
            {
                string retType = ResolveClassName(fd.ReturnType) ?? "FabVal";
                string note = (fd.IsNative && fd.Body.Count == 0)
                    ? "  // native — no body emitted, implement by hand under this exact signature"
                    : "";
                Raw($"{retType} {CName(fd.Name)}({FuncParms(fd.Params)});{note}\n");
            }
        foreach (var fwd in _funcForwards) Raw(fwd + "\n");
        Raw("\n");
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Type definitions (structs, classes)
    //  In C++ we use plain structs with FabVal fields — no typedef needed.
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitTypeDecls(IEnumerable<FabBase> stmts)
    {
        foreach (var s in stmts)
        {
            switch (s)
            {
                case FabStructDefStmt sd: EmitStructDef(sd.Def); break;
                case FabClasses cl: EmitClassDef(cl); break;
                case FabNamespaceDef nd:
                    foreach (var cl in nd.Classes) EmitClassDef(cl);
                    foreach (var sd in nd.Structs) EmitStructDef(sd.Def);
                    break;
            }
        }
        Raw("\n");
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Global variable definitions ('int a = 1;' / 'static int a = 1;')
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitGlobalDecls(IEnumerable<FabBase> stmts)
    {
        Raw("// ── global variables ──\n");
        foreach (var s in stmts)
            if (s is FabGlobalDecl gd) EmitGlobalDecl(gd);
        Raw("\n");
    }

    private void EmitGlobalDecl(FabGlobalDecl gd)
    {
        string prefix = gd.IsStatic ? "static " : "";
        switch (gd.Decl)
        {
            case FabAssign a when a.DeclaredType != null:
                {
                    string? cls = ResolveClassName(a.DeclaredType);
                    if (cls != null)
                        L($"{prefix}{cls} {a.Name} = {Expr(a.Expr)};");
                    else
                        L($"{prefix}FabVal {a.Name} = {Expr(a.Expr)};");
                    break;
                }
            case FabVarDecl vd:
                L($"{prefix}FabVal {vd.Name} = FAB_NIL;");
                break;
            case FabConst co:
                L($"{prefix}const FabVal {co.Name} = {Expr(co.Expr)};");
                break;
            case FabStatementList sl:
                foreach (var inner in sl.Statements)
                    EmitGlobalDecl(new FabGlobalDecl(inner, gd.IsStatic));
                break;
            default:
                // ListDecl/DictDecl/VFreeDecl etc. — reuse the existing local-style emission.
                EmitStmt(gd.Decl);
                break;
        }
    }

    private void EmitStructDef(FabStructDef def)
    {
        Raw($"struct {def.Name} {{");
        foreach (var f in def.Fields) Raw($" FabVal {f.Name};");
        Raw($" }};\n");
    }

    private void EmitClassDef(FabClasses cl)
    {
        if (cl.IsStatic) { EmitStaticClassDef(cl); return; }

        // Fields as zero-initialised FabVal members
        Raw($"struct {cl.Name} {{");
        foreach (var f in cl.Fields) Raw($" FabVal {f.Name}{{}};");
        Raw($" }};\n");

        // Forward-declare constructor (void) and every method
        if (cl.Constructor != null)
        {
            string cp = FuncParms(cl.Constructor.Params);
            Raw($"void {cl.Name}_{cl.Name}({cl.Name}* __self{(cp.Length > 0 ? ", " + cp : "")});\n");
        }
        foreach (var (mname, mdef) in cl.Methods)
        {
            string retType = ResolveClassName(mdef.ReturnType) ?? "FabVal";
            string mp = FuncParms(mdef.Params);
            Raw($"{retType} {cl.Name}_{mname}({cl.Name}* __self{(mp.Length > 0 ? ", " + mp : "")});\n");
        }
        Raw("\n");
    }

    // Static classes have no instances: fields become global FabVal statics
    // namespaced as ClassName_fieldName, and methods become free functions
    // ClassName_methodName(args) with no __self parameter.
    private void EmitStaticClassDef(FabClasses cl)
    {
        foreach (var f in cl.Fields)
        {
            string init = f.Default != null ? Expr(f.Default) : "FAB_NIL";
            Raw($"static FabVal {cl.Name}_{f.Name} = {init};\n");
        }
        foreach (var (mname, mdef) in cl.Methods)
        {
            string retType = ResolveClassName(mdef.ReturnType) ?? "FabVal";
            string mp = FuncParms(mdef.Params);
            Raw($"{retType} {cl.Name}_{mname}({mp});\n");
        }
        Raw("\n");
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Function bodies
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitFunctions(IEnumerable<FabBase> stmts)
    {
        foreach (var s in stmts)
        {
            switch (s)
            {
                case FabFuncDef fd when fd.IsNative && fd.Body.Count == 0:
                    // Bodyless native stub — only the forward declaration
                    // (already emitted) is produced. The real implementation
                    // must be supplied by hand, e.g. appended to
                    // fab_runtime.h, under the exact mangled name/signature
                    // shown in the forward-declaration comment.
                    break;
                case FabFuncDef fd: EmitFuncDef(fd, ""); break;
                case FabNamespaceDef nd:
                    foreach (var f in nd.Functions) EmitFuncDef(f, nd.Name + "_");
                    break;
                case FabClasses cl: EmitClassMethods(cl); break;
            }
        }
    }

    private void EmitFuncDef(FabFuncDef fd, string prefix)
    {
        // Return type: class name if declared return type is a user class, else FabVal
        string? resolvedRet = ResolveClassName(fd.ReturnType);
        string retType = resolvedRet ?? "FabVal";

        L($"{retType} {CName(prefix + fd.Name)}({FuncParms(fd.Params)}) {{");
        _indent++;
        var savedVarTypes = new Dictionary<string, string>(_varTypes);
        _varTypes.Clear();
        foreach (var (pt, pn) in fd.Params)
        {
            string? rc = ResolveClassName(pt);
            if (rc != null) _varTypes[pn] = rc;
        }
        EmitBody(fd.Body);
        _varTypes.Clear();
        foreach (var kv in savedVarTypes) _varTypes[kv.Key] = kv.Value;
        if (retType == "FabVal") L("return FAB_NIL;");
        else L($"return {retType}{{}};");
        _indent--;
        L("}");
        Raw("\n");
    }

    private void EmitClassMethods(FabClasses cl)
    {
        if (cl.IsStatic) { EmitStaticClassMethods(cl); return; }

        // ── Constructor ──────────────────────────────────────────────────────
        if (cl.Constructor != null)
        {
            var ctor = cl.Constructor;
            string cp = FuncParms(ctor.Params);
            L($"void {cl.Name}_{cl.Name}({cl.Name}* __self{(cp.Length > 0 ? ", " + cp : "")}) {{");
            _indent++;
            foreach (var f in cl.Fields)
                L($"FabVal& {f.Name} = __self->{f.Name};");
            var savedVarTypes = new Dictionary<string, string>(_varTypes);
            var savedClass = _currentClass;
            _currentClass = cl.Name;
            foreach (var (pt, pn) in ctor.Params)
            {
                string? rc = ResolveClassName(pt);
                if (rc != null) _varTypes[pn] = rc;
            }
            EmitBody(ctor.Body);
            _varTypes.Clear();
            foreach (var kv in savedVarTypes) _varTypes[kv.Key] = kv.Value;
            _currentClass = savedClass;
            _indent--;
            L("}");
            Raw("\n");
        }

        // ── Methods ──────────────────────────────────────────────────────────
        foreach (var (mname, mdef) in cl.Methods)
        {
            string retType = ResolveClassName(mdef.ReturnType) ?? "FabVal";
            string mp = FuncParms(mdef.Params);
            L($"{retType} {cl.Name}_{mname}({cl.Name}* __self{(mp.Length > 0 ? ", " + mp : "")}) {{");
            _indent++;
            foreach (var f in cl.Fields)
                L($"FabVal& {f.Name} = __self->{f.Name};");
            var savedVarTypes = new Dictionary<string, string>(_varTypes);
            var savedClass = _currentClass;
            _currentClass = cl.Name;
            foreach (var (pt, pn) in mdef.Params)
            {
                string? rc = ResolveClassName(pt);
                if (rc != null) _varTypes[pn] = rc;
            }
            EmitBody(mdef.Body);
            _varTypes.Clear();
            foreach (var kv in savedVarTypes) _varTypes[kv.Key] = kv.Value;
            _currentClass = savedClass;
            if (retType == "FabVal") L("return FAB_NIL;");
            else L($"return {retType}{{}};");
            _indent--;
            L("}");
            Raw("\n");
        }
    }

    private void EmitStaticClassMethods(FabClasses cl)
    {
        foreach (var (mname, mdef) in cl.Methods)
        {
            string retType = ResolveClassName(mdef.ReturnType) ?? "FabVal";
            string mp = FuncParms(mdef.Params);
            L($"{retType} {cl.Name}_{mname}({mp}) {{");
            _indent++;
            foreach (var f in cl.Fields)
                L($"FabVal& {f.Name} = {cl.Name}_{f.Name};");
            var savedVarTypes = new Dictionary<string, string>(_varTypes);
            var savedClass = _currentClass;
            _currentClass = cl.Name;
            foreach (var (pt, pn) in mdef.Params)
            {
                string? rc = ResolveClassName(pt);
                if (rc != null) _varTypes[pn] = rc;
            }
            EmitBody(mdef.Body);
            _varTypes.Clear();
            foreach (var kv in savedVarTypes) _varTypes[kv.Key] = kv.Value;
            _currentClass = savedClass;
            if (retType == "FabVal") L("return FAB_NIL;");
            else L($"return {retType}{{}};");
            _indent--;
            L("}");
            Raw("\n");
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Statement emitter
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitBody(IEnumerable<FabBase> stmts)
    {
        foreach (var s in stmts) EmitStmt(s);
    }

    private void EmitStmt(FabBase stmt)
    {
        switch (stmt)
        {
            case FabAssign a when a.DeclaredType != null:
                {
                    // ClassName[] name = [...]  →  std::vector<ClassName>
                    string dt = a.DeclaredType;
                    if (dt.EndsWith("[]"))
                    {
                        string? elemClass = ResolveClassName(dt[..^2]);
                        if (elemClass != null)
                        {
                            L($"std::vector<{elemClass}> {a.Name};");
                            _varTypes[a.Name + "[]"] = elemClass;
                            if (a.Expr is FabListLiteral ll0)
                                foreach (var item in ll0.Items)
                                    L($"{a.Name}.push_back({ExprNewAsClass(item, elemClass)});");
                            else if (a.Expr is not FabNullLit)
                                L($"// NOTE: non-literal initializer for {a.Name}[] not supported");
                            break;
                        }
                    }

                    string? resolvedClass = ResolveClassName(a.DeclaredType);
                    if (resolvedClass != null)
                    {
                        if (a.Expr is FabNew newExpr)
                        {
                            // Bank bk = new Bank(...)  →  Bank bk{}; Bank_Bank(&bk, ...);
                            L($"{resolvedClass} {a.Name}{{}};");
                            _varTypes[a.Name] = resolvedClass;
                            if (_classInfo.TryGetValue(resolvedClass, out var clDef) && clDef.Constructor != null)
                            {
                                string ctorArgs = string.Join(", ", newExpr.Args.Select(Expr));
                                string sep = ctorArgs.Length > 0 ? ", " : "";
                                L($"{resolvedClass}_{resolvedClass}(&{a.Name}{sep}{ctorArgs});");
                            }
                        }
                        else
                        {
                            // Vec3 pos1 = vec3(1, 0, 2)  →  Vec3 pos1 = vec3(...);
                            L($"{resolvedClass} {a.Name} = {Expr(a.Expr)};");
                            _varTypes[a.Name] = resolvedClass;
                        }
                    }
                    else
                    {
                        L($"FabVal {a.Name} = {Expr(a.Expr)};");
                    }
                    break;
                }

            case FabAssign a:
                L($"{a.Name} = {Expr(a.Expr)};");
                break;

            case FabVarDecl vd:
                L($"FabVal {vd.Name} = FAB_NIL;");
                break;

            case FabListDecl ld:
                {
                    string? elemClass = ResolveClassName(ld.ElementType);
                    if (elemClass != null)
                    {
                        // ClassName[] name  →  std::vector<ClassName> name;
                        L($"std::vector<{elemClass}> {ld.Name};");
                        _varTypes[ld.Name + "[]"] = elemClass; // mark as class-vector
                        if (ld.Initializer is FabListLiteral ll0)
                            foreach (var item in ll0.Items)
                                L($"{ld.Name}.push_back({ExprNewAsClass(item, elemClass)});");
                    }
                    else
                    {
                        string tmp = T();
                        L($"FabVal {ld.Name};");
                        L($"{{ FabListPtr {tmp} = fab_list_new({ld.Capacity});");
                        if (ld.Initializer is FabListLiteral ll0)
                            foreach (var item in ll0.Items)
                                L($"  fab_list_push({tmp}, {Expr(item)});");
                        L($"  {ld.Name} = fab_list_val({tmp}); }}");
                    }
                    break;
                }

            case FabDictDecl dd:
                {
                    string tmp = T();
                    L($"FabVal {dd.Name};");
                    L($"{{ FabDictPtr {tmp} = fab_dict_new({dd.Capacity});");
                    if (dd.Initializer is FabDictLiteral dl0)
                        foreach (var (k, v) in dl0.Pairs)
                            L($"  fab_dict_set({tmp}, {Expr(k)}, {Expr(v)});");
                    L($"  {dd.Name} = fab_dict_val({tmp}); }}");
                    break;
                }

            case FabConst co:
                L($"const FabVal {co.Name} = {Expr(co.Expr)};");
                break;

            case FabVFreeDecl vf:
                L($"FabVal {vf.Name} = {Expr(vf.Expr)};");
                break;

            case FabConstVFree cv:
                L($"const FabVal {cv.Name} = {Expr(cv.Expr)};");
                break;

            case FabStructDecl sd:
                {
                    L($"{sd.StructName} {sd.VarName}{{}};");
                    if (sd.Initializer is FabDictLiteral idl)
                        foreach (var (k, v) in idl.Pairs)
                        {
                            string fname = k is FabString fs ? fs.Value : Expr(k);
                            L($"{sd.VarName}.{fname} = {Expr(v)};");
                        }
                    break;
                }

            case FabCompoundAssign ca when ca.Op is "++" or "--":
                // d is a double field — use prefix form to avoid comma-expression issues
                L($"{{ double& __d = {ca.Name}.d; {ca.Op[0]}__d{ca.Op[1]}; (void)__d; }}");
                break;

            case FabCompoundAssign ca when ca.Op == "+=":
                L($"{ca.Name} = fab_add({ca.Name}, {Expr(ca.Expr!)});");
                break;

            case FabCompoundAssign ca when ca.Op == "^=":
                L($"{ca.Name} = fab_pow_v({ca.Name}, {Expr(ca.Expr!)});");
                break;

            case FabCompoundAssign ca:
                L($"{ca.Name}.d {ca.Op} {Expr(ca.Expr!)}.d;");
                break;

            case FabInstanceAssign ia:
                {
                    // Private field write check: only allow from within the same class
                    if (_varTypes.TryGetValue(ia.ObjName, out var iaClass)
                        && _classInfo.TryGetValue(iaClass, out var iaClDef))
                    {
                        var iaField = iaClDef.Fields.FirstOrDefault(f => f.Name == ia.Field);
                        if (iaField != null && iaField.Visibility == "private"
                            && _currentClass != iaClass)
                            throw new Exception($"Field '{ia.Field}' is private in class '{iaClass}'");
                    }
                    // Works for both user structs and classes: obj.field = expr
                    L($"{ia.ObjName}.{ia.Field} = {Expr(ia.Value)};");
                    break;
                }

            case FabDerefAssign da:
                // *ptrVar = value  →  fab_deref_set(ptrVar, value)
                L($"fab_deref_set({da.PtrVarName}, {Expr(da.Value)});");
                break;

            case FabIndexAssign ixa:
                {
                    // std::vector<ClassName> case
                    if (_varTypes.TryGetValue(ixa.Name + "[]", out var vecElemClass))
                    {
                        string idx = Expr(ixa.Index);
                        string val = ExprNewAsClass(ixa.Value, vecElemClass);
                        L($"{ixa.Name}[(int)({idx}).d] = {val};");
                    }
                    else
                    {
                        string tk = T(), tv = T();
                        L($"{{ FabVal {tk}={Expr(ixa.Index)}, {tv}={Expr(ixa.Value)};");
                        L($"  if ({ixa.Name}.tag==FabTag::List) fab_list_set({ixa.Name}.list,(int){tk}.d,{tv});");
                        L($"  else if ({ixa.Name}.tag==FabTag::Dict) fab_dict_set({ixa.Name}.dict,{tk},{tv}); }}");
                    }
                    break;
                }

            case FabWriteln wl:
                L($"fab_writeln({Expr(wl.Expr)});");
                break;

            case FabWrite w:
                L($"fab_write({Expr(w.Expr)});");
                break;

            case FabIf fi: EmitIf(fi); break;
            case FabWhile fw: EmitWhile(fw); break;
            case FabFor ff: EmitFor(ff); break;
            case FabForEach fe: EmitForEach(fe); break;
            case FabSwitch sw: EmitSwitch(sw); break;

            case FabReturn ret:
                L(ret.Expr == null ? "return FAB_NIL;" : $"return {Expr(ret.Expr)};");
                break;

            case FabThrow th:
                {
                    // If we can statically tell the thrown expression is a
                    // native error type (new TypeError(...) / a caught error
                    // variable being rethrown) or a user-defined class
                    // instance, throw the native C++ object directly so
                    // 'catch (X& e)' can target it by real type — including
                    // 'catch (Error& e)' catching every built-in error
                    // subtype via genuine C++ inheritance. Otherwise fall
                    // back to a string-carrying runtime_error, which
                    // 'catch (string e)' picks up via fab_str(e.what()).
                    string? throwClass = th.Expr switch
                    {
                        FabNew tn when _errorTypeCppNames.TryGetValue(tn.ClassName, out var ecpp) => ecpp,
                        FabNew tn => ResolveClassName(tn.ClassName),
                        FabVariable tv when _varTypes.TryGetValue(tv.Name, out var tvc) => tvc,
                        _ => null
                    };
                    if (throwClass != null)
                        L($"throw {Expr(th.Expr)};");
                    else
                        L($"throw std::runtime_error(fab_fmt({Expr(th.Expr)}));");
                    break;
                }

            case FabTry ft: EmitTry(ft); break;

            case FabBreak: L("break;"); break;
            case FabContinue: L("continue;"); break;

            case FabInputIn ii:
                if (ii.DeclaredType == null)
                {
                    L($"{{ FabVal __raw = fab_readline(); {ii.VarName} = fab_parse_input(__raw); }}");
                }
                else
                {
                    // Unlike the untyped form, a declared type must actually be
                    // validated against — fab_parse_input() alone only guesses
                    // "looks numeric or not" and never rejects bad input.
                    L($"FabVal {ii.VarName} = FAB_NIL;");
                    L($"{{ FabVal __raw = fab_readline(); {ii.VarName} = fab_parse_typed_input(__raw, \"{ii.DeclaredType}\"); }}");
                }
                break;

            case FabDelete del:
                L($"{del.Name} = FAB_NIL;");
                break;

            case FabSystem sys:
                // system("cmd") — fire-and-forget shell command
                L($"{{ (void)std::system({CStr(sys.Command)}); }}");
                break;

            case FabTupleDestructure td:
                {
                    string tmp = T();
                    L($"FabVal {tmp} = {Expr(td.Expr)};");
                    for (int i = 0; i < td.Targets.Count; i++)
                    {
                        var (dt, tname) = td.Targets[i];
                        if (dt != null) L($"FabVal {tname} = {tmp}.tuple_items[{i}];");
                        else L($"{tname} = {tmp}.tuple_items[{i}];");
                    }
                    break;
                }

            case FabFuncDef fd: EmitFuncDef(fd, ""); break;

            case FabStatementList sl:
                foreach (var s in sl.Statements) EmitStmt(s);
                break;

            case FabUse:
            case FabUsingNamespace:
            case FabNamespace:
            case FabNamespaceDef:
            case FabNamespaceAlias:
            case FabStructDefStmt:
            case FabClasses:
            case FabGlobalDecl:
                break;

            default:
                L($"(void)({Expr(stmt)});");
                break;
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Control flow
    // ══════════════════════════════════════════════════════════════════════════

    private void EmitIf(FabIf fi)
    {
        bool first = true;
        foreach (var (cond, body) in fi.Branches)
        {
            L($"{(first ? "if" : "else if")} (fab_truthy({Expr(cond)})) {{");
            first = false;
            _indent++; EmitBody(body); _indent--;
            L("}");
        }
        if (fi.ElseBody != null)
        {
            L("else {");
            _indent++; EmitBody(fi.ElseBody); _indent--;
            L("}");
        }
    }

    private void EmitWhile(FabWhile fw)
    {
        L($"while (fab_truthy({Expr(fw.Condition)})) {{");
        _indent++; EmitBody(fw.Body); _indent--;
        L("}");
    }

    private void EmitFor(FabFor ff)
    {
        string init = ff.Init switch
        {
            FabStatementList sl => "FabVal " + string.Join(", ", sl.Statements.Select(s => s switch
            {
                FabAssign a when a.DeclaredType != null => $"{a.Name} = {Expr(a.Expr)}",
                FabVarDecl vd => $"{vd.Name} = FAB_NIL",
                _ => throw new Exception("Unsupported declaration inside for-loop multi-init"),
            })),
            FabAssign a when a.DeclaredType != null => $"FabVal {a.Name} = {Expr(a.Expr)}",
            FabVarDecl vd => $"FabVal {vd.Name} = FAB_NIL",
            var other => Expr(other),
        };
        string cond = $"fab_truthy({Expr(ff.Condition)})";
        string step = ff.Step switch
        {
            FabCompoundAssign ca when ca.Op is "++" or "--" => $"{ca.Name}.d{ca.Op}",
            FabCompoundAssign ca when ca.Op == "+=" => $"{ca.Name} = fab_add({ca.Name}, {Expr(ca.Expr!)})",
            FabCompoundAssign ca => $"{ca.Name}.d {ca.Op} {Expr(ca.Expr!)}.d",
            var other => Expr(other),
        };
        L($"for ({init}; {cond}; {step}) {{");
        _indent++; EmitBody(ff.Body); _indent--;
        L("}");
    }

    private void EmitForEach(FabForEach fe)
    {
        // std::vector<ClassName> case: for (ClassName x in classVec) { ... }
        if (fe.Collection is FabVariable cv0 && _varTypes.TryGetValue(cv0.Name + "[]", out var vecElemClass))
        {
            L($"for (auto& {fe.VarName} : {cv0.Name}) {{");
            _indent++;
            bool hadPrev = _varTypes.TryGetValue(fe.VarName, out var prevType);
            _varTypes[fe.VarName] = vecElemClass;
            EmitBody(fe.Body);
            if (hadPrev) _varTypes[fe.VarName] = prevType!; else _varTypes.Remove(fe.VarName);
            _indent--;
            L("}");
            return;
        }

        // Generic FabVal-tagged collection: List / Dict / String / Tuple.
        // Each branch is a real C++ for-loop (not a lambda) so that break/continue
        // inside the Fab loop body compile to ordinary C++ break/continue.
        string colVar = T();
        L($"FabVal {colVar} = {Expr(fe.Collection)};");

        L($"if ({colVar}.tag == FabTag::List) {{");
        _indent++;
        L($"for (auto& __it : {colVar}.list->items) {{");
        _indent++;
        L($"FabVal {fe.VarName} = __it;");
        EmitBody(fe.Body);
        _indent--; L("}");
        _indent--; L("}");

        L($"else if ({colVar}.tag == FabTag::Dict) {{");
        _indent++;
        L($"for (auto& __p : {colVar}.dict->pairs) {{");
        _indent++;
        L($"FabVal {fe.VarName} = fab_make_tuple(2, __p.first, __p.second);");
        EmitBody(fe.Body);
        _indent--; L("}");
        _indent--; L("}");

        L($"else if ({colVar}.tag == FabTag::String) {{");
        _indent++;
        L($"for (char __ch : {colVar}.s) {{");
        _indent++;
        L($"FabVal {fe.VarName} = fab_char_v(__ch);");
        EmitBody(fe.Body);
        _indent--; L("}");
        _indent--; L("}");

        L($"else if ({colVar}.tag == FabTag::Tuple) {{");
        _indent++;
        L($"for (auto& __it : {colVar}.tuple_items) {{");
        _indent++;
        L($"FabVal {fe.VarName} = __it;");
        EmitBody(fe.Body);
        _indent--; L("}");
        _indent--; L("}");
    }

    private void EmitSwitch(FabSwitch sw)
    {
        string tmp = T();
        L($"FabVal {tmp} = {Expr(sw.Expr)};");
        bool first = true;
        foreach (var (vals, body) in sw.Cases)
        {
            string cond = string.Join(" || ", vals.Select(v => $"fab_eq({tmp},{Expr(v)})"));
            L($"{(first ? "if" : "else if")} ({cond}) {{");
            first = false;
            _indent++; EmitBody(body); _indent--;
            L("}");
        }
        if (sw.DefaultBody != null)
        {
            L(first ? "{" : "else {");
            _indent++; EmitBody(sw.DefaultBody); _indent--;
            L("}");
        }
    }

    // ── try / catch / finally ────────────────────────────────────────────────
    // 'finally' has no native C++ equivalent, so it's emitted as an RAII guard
    // whose destructor runs the finally body on every exit path (normal,
    // exception caught, exception rethrown, or a return/break/continue that
    // unwinds through this scope — C++ destructors run in all of these cases).
    //
    // Catch matching mirrors the interpreter:
    //   catch (string e)   → catch (const std::exception&), e = fab_str(what())
    //   catch (X e)        → catch (X& e)   — native typed catch (exact type;
    //                        C++ has no runtime link between our flat structs,
    //                        so base-class catches only match the exact type
    //                        actually thrown, unlike the interpreter's walk-up)
    //   catch (e)          → catches both std::exception and everything else
    // Catch matching mirrors the interpreter:
    //   catch (TypeError e) → catch (FabErr_TypeError& e) — a REAL native C++
    //                        type. 'catch (Error& e)' catches every built-in
    //                        error subtype polymorphically via genuine C++
    //                        inheritance (FabError <- FabErr_TypeError <- ...
    //                        defined in fab_runtime.h), not a tag check.
    //   catch (string e) /
    //   catch (e)          → universal fallback (kept identical, see
    //                        AST.FabTry): catches anything — plain throws,
    //                        FabError values, and internal engine errors
    //                        alike — always binding a formatted FabVal string.
    private void EmitTry(FabTry ft)
    {
        int gid = _tmp++;
        bool hasFinally = ft.FinallyBody != null;

        if (hasFinally)
        {
            L("{");
            _indent++;
            L($"struct __FabFinallyGuard_{gid} {{");
            _indent++;
            L("std::function<void()> fn;");
            L($"__FabFinallyGuard_{gid}(std::function<void()> f) : fn(std::move(f)) {{}}");
            L($"~__FabFinallyGuard_{gid}() {{ fn(); }}");
            _indent--;
            L($"}} __fab_fin_guard_{gid}([&]() {{");
            _indent++;
            EmitBody(ft.FinallyBody!);
            _indent--;
            L("});");
        }

        L("try {");
        _indent++;
        EmitBody(ft.Body);
        _indent--;
        L("}");

        foreach (var (typeName, varName, cbody) in ft.Catches)
        {
            if (typeName == null || typeName == "string")
            {
                L($"catch (const std::exception& __exc_{gid}) {{");
                _indent++;
                L($"FabVal {varName} = fab_str(__exc_{gid}.what());");
                EmitBody(cbody);
                _indent--;
                L("}");
                L("catch (...) {");
                _indent++;
                L($"FabVal {varName} = fab_str(\"unknown error\");");
                EmitBody(cbody);
                _indent--;
                L("}");
            }
            else
            {
                if (!_errorTypeCppNames.TryGetValue(typeName, out var cppType))
                    throw new Exception(
                        $"'catch ({typeName} {varName})': unknown error type '{typeName}'. " +
                        $"Valid types: {string.Join(", ", _errorTypeCppNames.Keys)}, or 'string'.");
                L($"catch ({cppType}& {varName}) {{");
                _indent++;
                bool had = _varTypes.TryGetValue(varName, out var prevType);
                _varTypes[varName] = cppType;
                EmitBody(cbody);
                if (had) _varTypes[varName] = prevType!; else _varTypes.Remove(varName);
                _indent--;
                L("}");
            }
        }

        if (hasFinally)
        {
            _indent--;
            L("}");
        }
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Expression builder
    // ══════════════════════════════════════════════════════════════════════════

    private string Expr(FabBase node) => node switch
    {
        FabNumber n => $"fab_num({n.Value.ToString(CultureInfo.InvariantCulture)})",
        FabBoolLit b => b.Value ? "FAB_TRUE" : "FAB_FALSE",
        FabString s => $"fab_str({CStr(s.Value)})",
        FabChar ch => $"fab_char_v('{EscCh(ch.Value)}')",
        FabNullLit => "FAB_NIL",
        FabFString fs => ExprFStr(fs),
        FabVariable v => ExprVariable(v),
        FabBinOp bo => ExprBinOp(bo),
        FabComparison c => ExprCmp(c),
        FabLogical lo => ExprLogical(lo),
        FabCall fc => ExprCall(fc),
        FabMemberAccess ma => ExprMember(ma),
        FabIndexAccess ia => ExprIndex(ia),
        FabLibCall lc => ExprLibCall(lc),
        FabNew fn => ExprNew(fn),
        FabCast fc => ExprCast(fc),
        FabIs fi => ExprIs(fi),
        FabIn fi => ExprIn(fi),
        FabListLiteral ll => ExprListLit(ll),
        FabDictLiteral dl => ExprDictLit(dl),
        FabTupleLiteral tl => ExprTupleLit(tl),
        FabTypeMethod tm => ExprTypeMethod(tm),
        FabSizeOf fs => SizeOfExpr(fs),
        FabAddressOf ao => $"fab_addr_of(&{ao.VarName})",
        FabDeref dr => $"fab_deref({Expr(dr.Expr)})",
        FabType ft => ExprType(ft),
        FabSystem sys => $"([&](){{(void)std::system({CStr(sys.Command)});return FAB_NIL;}}())",
        FabLiteral => "FAB_NIL",
        FabLambdaLiteral => throw new Exception(
            "Lambda expressions ('(params) => ...' / 'vfree name(...) => ...') are not " +
            "yet supported by --compile. Use a regular 'def' function, or a bodyless " +
            "'native def name(...);' with a host-registered implementation, instead. " +
            "They work fine in the interpreter (running the script without --compile)."),
        _ => $"FAB_NIL/*{node.GetType().Name}*/",
    };

    // Math constants that are zero-arg functions in fab_runtime.h
    private static readonly HashSet<string> _mathConsts = new()
        { "pi", "e", "tau", "fi", "sqrt2", "gam", "g" };

    private string ExprVariable(FabVariable v)
    {
        if (_usedLibs.Contains("math") && _mathConsts.Contains(v.Name))
            return ExprLibCall(new FabLibCall("math", v.Name, null));
        return v.Name;
    }

    private string ExprBinOp(FabBinOp bo)
    {
        string l = Expr(bo.Left), r = Expr(bo.Right);
        return bo.Op switch
        {
            "+" => $"fab_add({l},{r})",
            "-" => $"fab_sub({l},{r})",
            "*" => $"fab_mul({l},{r})",
            "/" => $"fab_div({l},{r})",
            "%" => $"fab_mod({l},{r})",
            "^" => $"fab_pow_v({l},{r})",
            _ => $"FAB_NIL/*{bo.Op}*/",
        };
    }

    private string ExprCmp(FabComparison c)
    {
        string l = Expr(c.Left), r = Expr(c.Right);
        return c.Op switch
        {
            "==" => $"fab_bool_v(fab_eq({l},{r}))",
            "!=" => $"fab_bool_v(!fab_eq({l},{r}))",
            "<" => $"fab_lt({l},{r})",
            ">" => $"fab_gt({l},{r})",
            "<=" => $"fab_le({l},{r})",
            ">=" => $"fab_ge({l},{r})",
            _ => "FAB_FALSE",
        };
    }

    private string ExprLogical(FabLogical lo)
    {
        string l = $"fab_truthy({Expr(lo.Left)})";
        string r = $"fab_truthy({Expr(lo.Right)})";
        return lo.Op == "and" ? $"fab_bool_v({l}&&{r})" : $"fab_bool_v({l}||{r})";
    }

    // All math lib function names
    private static readonly HashSet<string> _mathFuncs = new()
    {
        "pow","sqrt","cbrt","abs","floor","ceil","round","sin","cos","tan",
        "log","max","min","clamp","rad","deg","exp","atan2","sing","lerp",
        "hypot","factorial","pi","e","tau","fi","sqrt2","gam","g"
    };

    private string ExprCall(FabCall fc)
    {
        if (_usedLibs.Contains("math") && _mathFuncs.Contains(fc.Name))
            return ExprLibCall(new FabLibCall("math", fc.Name, fc.Args));

        // Bare call inside a class method body: length() → Vec3_length(__self)
        if (_currentClass != null
            && _classInfo.TryGetValue(_currentClass, out var selfClass)
            && selfClass.Methods.ContainsKey(fc.Name))
        {
            string callArgs = string.Join(", ", fc.Args.Select(Expr));
            string sep = callArgs.Length > 0 ? ", " : "";
            return $"{_currentClass}_{fc.Name}(__self{sep}{callArgs})";
        }

        return $"{CName(fc.Name)}({string.Join(",", fc.Args.Select(Expr))})";
    }

    private string ExprMember(FabMemberAccess ma)
    {
        // ── lib namespace call: math.sqrt(...) or alias mt.sqrt(...) etc. ──
        if (ma.Target is FabVariable lv)
        {
            // Resolve alias first
            string resolvedLib = _libAliases.TryGetValue(lv.Name, out var aliasTarget) ? aliasTarget : lv.Name;
            if (_usedLibs.Contains(resolvedLib))
                return ExprLibCall(new FabLibCall(resolvedLib, ma.Member, ma.Args ?? new List<FabBase>()));
        }

        // ── user-defined static class: StaticClassName.field / .Method(args) ──
        if (ma.Target is FabVariable scv
            && _classInfo.TryGetValue(scv.Name, out var staticClassDef)
            && staticClassDef.IsStatic
            && !_varTypes.ContainsKey(scv.Name))
        {
            bool isStaticMethod = staticClassDef.Methods.ContainsKey(ma.Member);
            if (!isStaticMethod)
                return $"{scv.Name}_{ma.Member}";
            var sArgs = ma.Args?.Select(Expr).ToList() ?? new List<string>();
            return $"{scv.Name}_{ma.Member}({string.Join(", ", sArgs)})";
        }

        // ── Builtin static classes: Color.red, Convert.to_int(...), ipaddress.parse(...) ──
        if (ma.Target is FabVariable bsv && _builtinStaticClasses.Contains(bsv.Name))
            return ExprLibCall(new FabLibCall(bsv.Name, ma.Member, ma.Args ?? new List<FabBase>()));

        // ── native error instance (caught via 'catch (X e)'): e.message / e.type ──
        if (ma.Target is FabVariable errv && _varTypes.TryGetValue(errv.Name, out var errType) && IsErrorCppType(errType))
        {
            return ma.Member switch
            {
                "message" => $"fab_str({errv.Name}.what())",
                "type" => $"fab_str({errv.Name}.type_name)",
                "to_str" => $"fab_str({errv.Name}.what())",
                _ => throw new Exception($"Error object has no member '{ma.Member}'"),
            };
        }

        // ── std::vector<ClassName> members: ts.length, ts.add(...), ts[i].method() ──
        if (ma.Target is FabVariable vv && _varTypes.ContainsKey(vv.Name + "[]"))
        {
            string vecElem = _varTypes[vv.Name + "[]"];
            var vargs = ma.Args?.Select(Expr).ToList() ?? new List<string>();
            string va0 = vargs.Count > 0 ? vargs[0] : "";
            return ma.Member switch
            {
                "length" => $"fab_num((double){vv.Name}.size())",
                "add" => $"([&](){{ {vv.Name}.insert({vv.Name}.begin(), {ExprNewAsClass(ma.Args?[0] ?? new FabNullLit(), vecElem)}); return FAB_NIL; }}())",
                "addend" => $"([&](){{ {vv.Name}.push_back({ExprNewAsClass(ma.Args?[0] ?? new FabNullLit(), vecElem)}); return FAB_NIL; }}())",
                "clear" => $"([&](){{ {vv.Name}.clear(); return FAB_NIL; }}())",
                "first" => $"{vv.Name}.front()",
                "last" => $"{vv.Name}.back()",
                "remove" => $"([&](){{ {vv.Name}.erase({vv.Name}.begin()+(int)({va0}).d); return FAB_NIL; }}())",
                "reverse" => $"([&](){{ std::reverse({vv.Name}.begin(), {vv.Name}.end()); return FAB_NIL; }}())",
                _ => $"FAB_NIL/*vec.{ma.Member}*/",
            };
        }

        // ── user-defined class instance: bk.Info() → Bank_Info(&bk, ...) ──
        if (ma.Target is FabVariable cv && _varTypes.TryGetValue(cv.Name, out var className))
        {
            if (_classInfo.TryGetValue(className, out var clDef))
            {
                // Private member access check (mirrors interpreter EvalInstance)
                var fieldDef2 = clDef.Fields.FirstOrDefault(f => f.Name == ma.Member);
                if (fieldDef2 != null && fieldDef2.Visibility == "private"
                    && _currentClass != className)
                    throw new Exception($"Field '{ma.Member}' is private in class '{className}'");

                bool isMethod = clDef.Methods.ContainsKey(ma.Member);
                if (isMethod && clDef.Methods[ma.Member].Visibility == "private"
                    && _currentClass != className)
                    throw new Exception($"Method '{ma.Member}' is private in class '{className}'");

                // Field read (no args, no matching method)
                if (!isMethod && ma.Args == null)
                    return $"{cv.Name}.{ma.Member}";

                // Method call
                var callArgs = ma.Args?.Select(Expr).ToList() ?? new List<string>();
                string sep = callArgs.Count > 0 ? ", " : "";
                return $"{className}_{ma.Member}(&{cv.Name}{sep}{string.Join(", ", callArgs)})";
            }
        }

        string tgt = Expr(ma.Target);
        var args = ma.Args?.Select(Expr).ToList() ?? new List<string>();
        string a0 = args.Count > 0 ? args[0] : "FAB_NIL";
        string a1 = args.Count > 1 ? args[1] : "FAB_NIL";

        return ma.Member switch
        {
            // ── Universal: length (property for List, Dict, String, Tuple) ────
            "length" => $"fab_member_length({tgt})",

            // ── String: only length and newline are properties ─────────────────
            "newline" => $"fab_str(\"\\n\")",
            // String methods (require parentheses)
            "first" => $"(({tgt}).tag==FabTag::String?fab_str_first({tgt}):(({tgt}).tag==FabTag::List?({tgt}).list->items[0]:FAB_NIL))",
            "last" => $"(({tgt}).tag==FabTag::String?fab_str_last({tgt}):(({tgt}).tag==FabTag::List?({tgt}).list->items.back():FAB_NIL))",
            "to_lower" => $"(({tgt}).tag==FabTag::Char?fab_char_to_lower({tgt}):fab_str_to_lower({tgt}))",
            "to_upper" => $"(({tgt}).tag==FabTag::Char?fab_char_to_upper({tgt}):fab_str_to_upper({tgt}))",
            "to_str" => $"(({tgt}).tag==FabTag::Char?fab_char_to_str({tgt}):fab_str(fab_fmt({tgt})))",
            "is_digit" => $"(({tgt}).tag==FabTag::Char?fab_bool_v(std::isdigit((unsigned char)({tgt}).c)!=0):fab_str_isdigit({tgt}))",
            "is_empty" => $"(({tgt}).tag==FabTag::String?fab_bool_v(({tgt}).s.empty()):FAB_NIL)",
            "is_null_or_empty" => $"fab_bool_v(({tgt}).tag==FabTag::Null||({tgt}).s.empty())",
            "is_null_or_space" => $"fab_str_is_null_or_space({tgt})",
            "normalize" => $"fab_str_normalize({tgt})",
            "is_normalize" => $"fab_str_is_normalize({tgt})",
            "replace" => $"fab_str_replace({tgt},{a0},{a1})",
            "substr" => args.Count >= 2 ? $"fab_str_substr({tgt},(int)({a0}).d,(int)({a1}).d)" : $"fab_str_substr({tgt},(int)({a0}).d,-1)",
            "split" => $"fab_str_split({tgt},{a0})",
            "trim" => args.Count >= 1 ? $"fab_str_trim_char({tgt},{a0})" : $"fab_str_trim({tgt})",
            "trim_end" => $"fab_str_trim_end({tgt},{a0})",
            "trim_start" => $"fab_str_trim_start({tgt},{a0})",
            "repeat" => $"fab_str_repeat({tgt},{a0},{a1})",
            "index" => $"(({tgt}).tag==FabTag::List?([&](){{auto& __l=({tgt}).list->items;for(int __i=0;__i<(int)__l.size();__i++)if(fab_eq(__l[__i],{a0}))return fab_num((double)__i);return fab_num(-1.0);}})():fab_str_index({tgt},{a0}))",
            "contains" => $"(({tgt}).tag==FabTag::List?fab_list_contains(({tgt}).list,{a0}):fab_str_contains({tgt},{a0}))",

            // ── List methods (require parentheses) ────────────────────────────
            "add" => $"(fab_list_push_front(({tgt}).list,{a0}),FAB_NIL)",
            "addend" => $"(fab_list_push(({tgt}).list,{a0}),FAB_NIL)",
            "remove" => $"(({tgt}).tag==FabTag::List?([&](){{({tgt}).list->items.erase(({tgt}).list->items.begin()+(int)({a0}).d);return FAB_NIL;}})():(({tgt}).dict->pairs.erase(({tgt}).dict->pairs.begin()+(int)({a0}).d),FAB_NIL))",
            "clear" => $"(({tgt}).tag==FabTag::List?(({tgt}).list->items.clear(),FAB_NIL):(({tgt}).tag==FabTag::Dict?(({tgt}).dict->pairs.clear(),FAB_NIL):FAB_NIL))",
            "sort" => $"(fab_list_sort(({tgt}).list),FAB_NIL)",
            "reverse" => $"([&](){{std::reverse(({tgt}).list->items.begin(),({tgt}).list->items.end());return FAB_NIL;}}())",

            // ── Dict methods (require parentheses) ────────────────────────────
            "get_key" => $"(({tgt}).dict->pairs[(int)({a0}).d].first)",
            "get_value" => $"(({tgt}).dict->pairs[(int)({a0}).d].second)",
            "keys" => $"fab_dict_keys(({tgt}).dict)",
            "values" => $"fab_dict_values(({tgt}).dict)",

            // ── Char: all properties (no parentheses) ─────────────────────────
            "is_lower" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::islower((unsigned char)({tgt}).c))",
            "is_upper" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::isupper((unsigned char)({tgt}).c))",
            "is_space" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::isspace((unsigned char)({tgt}).c))",
            "is_number" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::isdigit((unsigned char)({tgt}).c))",
            "is_symbol" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&issymbol_compat((unsigned char)({tgt}).c))",
            "is_punctuation" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::ispunct((unsigned char)({tgt}).c))",
            "is_letter" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::isalpha((unsigned char)({tgt}).c))",
            "isdigit" => $"fab_bool_v(({tgt}).tag==FabTag::Char&&std::isdigit((unsigned char)({tgt}).c))",
            "ascii" => $"(({tgt}).tag==FabTag::Char?fab_num((double)(unsigned char)({tgt}).c):FAB_NIL)",

            // Numeric: boolean methods (require parentheses)
            "is_even" => $"fab_num_is_even({tgt})",
            "iseven" => $"fab_num_is_even({tgt})",
            "is_odd" => $"fab_bool_v((long long)({tgt}).d%2!=0)",
            "is_positive" => $"fab_num_is_positive({tgt})",
            "ispositive" => $"fab_num_is_positive({tgt})",
            "is_negative" => $"fab_num_is_negative({tgt})",
            "isnegative" => $"fab_num_is_negative({tgt})",
            "is_zero" => $"fab_bool_v(({tgt}).d==0.0)",
            "is_even_integer" => $"fab_num_is_even_integer({tgt})",
            "is_odd_integer" => $"fab_num_is_odd_integer({tgt})",
            "is_pow2" => $"fab_num_is_pow2({tgt})",
            "is_pow3" => $"fab_num_is_pow3({tgt})",
            "is_integer" => $"fab_bool_v(({tgt}).d==std::floor(({tgt}).d)&&!std::isinf(({tgt}).d))",
            "is_nan" => $"fab_bool_v(std::isnan(({tgt}).d))",
            "is_infinity" => $"fab_bool_v(std::isinf(({tgt}).d))",

            // ── Bool ──────────────────────────────────────────────────────────
            "not" => $"fab_bool_v(!fab_truthy({tgt}))",

            // ── Tuple index access ────────────────────────────────────────────
            _ when int.TryParse(ma.Member, out int idx) => $"({tgt}).tuple_items[{idx}]",

            _ => args.Count > 0 ? $"FAB_NIL/*{ma.Member}*/" : $"({tgt}).{ma.Member}",
        };
    }

    private string ExprIndex(FabIndexAccess ia)
    {
        // std::vector<ClassName> case: varName[i] → varName[(int)(k).d]
        if (ia.Target is FabVariable iv && _varTypes.ContainsKey(iv.Name + "[]"))
        {
            string k = Expr(ia.Index);
            return $"{iv.Name}[(int)({k}).d]";
        }
        string t = Expr(ia.Target), ki = Expr(ia.Index);
        return $"(({t}).tag==FabTag::List?fab_list_get(({t}).list,(int)({ki}).d):(({t}).tag==FabTag::Dict?fab_dict_get(({t}).dict,{ki}):(({t}).tag==FabTag::String?fab_char_v(({t}).s[(int)({ki}).d]):FAB_NIL)))";
    }

    private string ExprLibCall(FabLibCall lc)
    {
        var args = lc.Args?.Select(Expr).ToList() ?? new List<string>();
        string a(int i) => i < args.Count ? args[i] : "FAB_NIL";

        return (lc.LibName, lc.FuncName) switch
        {
            // ── math ──────────────────────────────────────────────────────────
            ("math", "pow") => $"fab_math_pow({a(0)},{a(1)})",
            ("math", "sqrt") => $"fab_math_sqrt({a(0)})",
            ("math", "cbrt") => $"fab_math_cbrt({a(0)})",
            ("math", "abs") => $"fab_math_abs({a(0)})",
            ("math", "floor") => $"fab_math_floor({a(0)})",
            ("math", "ceil") => $"fab_math_ceil({a(0)})",
            ("math", "round") => $"fab_math_round({a(0)})",
            ("math", "sin") => $"fab_math_sin({a(0)})",
            ("math", "cos") => $"fab_math_cos({a(0)})",
            ("math", "tan") => args.Count == 2 ? $"fab_math_tan2({a(0)},{a(1)})" : $"fab_math_tan({a(0)})",
            ("math", "log") => args.Count == 2 ? $"fab_math_log2v({a(0)},{a(1)})" : $"fab_math_log1({a(0)})",
            ("math", "max") => $"fab_math_max({a(0)},{a(1)})",
            ("math", "min") => $"fab_math_min({a(0)},{a(1)})",
            ("math", "clamp") => $"fab_math_clamp({a(0)},{a(1)},{a(2)})",
            ("math", "rad") => $"fab_math_rad({a(0)})",
            ("math", "deg") => $"fab_math_deg({a(0)})",
            ("math", "exp") => $"fab_math_exp({a(0)})",
            ("math", "expm") => $"fab_math_expm({a(0)})",
            ("math", "atan2") => $"fab_math_atan2v({a(0)},{a(1)})",
            ("math", "sing") => $"fab_math_sign({a(0)})",
            ("math", "lerp") => $"fab_math_lerp({a(0)},{a(1)},{a(2)})",
            ("math", "hypot") => $"fab_math_hypot({a(0)},{a(1)})",
            ("math", "factorial") => $"fab_math_factorial({a(0)})",
            ("math", "factoriall") => $"fab_math_factoriall({a(0)})",
            ("math", "factoriald") => $"fab_math_factoriald({a(0)})",
            ("math", "factorialld") => $"fab_math_factorialld({a(0)})",
            ("math", "gamma") => $"fab_math_gamma({a(0)})",
            ("math", "log_gamma") => $"fab_math_log_gamma({a(0)})",
            ("math", "beta") => $"fab_math_beta({a(0)},{a(1)})",
            ("math", "betal") => $"fab_math_betal({a(0)},{a(1)})",
            ("math", "pi") => "fab_math_pi()",
            ("math", "e") => "fab_math_e()",
            ("math", "gam") => "fab_math_gam()",
            ("math", "g") => "fab_math_g()",
            ("math", "tau") => "fab_math_tau()",
            ("math", "phi") => "fab_math_phi()",
            ("math", "sqrt2") => "fab_math_sqrt2()",
            ("math", "negativ_zero") => "fab_num(-0.0)",

            // ── random ────────────────────────────────────────────────────────
            ("random", "randi") => $"fab_random_randi({a(0)},{a(1)})",
            ("random", "randl") => $"fab_random_randl({a(0)},{a(1)})",
            ("random", "randb") => $"fab_random_randb({a(0)},{a(1)})",
            ("random", "rands") => $"fab_random_rands({a(0)},{a(1)})",
            ("random", "randd") => $"fab_random_randd({a(0)},{a(1)})",
            ("random", "randf") => $"fab_random_randf({a(0)},{a(1)})",

            // ── date ──────────────────────────────────────────────────────────
            ("date", "date_now") => "fab_date_date_now()",
            ("date", "time_now") => "fab_date_time_now()",
            ("date", "day_now") => "fab_date_day_now()",
            ("date", "month_now") => "fab_date_month_now()",
            ("date", "year_now") => "fab_date_year_now()",
            ("date", "sec_now") => "fab_date_sec_now()",
            ("date", "min_now") => "fab_date_min_now()",
            ("date", "hours_now") => "fab_date_hours_now()",
            ("date", "now") => $"fab_date_now({a(0)})",
            ("date", "max_value") => "fab_date_max_value()",
            ("date", "min_value") => "fab_date_min_value()",

            // ── io ────────────────────────────────────────────────────────────
            ("io", "read_file") => $"fab_io_read_file({a(0)})",
            ("io", "write_file") => $"fab_io_write_file({a(0)},{a(1)})",
            ("io", "append_file") => $"fab_io_append_file({a(0)},{a(1)})",
            ("io", "delete_file") => $"fab_io_delete_file({a(0)})",
            ("io", "file_exists") => $"fab_io_file_exists({a(0)})",
            ("io", "read_lines") => $"fab_io_read_lines({a(0)})",
            ("io", "write_lines") => $"fab_io_write_lines({a(0)},{a(1)})",
            ("io", "copy_file") => $"fab_io_copy_file({a(0)},{a(1)})",
            ("io", "move_file") => $"fab_io_move_file({a(0)},{a(1)})",
            ("io", "dir_exists") => $"fab_io_dir_exists({a(0)})",
            ("io", "create_dir") => $"fab_io_create_dir({a(0)})",
            ("io", "delete_dir") => $"fab_io_delete_dir({a(0)})",
            ("io", "list_files") => $"fab_io_list_files({a(0)})",
            ("io", "list_dirs") => $"fab_io_list_dirs({a(0)})",
            ("io", "path_join") => $"fab_io_path_join({a(0)},{a(1)})",
            ("io", "get_ext") => $"fab_io_get_ext({a(0)})",
            ("io", "get_name") => $"fab_io_get_name({a(0)})",
            ("io", "file_size") => $"fab_io_file_size({a(0)})",

            // ── console ───────────────────────────────────────────────────────
            ("console", "clear") => "fab_console_clear()",
            ("console", "set_fg") => $"fab_console_set_fg({a(0)})",
            ("console", "set_bg") => $"fab_console_set_bg({a(0)})",
            ("console", "reset_color") => "fab_console_reset_color()",
            ("console", "set_title") => $"fab_console_set_title({a(0)})",
            ("console", "beep") => "fab_console_beep()",
            ("console", "set_cursor") => $"fab_console_set_cursor({a(0)},{a(1)})",
            ("console", "get_cursor_x") => "fab_console_get_cursor_x()",
            ("console", "get_cursor_y") => "fab_console_get_cursor_y()",
            ("console", "get_cursor_position") => "fab_make_tuple(2,fab_console_get_cursor_x(),fab_console_get_cursor_y())",
            ("console", "hide_cursor") => "fab_console_hide_cursor()",
            ("console", "show_cursor") => "fab_console_show_cursor()",
            ("console", "width") => "fab_console_width()",
            ("console", "height") => "fab_console_height()",
            ("console", "bold") => $"fab_console_bold({a(0)})",
            ("console", "underline") => $"fab_console_underline({a(0)})",
            ("console", "blink") => $"fab_console_blink({a(0)})",

            // ── environment ───────────────────────────────────────────────────
            ("environment", "exit") => $"fab_env_exit({a(0)})",
            ("environment", "sdelay") => $"fab_env_sdelay({a(0)})",
            ("environment", "mdelay") => $"fab_env_mdelay({a(0)})",
            ("environment", "machine_name") => "fab_env_machine_name()",
            ("environment", "new_line") => "fab_env_new_line()",
            ("environment", "stack_trace") => "fab_env_stack_trace()",
            ("environment", "exit_code") => "fab_env_exit_code()",
            ("environment", "os_ver") => "fab_env_os_ver()",
            ("environment", "os_name") => "fab_env_os_name()",
            ("environment", "current_directory") => "fab_env_current_directory()",
            ("environment", "com_line") => "fab_env_com_line()",
            ("environment", "process_id") => "fab_env_process_id()",
            ("environment", "process_count") => "fab_env_process_count()",
            ("environment", "username") => "fab_env_username()",
            ("environment", "user_interactive") => "fab_env_user_interactive()",
            ("environment", "tick_count") => "fab_env_tick_count()",
            ("environment", "working_set") => "fab_env_working_set()",
            ("environment", "compute") => $"fab_env_compute({a(0)},{a(1)})",
            ("environment", "service_pack") => "fab_env_service_pack()",
            ("environment", "process_path") => "fab_env_process_path()",
            ("environment", "user_domain_name") => "fab_env_user_domain_name()",

            // ── Convert ───────────────────────────────────────────────────────
            ("Convert", "to_int") => $"fab_convert_to_int({a(0)})",
            ("Convert", "to_short") => $"fab_convert_to_short({a(0)})",
            ("Convert", "to_long") => $"fab_convert_to_long({a(0)})",
            ("Convert", "to_byte") => $"fab_convert_to_byte({a(0)})",
            ("Convert", "to_uint") => $"fab_convert_to_uint({a(0)})",
            ("Convert", "to_ushort") => $"fab_convert_to_ushort({a(0)})",
            ("Convert", "to_ulong") => $"fab_convert_to_ulong({a(0)})",
            ("Convert", "to_ubyte") => $"fab_convert_to_ubyte({a(0)})",
            ("Convert", "to_float") => $"fab_convert_to_float({a(0)})",
            ("Convert", "to_double") => $"fab_convert_to_double({a(0)})",
            ("Convert", "to_bool") => $"fab_convert_to_bool({a(0)})",
            ("Convert", "to_str") => $"fab_convert_to_str({a(0)})",
            ("Convert", "to_bin") => $"fab_convert_to_bin({a(0)})",
            ("Convert", "to_hex") => $"fab_convert_to_hex({a(0)})",
            ("Convert", "to_oct") => $"fab_convert_to_oct({a(0)})",
            ("Convert", "from_bin") => $"fab_convert_from_bin({a(0)})",
            ("Convert", "from_hex") => $"fab_convert_from_hex({a(0)})",
            ("Convert", "from_oct") => $"fab_convert_from_oct({a(0)})",
            ("Convert", "to_char") => $"fab_convert_to_char({a(0)})",
            ("Convert", "to_ascii") => $"fab_convert_to_ascii({a(0)})",

            // ── Color ─────────────────────────────────────────────────────────
            ("Color", "red") => "fab_color_red()",
            ("Color", "green") => "fab_color_green()",
            ("Color", "blue") => "fab_color_blue()",
            ("Color", "white") => "fab_color_white()",
            ("Color", "black") => "fab_color_black()",
            ("Color", "yellow") => "fab_color_yellow()",
            ("Color", "cyan") => "fab_color_cyan()",
            ("Color", "magenta") => "fab_color_magenta()",
            ("Color", "orange") => "fab_color_orange()",
            ("Color", "purple") => "fab_color_purple()",
            ("Color", "gray") => "fab_color_gray()",
            ("Color", "transparent") => "fab_color_transparent()",
            ("Color", "from_hex") => $"fab_color_from_hex({a(0)})",
            ("Color", "to_hex") => $"fab_color_to_hex({a(0)})",
            ("Color", "lerp") => $"fab_color_lerp({a(0)},{a(1)},{a(2)})",
            ("Color", "invert") => $"fab_color_invert({a(0)})",
            ("Color", "with_alpha") => $"fab_color_with_alpha({a(0)},{a(1)})",
            ("Color", "get_r") => $"fab_color_get_r({a(0)})",
            ("Color", "get_g") => $"fab_color_get_g({a(0)})",
            ("Color", "get_b") => $"fab_color_get_b({a(0)})",
            ("Color", "get_a") => $"fab_color_get_a({a(0)})",
            ("Color", "to_str") => $"fab_color_to_str({a(0)})",

            // ── ipaddress ─────────────────────────────────────────────────────
            ("ipaddress", "parse") => $"fab_ipaddress_parse({a(0)})",
            ("ipaddress", "loopback") => "fab_ipaddress_loopback()",
            ("ipaddress", "broadcast") => "fab_ipaddress_broadcast()",
            ("ipaddress", "none") => "fab_ipaddress_none()",
            ("ipaddress", "IPv6_any") => "fab_ipaddress_ipv6_any()",
            ("ipaddress", "IPv6_loopback") => "fab_ipaddress_ipv6_loopback()",
            ("ipaddress", "IPv6_none") => "fab_ipaddress_ipv6_none()",
            ("ipaddress", "is_loopback") => $"fab_ipaddress_is_loopback({a(0)})",
            ("ipaddress", "host_to_network_order") => $"fab_ipaddress_host_to_network_order({a(0)})",
            ("ipaddress", "network_to_host_order") => $"fab_ipaddress_network_to_host_order({a(0)})",

            // ── regex ─────────────────────────────────────────────────────────
            ("regex", "is_match") => $"fab_regex_is_match({a(0)},{a(1)},{a(2)})",
            ("regex", "match") => $"fab_regex_match({a(0)},{a(1)},{a(2)})",
            ("regex", "find_all") => $"fab_regex_find_all({a(0)},{a(1)},{a(2)})",
            ("regex", "replace") => $"fab_regex_replace({a(0)},{a(1)},{a(2)},{a(3)})",
            ("regex", "split") => $"fab_regex_split({a(0)},{a(1)},{a(2)})",
            ("regex", "groups") => $"fab_regex_groups({a(0)},{a(1)},{a(2)})",
            ("regex", "escape") => $"fab_regex_escape({a(0)})",

            _ => $"FAB_NIL/*{lc.LibName}.{lc.FuncName}*/",
        };
    }

    /// <summary>
    /// Emit an expression that produces a value of type <paramref name="className"/>.
    /// If <paramref name="node"/> is a FabNew for that class, emit the ctor inline.
    /// Otherwise fall back to generic Expr().
    /// Used when pushing elements into std::vector&lt;ClassName&gt;.
    /// </summary>
    private string ExprNewAsClass(FabBase node, string className)
    {
        if (node is FabNew fn && ResolveClassName(fn.ClassName) == className)
        {
            string tmp = T();
            string ctorArgs = string.Join(", ", fn.Args.Select(Expr));
            string sep = ctorArgs.Length > 0 ? ", " : "";
            if (_classInfo.TryGetValue(className, out var clDef) && clDef.Constructor != null)
                return $"([&]() -> {className} {{ {className} {tmp}{{}}; {className}_{className}(&{tmp}{sep}{ctorArgs}); return {tmp}; }}())";
            return $"{className}{{}}";
        }
        // Already a variable of the right type (e.g. passing existing instance)
        return Expr(node);
    }

    private string ExprNew(FabNew fn)
    {
        // ── Native error types: new TypeError("msg") etc. ──────────────────
        // Produces a real C++ object (see fab_runtime.h's FabError hierarchy),
        // not a FabVal — checked first so it can never be shadowed by a
        // same-named user class (separate namespace, mirrors the interpreter).
        if (_errorTypeCppNames.TryGetValue(fn.ClassName, out var errCpp))
        {
            if (fn.Args.Count != 1)
                throw new Exception($"'{fn.ClassName}' expects 1 argument (message), got {fn.Args.Count}");
            return $"{errCpp}(fab_fmt({Expr(fn.Args[0])}))";
        }

        // ── Color(r, g, b) / Color(r, g, b, a) ───────────────────────────────
        if (fn.ClassName == "Color")
        {
            if (fn.Args.Count == 3)
                return $"fab_color_new({Expr(fn.Args[0])},{Expr(fn.Args[1])},{Expr(fn.Args[2])})";
            if (fn.Args.Count == 4)
                return $"fab_color_new4({Expr(fn.Args[0])},{Expr(fn.Args[1])},{Expr(fn.Args[2])},{Expr(fn.Args[3])})";
        }

        string? resolvedClass = ResolveClassName(fn.ClassName);
        if (resolvedClass != null)
        {
            if (_classInfo.TryGetValue(resolvedClass, out var ctorClassDef) && ctorClassDef.IsStatic)
                throw new Exception($"Cannot create an instance of static class '{resolvedClass}'");

            string tmp = T();
            string ctorArgs = string.Join(", ", fn.Args.Select(Expr));
            string sep = ctorArgs.Length > 0 ? ", " : "";
            return $"([&]() -> {resolvedClass} {{ {resolvedClass} {tmp}{{}}; {resolvedClass}_{resolvedClass}(&{tmp}{sep}{ctorArgs}); return {tmp}; }}())";
        }
        return $"({fn.ClassName}{{}})";  // plain struct zero-init
    }

    private string ExprCast(FabCast fc)
    {
        string inner = Expr(fc.Expr);
        return fc.TargetType switch
        {
            "int" or "short" or "long" or "uint" or "ushort" or "ulong" or "byte" or "sbyte"
                => $"fab_num((double)(long long)({inner}).d)",
            "float" or "double" => inner,
            "string" => $"fab_str(fab_fmt({inner}))",
            "bool" => $"fab_bool_v(fab_truthy({inner}))",
            "char" => $"fab_char_v((char)(int)({inner}).d)",
            _ => inner,
        };
    }

    private string ExprIs(FabIs fi)
    {
        string v = Expr(fi.Expr);
        return fi.TypeName switch
        {
            "int" or "short" or "long" or "uint" or "ushort" or "ulong" or "byte" or "sbyte"
                => $"fab_bool_v(({v}).tag==FabTag::Double&&(double)(long long)({v}).d==({v}).d)",
            "float" or "double" => $"fab_bool_v(({v}).tag==FabTag::Double)",
            "bool" => $"fab_bool_v(({v}).tag==FabTag::Bool)",
            "char" => $"fab_bool_v(({v}).tag==FabTag::Char)",
            "string" => $"fab_bool_v(({v}).tag==FabTag::String)",
            "list" => $"fab_bool_v(({v}).tag==FabTag::List)",
            "dict" => $"fab_bool_v(({v}).tag==FabTag::Dict)",
            "tuple" => $"fab_bool_v(({v}).tag==FabTag::Tuple)",
            "null" => $"fab_bool_v(({v}).tag==FabTag::Null)",
            "numeric" => $"fab_bool_v(({v}).tag==FabTag::Double)",
            "ptr" => $"fab_bool_v(({v}).tag==FabTag::Ptr)",
            _ => "FAB_FALSE",
        };
    }

    private string ExprIn(FabIn fi)
    {
        string item = Expr(fi.Item), col = Expr(fi.Collection);
        return
            $"(({col}).tag==FabTag::List?fab_list_contains(({col}).list,{item}):" +
            $"(({col}).tag==FabTag::Dict?fab_bool_v(fab_dict_find(({col}).dict,{item})>=0):" +
            $"(({col}).tag==FabTag::String?fab_str_contains({col},{item}):" +
            $"(({col}).tag==FabTag::Tuple?" +
            $"([&](){{for(auto& __ti:{col}.tuple_items)if(fab_eq(__ti,{item}))return FAB_TRUE;return FAB_FALSE;}})():" +
            $"FAB_FALSE))))";
    }

    private string ExprListLit(FabListLiteral ll)
    {
        if (ll.Items.Count == 0) return $"fab_make_list({ll.Capacity},0)";
        return $"fab_make_list({ll.Capacity},{ll.Items.Count},{string.Join(",", ll.Items.Select(Expr))})";
    }

    private string ExprDictLit(FabDictLiteral dl)
    {
        if (dl.Pairs.Count == 0) return $"fab_make_dict({dl.Capacity},0)";
        var kv = string.Join(",", dl.Pairs.Select(p => $"{Expr(p.Key)},{Expr(p.Value)}"));
        return $"fab_make_dict({dl.Capacity},{dl.Pairs.Count},{kv})";
    }

    private string ExprTupleLit(FabTupleLiteral tl)
    {
        if (tl.Items.Count == 0) return "fab_make_tuple(0)";
        return $"fab_make_tuple({tl.Items.Count},{string.Join(",", tl.Items.Select(Expr))})";
    }

    private string ExprTypeMethod(FabTypeMethod tm) => (tm.TypeName, tm.Member) switch
    {
        ("int", "max_value") => "fab_num(2147483647.0)",
        ("int", "min_value") => "fab_num(-2147483648.0)",
        ("short", "max_value") => "fab_num(32767.0)",
        ("short", "min_value") => "fab_num(-32768.0)",
        ("long", "max_value") => "fab_num(9223372036854775807.0)",
        ("long", "min_value") => "fab_num(-9223372036854775808.0)",
        ("uint", "max_value") => "fab_num(4294967295.0)",
        ("uint", "min_value") => "fab_num(0.0)",
        ("ushort", "max_value") => "fab_num(65535.0)",
        ("ushort", "min_value") => "fab_num(0.0)",
        ("ulong", "max_value") => "fab_num(18446744073709551615.0)",
        ("ulong", "min_value") => "fab_num(0.0)",
        ("byte", "max_value") => "fab_num(255.0)",
        ("byte", "min_value") => "fab_num(0.0)",
        ("sbyte", "max_value") => "fab_num(127.0)",
        ("sbyte", "min_value") => "fab_num(-128.0)",
        ("float", "max_value") => "fab_num(3.4028234663852886e+38)",
        ("float", "min_value") => "fab_num(-3.4028234663852886e+38)",
        ("double", "max_value") => "fab_num(1.7976931348623157e+308)",
        ("double", "min_value") => "fab_num(-1.7976931348623157e+308)",
        (_, "parse") when tm.Args?.Count > 0 => $"fab_parse_input(fab_str(fab_fmt({Expr(tm.Args[0])})))",
        _ => "FAB_NIL",
    };

    // type(varName) — runtime type introspection
    private string ExprType(FabType ft)
    {
        string v = ft.VarName;
        return
            $"([&]() -> FabVal {{" +
            $" FabVal& __tv = {v};" +
            $" switch (__tv.tag) {{" +
            $" case FabTag::Null:   return fab_str(\"null\");" +
            $" case FabTag::Bool:   return fab_str(\"bool\");" +
            $" case FabTag::Double: return ((double)(long long)__tv.d == __tv.d) ? fab_str(\"int\") : fab_str(\"double\");" +
            $" case FabTag::String: return fab_str(\"string\");" +
            $" case FabTag::Char:   return fab_str(\"char\");" +
            $" case FabTag::List:   return fab_str(\"vfree[]\");" +
            $" case FabTag::Dict:   return fab_str(\"(vfree, vfree){{}}\");" +
            $" case FabTag::Tuple:  return fab_str(\"tuple\");" +
            $" case FabTag::Ptr:    return fab_str(\"ptr\");" +
            $" default:             return fab_str(\"unknown\");" +
            $" }}" +
            $" }}())";
    }

    // size_of(varName) — mirrors interpreter FabSizeOf.Eval
    private string SizeOfExpr(FabSizeOf fs)
    {
        string v = fs.VarName;
        // If it's a known class type, return field count at compile time
        if (_varTypes.TryGetValue(v, out var cls) && _classInfo.TryGetValue(cls, out var cd))
            return $"fab_num({cd.Fields.Count}.0)";
        // Class names directly (size_of(ClassName) → field count)
        if (_classInfo.TryGetValue(v, out var cd2))
            return $"fab_num({cd2.Fields.Count}.0)";
        return
            $"([&](){{" +
            $"auto& __sv={v};" +
            $"if(__sv.tag==FabTag::List)return fab_num(__sv.list->limit>=0?(double)__sv.list->limit:(double)__sv.list->items.size());" +
            $"if(__sv.tag==FabTag::Dict)return fab_num(__sv.dict->limit>=0?(double)__sv.dict->limit:(double)__sv.dict->pairs.size());" +
            $"if(__sv.tag==FabTag::Tuple)return fab_num((double)__sv.tuple_items.size());" +
            $"if(__sv.tag==FabTag::String)return fab_num((double)__sv.s.size());" +
            $"return FAB_NIL;" +
            $"}})()";
    }

    private string ExprFStr(FabFString fs)
    {
        var parts = new List<string>();
        string t = fs.Template;
        int i = 0;
        while (i < t.Length)
        {
            int open = t.IndexOf('{', i);
            if (open < 0) { if (i < t.Length) parts.Add($"fab_str({CStr(t[i..])})"); break; }
            if (open > i) parts.Add($"fab_str({CStr(t[i..open])})");
            int close = t.IndexOf('}', open);
            if (close < 0) { parts.Add($"fab_str({CStr(t[open..])})"); break; }

            // Parse the inner expression through the full Expr() pipeline so that
            // class method calls like bk.Rate() are correctly dispatched to Bank_Rate(&bk)
            string innerText = t[(open + 1)..close].Trim();
            string innerCode;
            try
            {
                var tokens = new Lexer().Lex(innerText).ToList();
                var innerAst = new Parser(tokens).ParseExpr();
                innerCode = Expr(innerAst);
            }
            catch
            {
                // Fallback: emit raw text (will likely fail to compile, but preserves old behaviour)
                innerCode = innerText;
            }

            parts.Add($"fab_str(fab_fmt({innerCode}))");
            i = close + 1;
        }
        if (parts.Count == 0) return "fab_str(\"\")";
        string result = parts[0];
        for (int j = 1; j < parts.Count; j++) result = $"fab_concat({result},{parts[j]})";
        return result;
    }

    // ══════════════════════════════════════════════════════════════════════════
    //  Low-level helpers
    // ══════════════════════════════════════════════════════════════════════════

    // In C++ we use empty parameter list for zero-arg functions, not "void"
    private static string FuncParms(List<(string Type, string Name)> parms)
        => parms.Count == 0 ? "" : string.Join(", ", parms.Select(p => $"FabVal {p.Name}"));

    private static string CName(string name) => name == "main" ? "fab_main" : name;

    /// <summary>
    /// ParseTypeString() lowercases all type names (including user-defined classes).
    /// This resolves a lowercased type string back to the original class name,
    /// or returns null if it is not a known user class.
    /// Example: "vec3" → "Vec3",  "fabval" → null
    /// </summary>
    private string? ResolveClassName(string? typeName)
    {
        if (typeName == null) return null;
        // Exact match first (for already-correct casing)
        if (_classNames.Contains(typeName)) return typeName;
        // Lowercase lookup (handles ParseTypeString lowercasing)
        if (_classNamesLower.TryGetValue(typeName.ToLower(), out var orig)) return orig;
        return null;
    }

    private static string CStr(string s)
    {
        var sb = new StringBuilder("\"");
        foreach (char c in s)
            sb.Append(c switch { '"' => "\\\"", '\\' => "\\\\", '\n' => "\\n", '\r' => "\\r", '\t' => "\\t", _ => c.ToString() });
        sb.Append('"');
        return sb.ToString();
    }

    private static string EscCh(char c) => c switch
    { '\'' => "\\'", '\\' => "\\\\", '\n' => "\\n", '\r' => "\\r", '\t' => "\\t", _ => c.ToString() };

    private void L(string text)
    {
        for (int i = 0; i < _indent; i++) _sb.Append(IU);
        _sb.AppendLine(text);
    }

    private void Raw(string text) => _sb.Append(text);
}

// ═══════════════════════════════════════════════════════════════════════════════
//  FabCRunner — orchestrates the full compile pipeline
//  Now targets C++17 using g++ / clang++
// ═══════════════════════════════════════════════════════════════════════════════

public static class FabCRunner
{
    /// <summary>
    /// Transpile a Fab program to a native binary via g++ / clang++.
    /// Returns 0 on success.
    /// </summary>
    public static int Build(
        string cppSource,
        string outDir,
        string baseName,
        bool verbose = false)
    {
        // ── 1. Find C++ compiler ──────────────────────────────────────────────
        string? cc = FindCompiler();
        if (cc == null)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.Error.WriteLine("[fab:build] No C++ compiler found. Install g++ or clang++.");
            Console.ResetColor();
            return 1;
        }

        // ── 2. Write to temp .cpp file ────────────────────────────────────────
        bool isWindows = System.Runtime.InteropServices.RuntimeInformation
            .IsOSPlatform(System.Runtime.InteropServices.OSPlatform.Windows);

        string outExt = isWindows ? ".exe" : "";
        string outPath = Path.Combine(outDir, baseName + outExt);
        string tmpCpp = Path.Combine(
            Path.GetTempPath(),
            $"fab_{baseName}_{Guid.NewGuid():N}.cpp");
        File.WriteAllText(tmpCpp, cppSource);

        // ── 3. Compile ────────────────────────────────────────────────────────
        string winLibs = isWindows ? " -lws2_32" : "";
        string flags = $"\"{tmpCpp}\" -o \"{outPath}\" -O2 -std=c++17{winLibs} " +
                       "-Wno-unused-function -Wno-unused-variable -Wno-unused-but-set-variable";
        if (verbose) PrintStep("c++", $"{cc} {flags}");

        var psi = new System.Diagnostics.ProcessStartInfo
        {
            FileName = cc,
            Arguments = flags,
            RedirectStandardOutput = true,
            RedirectStandardError = true,
            UseShellExecute = false,
        };

        using var proc = System.Diagnostics.Process.Start(psi)
            ?? throw new Exception("Failed to start C++ compiler");

        string stderr = proc.StandardError.ReadToEnd();
        string stdout = proc.StandardOutput.ReadToEnd();
        proc.WaitForExit();
        try { File.Delete(tmpCpp); } catch { }

        if (proc.ExitCode != 0)
        {
            Console.ForegroundColor = ConsoleColor.Red;
            Console.Error.WriteLine("[fab:build error] Compilation failed:");
            Console.ResetColor();
            if (!string.IsNullOrWhiteSpace(stderr)) Console.Error.WriteLine(stderr);
            if (!string.IsNullOrWhiteSpace(stdout)) Console.Error.WriteLine(stdout);
            return proc.ExitCode;
        }

        PrintStep("done", $"Binary: {outPath}");
        return 0;
    }

    // Prefer g++ / clang++, fall back to c++ (common alias on macOS/BSD)
    private static string? FindCompiler()
    {
        foreach (string cc in new[] { "g++", "clang++", "c++" })
        {
            try
            {
                var p = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
                {
                    FileName = cc,
                    Arguments = "--version",
                    RedirectStandardOutput = true,
                    RedirectStandardError = true,
                    UseShellExecute = false,
                });
                p?.WaitForExit();
                if (p?.ExitCode == 0) return cc;
            }
            catch { }
        }
        return null;
    }

    private static void PrintStep(string tag, string msg)
    {
        Console.ForegroundColor = ConsoleColor.DarkCyan;
        Console.Write($"[fab:{tag}] ");
        Console.ResetColor();
        Console.WriteLine(msg);
    }
}