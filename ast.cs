using System.Runtime.InteropServices;
using System.Text.RegularExpressions;
using System.Globalization;
using System.Diagnostics;
using System.Data;
using System.Net;

public class AST
{
    // ── Built-in error types (native, not Fab# classes) ────────────────────────
    // Error/TypeError/ValueError/... are a genuine runtime type, not sugar over
    // FabClasses/FabInstance. 'new TypeError("msg")' produces a FabError value
    // directly (see FabNew.Eval); the C++ compiler mirrors this with a real
    // native struct hierarchy (FabError <- FabErr_TypeError <- ...) in
    // fab_runtime.h, so 'catch (Error e)' polymorphically catches every
    // built-in error subtype in compiled code too — not just in the interpreter.
    public static class FabErrorTypes
    {
        // type name → parent type name (null = root)
        public static readonly Dictionary<string, string?> Hierarchy = new()
        {
            ["Error"] = null,
            ["TypeError"] = "Error",
            ["ValueError"] = "Error",
            ["IndexError"] = "Error",
            ["ArgumentError"] = "Error",
            ["IOError"] = "Error",
            ["NotFoundError"] = "Error",
        };

        public static bool IsKnown(string name) => Hierarchy.ContainsKey(name);

        /// <summary>True if typeName == target or target is an ancestor of typeName.</summary>
        public static bool IsOrDescendantOf(string typeName, string target)
        {
            string? cur = typeName;
            while (cur != null)
            {
                if (cur == target) return true;
                Hierarchy.TryGetValue(cur, out cur);
            }
            return false;
        }
    }

    /// <summary>
    /// Runtime value produced by 'new TypeError("msg")' etc. A first-class
    /// error value — not a FabInstance — carrying its type name and message.
    /// </summary>
    public class FabError
    {
        public string TypeName { get; }
        public string Message { get; }
        public FabError(string typeName, string message) { TypeName = typeName; Message = message; }
        public override string ToString() => Message;
    }

    public abstract class FabBase
    {
        public abstract object Eval(FabInterpreter interpreter);
    }

    // ── Runtime collection types ──────────────────────────────────────────────

    /// <summary>
    /// Fab list: unlimited (capacity == -1) or limited (capacity > 0).
    /// Element type is stored for display/error purposes only — Fab is dynamically
    /// typed at runtime, so we do not enforce element types here.
    /// </summary>
    public class FabList
    {
        public List<object> Items { get; } = new();
        public int Capacity { get; }           // -1 = unlimited
        public string ElementType { get; }

        public FabList(string elementType, int capacity = -1)
        {
            ElementType = elementType;
            Capacity = capacity;
        }

        public void Add(object item)
        {
            if (Capacity != -1 && Items.Count >= Capacity)
                throw new Exception($"List is full (capacity {Capacity})");
            Items.Add(item);
        }

        public void AddFront(object item)
        {
            if (Capacity != -1 && Items.Count >= Capacity)
                throw new Exception($"List is full (capacity {Capacity})");
            Items.Insert(0, item);
        }

        public override string ToString() =>
            "[" + string.Join(", ", Items.Select(AST.FormatValue)) + "]";
    }

    /// <summary>
    /// Fab dictionary: unlimited (capacity == -1) or limited (capacity > 0).
    /// Keys are compared by value using string representation for uniformity.
    /// </summary>
    public class FabDict
    {
        // Use List of pairs so insertion order is preserved and get_key/get_value work by index.
        public List<(object Key, object Value)> Pairs { get; } = new();
        public int Capacity { get; }
        public string KeyType { get; }
        public string ValType { get; }

        public FabDict(string keyType, string valType, int capacity = -1)
        {
            KeyType = keyType; ValType = valType; Capacity = capacity;
        }

        private int FindIndex(object key)
        {
            string ks = AST.FormatValue(key);
            for (int i = 0; i < Pairs.Count; i++)
                if (AST.FormatValue(Pairs[i].Key) == ks) return i;
            return -1;
        }

        public bool ContainsKey(object key) => FindIndex(key) >= 0;

        public object Get(object key)
        {
            int i = FindIndex(key);
            if (i < 0) throw new Exception($"Key '{AST.FormatValue(key)}' not found in dictionary");
            return Pairs[i].Value;
        }

        public void Set(object key, object value)
        {
            if (KeyType == "vfree") key = FabVFreeDecl.InferMinType(key);
            if (ValType == "vfree") value = FabVFreeDecl.InferMinType(value);
            int i = FindIndex(key);
            if (i >= 0) { Pairs[i] = (key, value); return; }
            if (Capacity != -1 && Pairs.Count >= Capacity)
                throw new Exception($"Dictionary is full (capacity {Capacity})");
            Pairs.Add((key, value));
        }

        public override string ToString() =>
            "{" + string.Join(", ", Pairs.Select(p => $"{AST.FormatValue(p.Key)}: {AST.FormatValue(p.Value)}")) + "}";
    }

    // ── Runtime tuple type ───────────────────────────────────────────────────

    /// <summary>
    /// Fab tuple: fixed-length, heterogeneous, immutable sequence of values.
    /// Elements are accessed via t.0, t.1, ... or destructured on assignment.
    /// </summary>
    public class FabTuple
    {
        public object[] Items { get; }

        public FabTuple(object[] items) { Items = items; }

        public object Get(int index)
        {
            if (index < 0 || index >= Items.Length)
                throw new Exception($"Tuple index {index} out of range (length {Items.Length})");
            return Items[index];
        }

        public int Length => Items.Length;

        public override string ToString() =>
            "(" + string.Join(", ", Items.Select(AST.FormatValue)) + ")";
    }

    // ── Runtime closure value (lambda literals) ────────────────────────────────

    /// <summary>
    /// A first-class function value produced by a lambda literal:
    /// '(params) => { ... }' or the 'vfree name(params) => ...' shorthand.
    /// Unlike a regular 'def' function (which runs in a completely fresh,
    /// isolated scope — no access to the caller's or definer's locals), a
    /// closure keeps a reference to the exact lexical frame that was active
    /// where it was created, so it can read (and write) variables from its
    /// enclosing scope. That captured frame is a snapshot (see
    /// ScopeStack's frame-snapshot constructor): later Push()/Pop() calls on
    /// the original enclosing scope never retroactively change what this
    /// closure sees.
    /// </summary>
    public class FabClosure
    {
        public List<string> Params { get; }
        public List<FabBase> Body { get; }
        public ScopeStack CapturedScope { get; }

        public FabClosure(List<string> parms, List<FabBase> body, ScopeStack capturedScope)
        { Params = parms; Body = body; CapturedScope = capturedScope; }

        public override string ToString() => $"<lambda({string.Join(", ", Params)})>";
    }

    // ── Exception for return ──────────────────────────────────────────────────

    public class ReturnException : Exception
    {
        public object Value { get; }
        public ReturnException(object value) : base("return") { Value = value; }
    }

    public class BreakException : Exception
    {
        public BreakException() : base("break") { }
    }

    public class ContinueException : Exception
    {
        public ContinueException() : base("continue") { }
    }

    // ── Exception carrying the original thrown Fab value ──────────────────────
    // Wraps whatever 'throw expr;' evaluated to (string, number, Error instance,
    // etc.) so try/catch can match it against a catch clause's declared type
    // without losing information the way a plain string message would.
    public class FabThrownException : Exception
    {
        public object Value { get; }
        public FabThrownException(object value) : base(AST.FormatValue(value)) { Value = value; }
    }

    // ── Literals ──────────────────────────────────────────────────────────────

    public class FabNumber : FabBase
    {
        public double Value { get; }
        public FabNumber(double value) { Value = value; }
        public override object Eval(FabInterpreter interpreter) => Value;
    }

    public class FabBoolLit : FabBase
    {
        public bool Value { get; }
        public FabBoolLit(bool value) { Value = value; }
        public override object Eval(FabInterpreter interpreter) => Value;
    }

    public class FabString : FabBase
    {
        public string Value { get; }
        public FabString(string value) { Value = value; }
        public override object Eval(FabInterpreter interpreter) => Value;
    }

    public class FabChar : FabBase
    {
        public char Value { get; }
        public FabChar(char value) { Value = value; }
        public override object Eval(FabInterpreter interpreter) => Value;
    }

    // ── null literal ──────────────────────────────────────────────────────────

    /// <summary>
    /// The Fab <c>null</c> literal. Evaluates to C# <c>null</c>.
    /// Distinct from void: null is a valid variable value; void is a function
    /// return marker that should never be stored in a variable.
    /// </summary>
    public class FabNullLit : FabBase
    {
        public override object Eval(FabInterpreter interpreter) => null;
    }

    // ── Pointer registry (global address table) ───────────────────────────────
    // Maps a unique "address" string to a (interpreter, varName) pair.
    // Real memory addresses are not exposed — we simulate pointer semantics
    // with a stable identifier.

    public class FabPointerStore
    {
        // address → (interpreter reference, variable name)
        // We store varName; the interpreter scope holds the actual value.
        private static int _nextAddr = 1;
        private static readonly Dictionary<string, (FabInterpreter Interp, string VarName)> _table = new();

        public static string Alloc(FabInterpreter interp, string varName)
        {
            // Stable: same var in same interpreter always gets the same address
            foreach (var kv in _table)
                if (kv.Value.Interp == interp && kv.Value.VarName == varName)
                    return kv.Key;
            string addr = $"0x{_nextAddr++:X8}";
            _table[addr] = (interp, varName);
            return addr;
        }

        public static object Deref(string addr)
        {
            if (!_table.TryGetValue(addr, out var entry))
                throw new Exception($"Invalid pointer '{addr}': dangling or uninitialized");
            if (!entry.Interp.Scope.TryGet(entry.VarName, out var val))
                throw new Exception($"Pointer '{addr}' refers to variable '{entry.VarName}' which is no longer in scope");
            return val;
        }

        public static void Write(string addr, object value)
        {
            if (!_table.TryGetValue(addr, out var entry))
                throw new Exception($"Invalid pointer '{addr}': dangling or uninitialized");
            if (!entry.Interp.Scope.Contains(entry.VarName))
                throw new Exception($"Pointer '{addr}' refers to variable '{entry.VarName}' which is no longer in scope");
            var slot = entry.Interp.Scope.GetSlot(entry.VarName);
            entry.Interp.Scope.SetSlot(entry.VarName, new VarSlot(value, slot.IsConst, slot.IsLocked, slot.IsFree));
        }
    }

    // ── &varName — взять адрес переменной ────────────────────────────────────
    public class FabAddressOf : FabBase
    {
        public string VarName { get; }
        public FabAddressOf(string varName) { VarName = varName; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.Contains(VarName))
                throw new Exception($"Cannot take address of undefined variable '{VarName}'");
            return FabPointerStore.Alloc(interpreter, VarName);
        }
    }

    // ── *expr — разыменование указателя (чтение) ──────────────────────────────
    public class FabDeref : FabBase
    {
        public FabBase Expr { get; }
        public FabDeref(FabBase expr) { Expr = expr; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Expr.Eval(interpreter);
            if (val is not string addr || !addr.StartsWith("0x"))
                throw new Exception($"Dereference (*) requires a pointer value, got '{FormatValue(val)}'");
            return FabPointerStore.Deref(addr);
        }
    }

    // ── *varName = expr — запись через указатель ──────────────────────────────
    public class FabDerefAssign : FabBase
    {
        public string PtrVarName { get; }
        public FabBase Value { get; }
        public FabDerefAssign(string ptrVarName, FabBase value) { PtrVarName = ptrVarName; Value = value; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.TryGet(PtrVarName, out var ptrVal))
                throw new Exception($"Variable '{PtrVarName}' is not defined");
            if (ptrVal is not string addr || !addr.StartsWith("0x"))
                throw new Exception($"'{PtrVarName}' is not a pointer");
            FabPointerStore.Write(addr, Value.Eval(interpreter));
            return null;
        }
    }

    // ── Explicit cast: (type)expr ─────────────────────────────────────────────

    /// <summary>
    /// C-style explicit cast. Converts the result of <see cref="Expr"/> to
    /// <see cref="TargetType"/> at runtime, using truncation semantics for
    /// floating-point → integer casts (same as C/C++).
    ///
    /// Supported target types: int, short, long, uint, ushort, ulong,
    ///                          byte, sbyte, float, double, bool, char, string.
    /// </summary>
    public class FabCast : FabBase
    {
        public string TargetType { get; }
        public FabBase Expr { get; }

        public FabCast(string targetType, FabBase expr) { TargetType = targetType; Expr = expr; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Expr.Eval(interpreter);
            return TargetType switch
            {
                "int" => (object)(int)ToDouble(val, "int"),
                "short" => (object)(short)ToDouble(val, "short"),
                "long" => (object)(long)ToDouble(val, "long"),
                "uint" => (object)(uint)ToDouble(val, "uint"),
                "ushort" => (object)(ushort)ToDouble(val, "ushort"),
                "ulong" => (object)(ulong)ToDouble(val, "ulong"),
                "byte" => (object)(byte)ToDouble(val, "byte"),
                "sbyte" => (object)(sbyte)ToDouble(val, "sbyte"),
                "float" => (object)ToFloat(val),
                "double" => ToDouble(val, "double"),
                "ldouble" => Convert.ToDecimal(val),
                "bool" => val is bool b ? (object)b
                                          : ToDouble(val, "bool") != 0,
                "char" => val switch
                {
                    char c => (object)c,
                    string s when s.Length == 1 => s[0],
                    string s => throw new Exception(
                        $"Cast to char: string \"{s}\" has more than one character"),
                    double d => (char)(int)Math.Truncate(d),
                    _ => (char)(int)ToDouble(val, "char")
                },
                "string" => FormatValue(val),
                _ => throw new Exception($"Cast to unknown type '{TargetType}'")
            };
        }

        // ── Helpers ──────────────────────────────────────────────────────────

        /// <summary>
        /// Converts val to double for numeric casts.
        /// char  → ASCII code  (e.g. 'A' → 65)
        /// string → parsed as number  (e.g. "42" → 42, "3.14" → 3.14)
        /// anything else → Convert.ToDouble (handles int, float, bool …)
        /// </summary>
        private static double ToDouble(object val, string targetType)
        {
            // For float/double targets we keep the fractional part
            bool keepFraction = targetType is "float" or "double";

            double raw = val switch
            {
                char c => (double)c,
                string s => double.TryParse(s, System.Globalization.NumberStyles.Any,
                                System.Globalization.CultureInfo.InvariantCulture, out double parsed)
                            ? parsed
                            : throw new Exception(
                                $"Cast to {targetType}: cannot convert string \"{s}\" to a number"),
                _ => Convert.ToDouble(val)
            };
            return keepFraction ? raw : Math.Truncate(raw);
        }

        private static float ToFloat(object val) => val switch
        {
            char c => (float)c,
            string s => float.TryParse(s, System.Globalization.NumberStyles.Any,
                            System.Globalization.CultureInfo.InvariantCulture, out float f)
                        ? f
                        : throw new Exception($"Cast to float: cannot convert string \"{s}\" to a number"),
            _ => Convert.ToSingle(val)
        };
    }

    // ── void sentinel ─────────────────────────────────────────────────────────

    /// <summary>
    /// Singleton marker returned by void functions.
    /// If the user tries to assign or print it, they get a clear message.
    /// </summary>
    public sealed class FabVoidSentinel
    {
        public static readonly FabVoidSentinel Instance = new();
        private FabVoidSentinel() { }
        public override string ToString() => "void";
    }

    // Форматированная строка: $"Hello {name}, result {a + b}"
    public class FabFString : FabBase
    {
        public string Template { get; }
        private static readonly Dictionary<string, FabBase> _cache = new();

        public FabFString(string template) { Template = template; }

        public override object Eval(FabInterpreter interpreter)
        {
            return Regex.Replace(Template, @"\{([^}]+)\}", match =>
            {
                string exprText = match.Groups[1].Value.Trim();
                if (!_cache.TryGetValue(exprText, out var ast))
                {
                    var tokens = new Lexer().Lex(exprText).ToList();
                    ast = new Parser(tokens).ParseExpr();
                    _cache[exprText] = ast;
                }
                var result = ast.Eval(interpreter);
                if (result is double d && d == Math.Floor(d) && !double.IsInfinity(d))
                    return ((long)d).ToString();
                if (result is double df)
                    return df.ToString(CultureInfo.InvariantCulture);
                return FormatValue(result);
            });
        }
    }

    // ── Variable ──────────────────────────────────────────────────────────────

    public class FabVariable : FabBase
    {
        public string Name { get; }
        public FabVariable(string name) { Name = name; }
        public override object Eval(FabInterpreter interpreter)
        {
            if (interpreter.Scope.TryGet(Name, out var val))
                return val;

            // Directly imported via 'use lib only name;' — works for both fields and functions
            if (interpreter.ImportedFunctions.TryGetValue(Name, out var imported))
                return FabLibRegistry.HasLib(imported.LibName)
                    ? FabLibRegistry.Call(imported.LibName, imported.FuncName, Array.Empty<string>())
                    : new FabLibCall(imported.LibName, imported.FuncName, new List<FabBase>()).Eval(interpreter);

            foreach (var ns in interpreter.ActiveNamespaces)
            {
                if (interpreter.UsedLibs.Contains(ns) && FabLibRegistry.HasLib(ns))
                    if (FabLibRegistry.Libs[ns].ContainsKey(Name))
                    {
                        if (!interpreter.IsLibFuncAllowed(ns, Name))
                            throw new Exception($"Function '{Name}' was not exported from library '{ns}' (not listed in 'only' clause)");
                        return FabLibRegistry.Call(ns, Name, Array.Empty<string>());
                    }
                if (interpreter.LibFunctions.TryGetValue(ns, out var userLib) && userLib.ContainsKey(Name))
                    return new FabLibCall(ns, Name, new List<FabBase>()).Eval(interpreter);
            }

            throw new Exception($"Variable '{Name}' is not defined.{DidYouMean(Name, interpreter.Scope.AllNames().Concat(interpreter.Functions.Keys).Concat(interpreter.ImportedFunctions.Keys))}");
        }
    }

    // ── Member access (strings, lists, dicts, numbers, bools) ─────────

    public class FabMemberAccess : FabBase
    {
        public FabBase Target { get; }
        public string Member { get; }
        public List<FabBase>? Args { get; }

        public FabMemberAccess(FabBase target, string member, List<FabBase>? args)
        {
            Target = target; Member = member; Args = args;
        }

        public override object Eval(FabInterpreter interp)
        {
            // ── INSTANCE ACCESS (before lib-fallback) ──
            // Handle both FabVariable (normal obj.method) and FabLiteral wrapping
            // a FabInstance (used internally when a bare method call delegates here).
            FabInstance? directInst = null;
            if (Target is FabLiteral lit && lit.Eval(interp) is FabInstance li)
                directInst = li;

            if (directInst != null)
                return EvalInstance(directInst, interp);

            if (Target is FabVariable instVar
                && interp.Scope.TryGet(instVar.Name, out var maybeInst)
                && maybeInst is FabInstance inst)
                return EvalInstance(inst, interp);

            // ── LIB FALLBACK ──
            if (Target is FabVariable libVar)
            {
                // ── STATIC CLASS ACCESS: ClassName.Method(args) ──
                if (interp.Classes.TryGetValue(libVar.Name, out var maybeStaticClass)
                    && maybeStaticClass.IsStatic
                    && !interp.Scope.Contains(libVar.Name))
                    return EvalStaticMethod(maybeStaticClass, interp);

                // ── Namespace alias: mt.pow(...) → math.pow(...) ──
                string resolvedLibName = interp.LibAliases.TryGetValue(libVar.Name, out var aliasTarget)
                    ? aliasTarget : libVar.Name;

                bool isBuiltinLib = FabLibRegistry.HasLib(resolvedLibName) && interp.UsedLibs.Contains(resolvedLibName);
                bool isUserLib = interp.LibFunctions.ContainsKey(resolvedLibName);
                if ((isBuiltinLib || isUserLib) && !interp.Scope.Contains(libVar.Name))
                    return new FabLibCall(resolvedLibName, Member, Args ?? new List<FabBase>()).Eval(interp);
            }

            var val = Target.Eval(interp);

            // ── ERROR (native TypeError/ValueError/... value) ──
            if (val is FabError ferr)
            {
                return Member switch
                {
                    "message" => ferr.Message,
                    "type" => ferr.TypeName,
                    "to_str" => Args == null ? throw new Exception($"'{Member}' is a method — use e.to_str()") : ferr.Message,
                    _ => throw new Exception($"Error has no member '{Member}'.{DidYouMean(Member, new[] { "message", "type", "to_str" })}")
                };
            }

            // ── STRUCT ──
            if (val is FabStructValue sv)
            {
                if (!sv.Fields.ContainsKey(Member))
                    throw new Exception($"Struct '{sv.StructDef.Name}' has no field '{Member}'.{DidYouMean(Member, sv.Fields.Keys)}");
                return sv.Fields[Member];
            }

            // ── TUPLE ──
            if (val is FabTuple tup)
            {
                if (Member == "length") return (object)(double)tup.Length;
                if (Member == "to_str") return tup.ToString();
                if (Member == "is_empty") return (object)(tup.Length == 0);
                if (int.TryParse(Member, out int tupIdx))
                    return tup.Get(tupIdx);
                throw new Exception($"Tuple has no member '{Member}'");
            }

            // ── LIST ──
            if (val is FabList list)
            {
                return Member switch
                {
                    // ── Property (no parentheses) ──
                    "length" => (object)(double)list.Items.Count,
                    // ── Methods (require parentheses) ──
                    "add" => Args == null ? throw new Exception($"'{Member}' is a method — use list.add(value)") : ListAdd(list, interp),
                    "addend" => Args == null ? throw new Exception($"'{Member}' is a method — use list.addend(value)") : ListAddEnd(list, interp),
                    "remove" => Args == null ? throw new Exception($"'{Member}' is a method — use list.remove(index)") : ListRemove(list, interp),
                    "contains" => Args == null ? throw new Exception($"'{Member}' is a method — use list.contains(value)") : ListContains(list, interp),
                    "index" => Args == null ? throw new Exception($"'{Member}' is a method — use list.index(value)") : ListIndexOf(list, interp),
                    "first" => Args == null ? throw new Exception($"'{Member}' is a method — use list.first()") : (list.Items.Count == 0 ? throw new Exception("list.first: list is empty") : list.Items[0]),
                    "last" => Args == null ? throw new Exception($"'{Member}' is a method — use list.last()") : (list.Items.Count == 0 ? throw new Exception("list.last: list is empty") : list.Items[^1]),
                    "clear" => Args == null ? throw new Exception($"'{Member}' is a method — use list.clear()") : ListClear(list),
                    "sort" => Args == null ? throw new Exception($"'{Member}' is a method — use list.sort()") : ListSort(list),
                    "reverse" => Args == null ? throw new Exception($"'{Member}' is a method — use list.reverse()") : ListReverse(list),
                    "to_str" => Args == null ? throw new Exception($"'{Member}' is a method — use list.to_str()") : ToStr(list),
                    _ => throw new Exception($"List has no member '{Member}'.{DidYouMean(Member, new[] { "length", "add", "addend", "remove", "contains", "index", "first", "last", "clear", "sort", "reverse", "to_str" })}")
                };
            }

            // ── DICT ──
            if (val is FabDict dict)
            {
                return Member switch
                {
                    // ── Property (no parentheses) ──
                    "length" => (object)(double)dict.Pairs.Count,
                    // ── Methods (require parentheses) ──
                    "keys" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.keys()") : (object)WrapList(dict.Pairs.Select(p => p.Key)),
                    "values" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.values()") : (object)WrapList(dict.Pairs.Select(p => p.Value)),
                    "clear" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.clear()") : DictClear(dict),
                    "get_key" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.get_key(index)") : DictGetKey(dict, interp),
                    "get_value" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.get_value(index)") : DictGetValue(dict, interp),
                    "remove" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.remove(index)") : DictRemove(dict, interp),
                    "contains" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.contains(key)") : DictContains(dict, interp),
                    "to_str" => Args == null ? throw new Exception($"'{Member}' is a method — use dict.to_str()") : ToStr(dict),
                    _ => throw new Exception($"Dictionary has no member '{Member}'.{DidYouMean(Member, new[] { "length", "keys", "values", "clear", "remove", "get_key", "get_value", "contains", "to_str" })}")
                };
            }

            // ── STRING ──
            if (val is string str)
            {
                return Member switch
                {
                    // ── Properties (no parentheses) ──
                    "length" => (object)(double)str.Length,
                    "newline" => "\n",
                    // ── Methods (require parentheses) ──
                    "first" => Args == null ? throw new Exception($"'{Member}' is a method — use str.first()") : (str.Length == 0 ? throw new Exception("str.first: string is empty") : (object)str[0]),
                    "last" => Args == null ? throw new Exception($"'{Member}' is a method — use str.last()") : (str.Length == 0 ? throw new Exception("str.last: string is empty") : (object)str[^1]),
                    "to_lower" => Args == null ? throw new Exception($"'{Member}' is a method — use str.to_lower()") : StringArgMethod(str, interp, s => s.ToLower(), "to_lower"),
                    "to_upper" => Args == null ? throw new Exception($"'{Member}' is a method — use str.to_upper()") : StringArgMethod(str, interp, s => s.ToUpper(), "to_upper"),
                    "to_str" => Args == null ? throw new Exception($"'{Member}' is a method — use str.to_str()") : str,
                    "normalize" => Args == null ? throw new Exception($"'{Member}' is a method — use str.normalize()") : str.Normalize(),
                    "is_normalize" => Args == null ? throw new Exception($"'{Member}' is a method — use str.is_normalize()") : (object)str.IsNormalized(),
                    "is_digit" => Args == null ? throw new Exception($"'{Member}' is a method — use str.is_digit()") : (object)isDigit(str),
                    "is_empty" => Args == null ? throw new Exception($"'{Member}' is a method — use str.is_empty()") : (object)(str.Length == 0),
                    "is_null_or_empty" => Args == null ? throw new Exception($"'{Member}' is a method — use str.is_null_or_empty()") : (object)string.IsNullOrEmpty(str),
                    "is_null_or_space" => Args == null ? throw new Exception($"'{Member}' is a method — use str.is_null_or_space()") : (object)string.IsNullOrWhiteSpace(str),
                    "replace" => Args == null ? throw new Exception($"'{Member}' is a method — use str.replace(old, new)") : StringReplace(str, interp),
                    "substr" => Args == null ? throw new Exception($"'{Member}' is a method — use str.substr(from[, to])") : StringSubstr(str, interp),
                    "trim" => Args == null ? throw new Exception($"'{Member}' is a method — use str.trim(char)") : StringTrim(str, interp, "trim"),
                    "trim_end" => Args == null ? throw new Exception($"'{Member}' is a method — use str.trim_end(char)") : StringTrim(str, interp, "trim_end"),
                    "trim_start" => Args == null ? throw new Exception($"'{Member}' is a method — use str.trim_start(char)") : StringTrim(str, interp, "trim_start"),
                    "index" => Args == null ? throw new Exception($"'{Member}' is a method — use str.index(sub)") : StringIndex(str, interp),
                    "split" => Args == null ? throw new Exception($"'{Member}' is a method — use str.split(sep)") : StringSplit(str, interp),
                    "repeat" => Args == null ? throw new Exception($"'{Member}' is a method — use str.repeat(char, n)") : StringRepeat(str, interp),
                    "contains" => Args == null ? throw new Exception($"'{Member}' is a method — use str.contains(sub)") : StringContains(str, interp),
                    _ => throw new Exception($"String has no member '{Member}'.{DidYouMean(Member, new[] { "length", "newline",
                        "first", "last", "to_lower", "to_upper", "to_str", "replace", "substr", "index", "split",
                        "is_digit", "is_empty", "repeat", "contains", "is_null_or_empty", "is_null_or_space",
                        "trim", "trim_end", "trim_start", "normalize", "is_normalize" })}")
                };
            }

            // ── CHAR ──
            if (val is char ch)
            {
                return Member switch
                {
                    // ── Properties (no parentheses) ──
                    "to_lower" => (object)char.ToLower(ch),
                    "to_upper" => (object)char.ToUpper(ch),
                    "to_str" => ch.ToString(),
                    "is_digit" => (object)char.IsDigit(ch),
                    "is_lower" => (object)char.IsLower(ch),
                    "is_upper" => (object)char.IsUpper(ch),
                    "is_number" => (object)char.IsNumber(ch),
                    "is_symbol" => (object)char.IsSymbol(ch),
                    "is_space" => (object)char.IsWhiteSpace(ch),
                    "is_punctuation" => (object)char.IsPunctuation(ch),
                    "is_letter" => (object)char.IsLetter(ch),
                    "ascii" => (object)(double)ch,
                    _ => throw new Exception($"Char has no member '{Member}'.{DidYouMean(Member, new[] { "to_lower", "to_upper", "to_str", "is_digit", "is_lower", "is_upper", "is_number", "is_symbol", "is_space", "is_punctuation", "is_letter", "ascii" })}")
                };
            }

            // ── NUMERIC ──
            if (IsNumeric(val))
            {
                double dv = Convert.ToDouble(val);
                return Member switch
                {
                    "to_str" => Args == null ? throw new Exception($"'{Member}' is a method — use num.to_str()") : ToStr(val),
                    "is_even" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_even()") : (object)(Convert.ToInt64(val) % 2 == 0),
                    "is_odd" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_odd()") : (object)(Convert.ToInt64(val) % 2 != 0),
                    "is_positive" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_positive()") : (object)(dv > 0),
                    "is_negative" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_negative()") : (object)(dv < 0),
                    "is_zero" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_zero()") : (object)(dv == 0.0),
                    "is_even_integer" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_even_integer()") : (object)double.IsEvenInteger(dv),
                    "is_odd_integer" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_odd_integer()") : (object)double.IsOddInteger(dv),
                    "is_pow2" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_pow2()") : (object)double.IsPow2(dv),
                    "is_pow3" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_pow3()") : (object)IsPow3(dv),
                    "is_integer" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_integer()") : (object)(dv == Math.Truncate(dv) && !double.IsInfinity(dv)),
                    "is_nan" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_nan()") : (object)double.IsNaN(dv),
                    "is_infinity" => Args == null ? throw new Exception($"'{Member}' is a method — use num.is_infinity()") : (object)double.IsInfinity(dv),
                    _ => throw new Exception($"Type '{val.GetType().Name}' has no member '{Member}'.{DidYouMean(Member, new[] {
                        "max_value", "min_value", "to_str", "is_even", "is_odd", "is_positive", "is_negative", "is_zero",
                        "is_even_integer", "is_odd_integer", "is_pow2", "is_pow3", "is_integer", "is_nan", "is_infinity" })}")
                };
            }

            // ── BOOL ──
            if (val is bool b)
                return Member switch
                {
                    "to_str" => b ? "true" : "false",
                    "not" => (object)(!b),
                    _ => throw new Exception($"bool has no member '{Member}'.{DidYouMean(Member, new[] { "to_str", "not" })}")
                };

            throw new Exception($"Type '{val?.GetType().Name ?? "null"}' has no member '{Member}'");
        }

        // ── Instance member helper ──
        private object EvalInstance(FabInstance inst, FabInterpreter interp)
        {
            var classDef = inst.ClassDef;

            // ── Field read ──
            var fieldDef = classDef.Fields.FirstOrDefault(f => f.Name == Member);
            if (fieldDef != null)
            {
                if (fieldDef.Visibility == "private")
                {
                    bool inside = interp.Scope.TryGet("__instance__", out var self)
                                  && self is FabInstance fi && fi.ClassDef.Name == classDef.Name;
                    if (!inside)
                        throw new Exception($"Field '{Member}' is private in class '{classDef.Name}'");
                }
                return inst.Fields.TryGetValue(Member, out var fv) ? fv : null;
            }

            // ── Method call ──
            if (classDef.Methods.TryGetValue(Member, out var method))
            {
                // Enforce visibility: private methods can only be called from within the same class
                if (method.Visibility == "private")
                {
                    bool inside = interp.Scope.TryGet("__instance__", out var self)
                                  && self is FabInstance fi && fi.ClassDef.Name == classDef.Name;
                    if (!inside)
                        throw new Exception($"Method '{Member}' is private in class '{classDef.Name}'");
                }

                var callArgs = Args ?? new List<FabBase>();
                if (callArgs.Count != method.Params.Count)
                    throw new Exception(
                        $"Method '{Member}' expects {method.Params.Count} arguments, got {callArgs.Count}");

                var argValues = callArgs.Select(a => a.Eval(interp)).ToArray();
                var savedScope = interp.Scope;
                var savedFunctions = interp.Functions;
                interp.Scope = interp.NewCallScope(method);

                // Inject all instance fields into scope so body can read/write them directly
                foreach (var kv in inst.Fields)
                    interp.Scope.Define(kv.Key, kv.Value);

                // Bind parameters (may shadow fields)
                for (int i = 0; i < method.Params.Count; i++)
                    interp.Scope.Define(method.Params[i].Name, argValues[i]);

                // Hidden marker so private-access checks work
                interp.Scope.Define("__instance__", inst);

                // Inject all class methods into Functions so bare calls like helper()
                // and self.method() calls resolve correctly from within a method body.
                // We merge on top of the existing function table so top-level functions
                // remain callable too.
                interp.Functions = new Dictionary<string, FabFuncDef>(savedFunctions);
                foreach (var kv in classDef.Methods)
                    interp.Functions[kv.Key] = kv.Value;

                // Also expose the instance under a reserved name so 'self.method()'
                // syntax works: FabMemberAccess will find '__self__' in scope and
                // dispatch back through EvalInstance.
                interp.Scope.Define("__self__", inst);

                object result = null;
                try { foreach (var stmt in method.Body) stmt.Eval(interp); }
                catch (ReturnException ret) { result = ret.Value; }
                finally
                {
                    // Write back field changes from scope into instance
                    foreach (var f in classDef.Fields)
                        if (interp.Scope.TryGet(f.Name, out var updated))
                            inst.Fields[f.Name] = updated;
                    interp.Scope = savedScope;
                    interp.Functions = savedFunctions;
                }
                return result;
            }

            var classMembers = classDef.Fields.Select(f => f.Name)
                .Concat(classDef.Methods.Keys);
            throw new Exception($"Class '{classDef.Name}' has no member '{Member}'.{DidYouMean(Member, classMembers)}");
        }

        // ── Static method/field helper ──
        private object EvalStaticMethod(FabClasses classDef, FabInterpreter interp)
        {
            // ── Static field read ──
            var fieldDef = classDef.Fields.FirstOrDefault(f => f.Name == Member);
            if (fieldDef != null)
            {
                if (fieldDef.Visibility == "private")
                    throw new Exception($"Field '{Member}' is private in static class '{classDef.Name}'");
                // Static fields live in the interpreter's static field store
                return interp.GetStaticField(classDef.Name, Member) ?? fieldDef.Default?.Eval(interp);
            }

            // ── Static method call ──
            if (!classDef.Methods.TryGetValue(Member, out var method))
            {
                var staticMembers = classDef.Fields.Select(f => f.Name).Concat(classDef.Methods.Keys);
                throw new Exception($"Static class '{classDef.Name}' has no member '{Member}'.{DidYouMean(Member, staticMembers)}");
            }

            if (method.Visibility == "private")
                throw new Exception($"Method '{Member}' is private in static class '{classDef.Name}'");

            var callArgs = Args ?? new List<FabBase>();
            if (callArgs.Count != method.Params.Count)
                throw new Exception(
                    $"Method '{Member}' expects {method.Params.Count} arguments, got {callArgs.Count}");

            var argValues = callArgs.Select(a => a.Eval(interp)).ToArray();
            var savedScope = interp.Scope;
            var savedFunctions = interp.Functions;
            interp.Scope = interp.NewCallScope(method);

            // Inject static fields as mutable scope variables so the method body can read/write them
            foreach (var f in classDef.Fields)
            {
                var storedVal = interp.GetStaticField(classDef.Name, f.Name) ?? f.Default?.Eval(interp);
                interp.Scope.Define(f.Name, storedVal);
            }

            // Bind parameters
            for (int i = 0; i < method.Params.Count; i++)
                interp.Scope.Define(method.Params[i].Name, argValues[i]);

            // Inject all class methods into Functions for intra-class bare calls
            interp.Functions = new Dictionary<string, FabFuncDef>(savedFunctions);
            foreach (var kv in classDef.Methods)
                interp.Functions[kv.Key] = kv.Value;

            // Marker so private-access checks can verify context
            interp.Scope.Define("__static_class__", classDef.Name);

            object result = null;
            if (method is FabBuiltinMethod bm)
            {
                result = bm.Invoke(argValues);
            }
            else
            {
                try { foreach (var stmt in method.Body) stmt.Eval(interp); }
                catch (ReturnException ret) { result = ret.Value; }
                finally
                {
                    // Write back static field changes
                    foreach (var f in classDef.Fields)
                        if (interp.Scope.TryGet(f.Name, out var updated))
                            interp.SetStaticField(classDef.Name, f.Name, updated);
                    interp.Scope = savedScope;
                    interp.Functions = savedFunctions;
                }
            }
            return result;
        }

        // ── List helpers ──
        private object ListAdd(FabList list, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("list.add() requires 1 argument");
            list.AddFront(Args[0].Eval(interp));
            return null;
        }

        private object ListAddEnd(FabList list, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("list.addend() requires 1 argument");
            list.Add(Args[0].Eval(interp));
            return null;
        }

        private static object ListClear(FabList list) { list.Items.Clear(); return null; }

        private object ListRemove(FabList list, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("list.remove() requires 1 argument (index)");
            int idx = Convert.ToInt32(Args[0].Eval(interp));
            if (idx < 0 || idx >= list.Items.Count)
                throw new Exception($"list.remove: index {idx} out of range (length {list.Items.Count})");
            list.Items.RemoveAt(idx);
            return null;
        }

        private object ListContains(FabList list, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("list.contains() requires 1 argument");
            var item = Args[0].Eval(interp);
            return (object)list.Items.Any(x => FormatValue(x) == FormatValue(item));
        }

        private object ListIndexOf(FabList list, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("list.index() requires 1 argument");
            var item = Args[0].Eval(interp);
            for (int i = 0; i < list.Items.Count; i++)
                if (FormatValue(list.Items[i]) == FormatValue(item)) return (object)(double)i;
            return (object)(double)(-1);
        }

        private static object ListReverse(FabList list)
        {
            list.Items.Reverse();
            return null;
        }

        private object DictContains(FabDict dict, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("dict.contains() requires 1 argument (key)");
            var key = Args[0].Eval(interp);
            return (object)dict.ContainsKey(key);
        }

        private static FabList WrapList(IEnumerable<object> items)
        {
            var list = new FabList("vfree");
            foreach (var item in items) list.Add(item);
            return list;
        }

        private static object ListSort(FabList list)
        {
            list.Items.Sort((a, b) =>
            {
                if (a is string sa && b is string sb) return string.Compare(sa, sb, StringComparison.Ordinal);
                double da = Convert.ToDouble(a), db = Convert.ToDouble(b);
                return da.CompareTo(db);
            });
            return null;
        }

        // ── Dict helpers ──
        private object DictClear(FabDict dict) { dict.Pairs.Clear(); return null; }

        private object DictGetKey(FabDict dict, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("dict.get_key() requires 1 argument (index)");
            int idx = Convert.ToInt32(Args[0].Eval(interp));
            if (idx < 0 || idx >= dict.Pairs.Count)
                throw new Exception($"Dictionary index {idx} out of range (length {dict.Pairs.Count})");
            return dict.Pairs[idx].Key;
        }

        private object DictGetValue(FabDict dict, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("dict.get_value() requires 1 argument (index)");
            int idx = Convert.ToInt32(Args[0].Eval(interp));
            if (idx < 0 || idx >= dict.Pairs.Count)
                throw new Exception($"Dictionary index {idx} out of range (length {dict.Pairs.Count})");
            return dict.Pairs[idx].Value;
        }

        private object DictRemove(FabDict dict, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1)
                throw new Exception("dict.remove() requires 1 argument (index)");

            int idx = Convert.ToInt32(Args[0].Eval(interp));

            if (idx >= dict.Pairs.Count)
                throw new Exception($"Dictionary index {idx} out of range (length {dict.Pairs.Count})");

            dict.Pairs.RemoveAt(idx);

            return null;
        }

        // ── String helpers ──
        private object Arg(int i, FabInterpreter interp)
        {
            if (Args == null || i >= Args.Count)
                throw new Exception($"Missing argument {i} for '{Member}'");
            return Args[i].Eval(interp);
        }

        private object StringReplace(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 2) throw new Exception("replace() requires 2 arguments");
            var a0 = Arg(0, interp); var a1 = Arg(1, interp);
            string oldVal = a0 is char c0 ? c0.ToString() : a0?.ToString() ?? "";
            string newVal = a1 is char c1 ? c1.ToString() : a1?.ToString() ?? "";
            return str.Replace(oldVal, newVal);
        }

        private object StringRepeat(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 2) throw new Exception("repeat() requires 2 arguments");
            var a0 = Arg(0, interp); var b0 = Arg(1, interp);
            string ch = a0.ToString();
            int n = int.TryParse(b0.ToString(), out int b) ? b : throw new Exception("Second argument is not int");
            return string.Join("", Enumerable.Repeat(ch, n));
        }

        private object StringContains(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1) throw new Exception("contains() requires 1 arguments");
            var a0 = Arg(0, interp);
            return str.Contains(a0.ToString());
        }

        private object StringSubstr(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count < 1) throw new Exception("substr() requires at least 1 argument");
            int from = Convert.ToInt32(Arg(0, interp));
            if (Args.Count == 1) return str.Substring(from);
            int to = Convert.ToInt32(Arg(1, interp));
            if (to == -1) return str.Substring(from);
            return str.Substring(from, to - from);
        }

        private object StringIndex(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1) throw new Exception("index() requires 1 argument");
            var a = Arg(0, interp);
            string search = a is char c ? c.ToString() : a?.ToString() ?? "";
            return (double)str.IndexOf(search);
        }

        private object StringSplit(string str, FabInterpreter interp)
        {
            if (Args == null || Args.Count != 1) throw new Exception("split() requires 1 argument");
            var a = Arg(0, interp);
            string sep = a is char c ? c.ToString() : a?.ToString() ?? "";
            return str.Split(new[] { sep }, StringSplitOptions.None).Cast<object>().ToArray();
        }

        private object StringTrim(string str, FabInterpreter interp, string member)
        {
            var a = Arg(0, interp);
            char sep = a is char c ? c : Convert.ToChar(a);
            if (member == "trim") return str.Trim(sep);
            else if (member == "trim_end") return str.TrimEnd(sep);
            else return str.TrimStart(sep);
        }

        // ── Arg-style helpers for string ──
        // e.g. string.to_upper("hello")  — the method is called on the TYPE,
        // the string to transform is Args[0].
        private object StringArgMethod(string fallback, FabInterpreter interp,
            Func<string, string> fn, string name)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                string s = a is char c ? c.ToString() : a?.ToString() ?? "";
                return fn(s);
            }
            // Zero-arg fallback: operate on the instance value
            if (Args == null || Args.Count == 0)
                return fn(fallback);
            throw new Exception($"{name}() takes 0 or 1 argument");
        }

        private object StringArgCharMethod(string fallback, FabInterpreter interp,
            Func<string, char> fn, string name)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                string s = a is char c ? c.ToString() : a?.ToString() ?? "";
                return fn(s);
            }
            // Zero-arg fallback: operate on the instance value
            if (Args == null || Args.Count == 0)
                return fn(fallback);
            throw new Exception($"{name}() takes 0 or 1 argument");
        }

        private object StringArgBoolMethod(string fallback, FabInterpreter interp,
            Func<string, bool> fn, string name)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                string s = a is char c ? c.ToString() : a?.ToString() ?? "";
                return fn(s);
            }
            if (Args == null || Args.Count == 0)
                return fn(fallback);
            throw new Exception($"{name}() takes 0 or 1 argument");
        }

        // ── Arg-style helpers for char ──
        // e.g. char.to_upper('a')  — the char to test/transform is Args[0].
        private object CharArgMethod(char fallback, FabInterpreter interp,
            Func<char, char> fn, string name)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                char c = a is char ch ? ch : (a?.ToString() ?? " ").Length == 1
                    ? (a.ToString())[0]
                    : throw new Exception($"{name}() argument must be a char");
                return fn(c);
            }
            if (Args == null || Args.Count == 0)
                return fn(fallback);
            throw new Exception($"{name}() takes 0 or 1 argument");
        }

        private object CharArgBoolMethod(char fallback, FabInterpreter interp,
            Func<char, bool> fn, string name)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                char c = a is char ch ? ch : (a?.ToString() ?? " ").Length == 1
                    ? (a.ToString())[0]
                    : throw new Exception($"{name}() argument must be a char");
                return fn(c);
            }
            if (Args == null || Args.Count == 0)
                return fn(fallback);
            throw new Exception($"{name}() takes 0 or 1 argument");
        }

        private object CharArgToStr(char fallback, FabInterpreter interp)
        {
            if (Args != null && Args.Count == 1)
            {
                var a = Args[0].Eval(interp);
                char c = a is char ch ? ch : (a?.ToString() ?? " ").Length == 1
                    ? (a.ToString())[0]
                    : throw new Exception("to_str() argument must be a char");
                return char.ToString(c);
            }
            return char.ToString(fallback);
        }

        // ── Type utilities ──
        private static string ToStr(object val) => val switch
        {
            float f => f.ToString(CultureInfo.InvariantCulture),
            double d => d % 1 == 0 && !double.IsInfinity(d) ? ((long)d).ToString() : d.ToString(CultureInfo.InvariantCulture),
            _ => val.ToString()
        };

        private static bool isDigit(string str)
        {
            return str.All(char.IsDigit);
        }

        private static bool IsNumeric(object v) =>
            v is int or short or long or uint or ushort or ulong or float or double or decimal or byte;

        private static object MaxValue(object v) => v switch
        {
            int _ => (object)int.MaxValue,
            short _ => short.MaxValue,
            long _ => long.MaxValue,
            uint _ => uint.MaxValue,
            ushort _ => ushort.MaxValue,
            ulong _ => ulong.MaxValue,
            byte _ => byte.MaxValue,
            float _ => float.MaxValue,
            double _ => double.MaxValue,
            decimal _ => decimal.MaxValue,
            _ => throw new Exception($"max_value not available for {v.GetType().Name}")
        };

        private static object MinValue(object v) => v switch
        {
            int _ => (object)int.MinValue,
            short _ => short.MinValue,
            long _ => long.MinValue,
            uint _ => 0.0,
            ushort _ => 0.0,
            ulong _ => 0.0,
            byte _ => 0.0,
            float _ => float.MinValue,
            double _ => double.MinValue,
            decimal _ => decimal.MinValue,
            _ => throw new Exception($"min_value not available for {v.GetType().Name}")
        };
        private bool IsPow3(double value)
        {
            string a = Math.Cbrt(value).ToString();
            if (a.Contains('.') || a.Contains(','))
                return false;
            else
                return true;
            throw new Exception($"{value.GetType().Name}.is_pow3 requires 1 intenger argument");
        }
    }

    // ── Index access: str[0], arr[i], list[i], dict[key] ─────────────────────

    public class FabIndexAccess : FabBase
    {
        public FabBase Target { get; }
        public FabBase Index { get; }
        public FabIndexAccess(FabBase target, FabBase index) { Target = target; Index = index; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Target.Eval(interpreter);
            var idxVal = Index.Eval(interpreter);

            if (val is FabList list)
            {
                int idx = Convert.ToInt32(idxVal);
                if (idx < 0 || idx >= list.Items.Count)
                    throw new Exception($"List index {idx} out of range (length {list.Items.Count})");
                return list.Items[idx];
            }

            if (val is FabDict dict)
            {
                if (idxVal is double dIdx && dIdx == Math.Floor(dIdx))
                {
                    int i = (int)dIdx;
                    if (i >= 0 && i < dict.Pairs.Count)
                        return dict.Pairs[i].Value;
                }

                return dict.Get(idxVal);
            }

            if (val is string str)
            {
                int idx = Convert.ToInt32(idxVal);
                if (idx < 0 || idx >= str.Length)
                    throw new Exception($"String index {idx} out of range (length {str.Length})");
                return str[idx];
            }

            if (val is object[] arr)
            {
                int idx = Convert.ToInt32(idxVal);
                if (idx < 0 || idx >= arr.Length)
                    throw new Exception($"Array index {idx} out of range (length {arr.Length})");
                return arr[idx];
            }

            throw new Exception($"Type '{val?.GetType().Name}' does not support indexing");
        }
    }

    // ── Index assignment: a[i] = v for arrays, lists, and dicts ──────────────

    public class FabIndexAssign : FabBase
    {
        public string Name { get; }
        public FabBase Index { get; }
        public FabBase Value { get; }
        public FabIndexAssign(string name, FabBase index, FabBase value) { Name = name; Index = index; Value = value; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.TryGet(Name, out var target))
                throw new Exception($"Variable '{Name}' is not defined");

            var idxVal = Index.Eval(interpreter);
            var val = Value.Eval(interpreter);

            if (target is FabList list)
            {
                int idx = Convert.ToInt32(idxVal);
                if (idx < 0 || idx >= list.Items.Count)
                    throw new Exception($"List index {idx} out of range (length {list.Items.Count})");
                list.Items[idx] = val;
                return null;
            }

            if (target is FabDict dict)
            {
                dict.Set(idxVal, val);
                return null;
            }

            if (target is object[] arr)
            {
                int idx = Convert.ToInt32(idxVal);
                if (idx < 0 || idx >= arr.Length)
                    throw new Exception($"Array index {idx} out of range (length {arr.Length})");
                arr[idx] = val;
                return null;
            }

            throw new Exception($"Variable '{Name}' does not support index assignment");
        }
    }

    // ── Static type methods: int.parse("42"), int.max_value ──────────────────

    public class FabTypeMethod : FabBase
    {
        public string TypeName { get; }
        public string Member { get; }
        public List<FabBase>? Args { get; }

        public FabTypeMethod(string typeName, string member, List<FabBase>? args)
        {
            TypeName = typeName; Member = member; Args = args;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            return (TypeName, Member) switch
            {
                ("int", "max_value") => int.MaxValue,
                ("int", "min_value") => int.MinValue,
                ("short", "max_value") => short.MaxValue,
                ("short", "min_value") => short.MinValue,
                ("long", "max_value") => long.MaxValue,
                ("long", "min_value") => long.MinValue,
                ("uint", "max_value") => uint.MaxValue,
                ("uint", "min_value") => 0.0,
                ("ushort", "max_value") => ushort.MaxValue,
                ("ushort", "min_value") => 0.0,
                ("ulong", "max_value") => ulong.MaxValue,
                ("ulong", "min_value") => 0.0,
                ("byte", "max_value") => byte.MaxValue,
                ("byte", "min_value") => 0.0,
                ("sbyte", "max_value") => (object)sbyte.MaxValue,
                ("sbyte", "min_value") => (object)sbyte.MinValue,
                ("float", "max_value") => float.MaxValue,
                ("float", "min_value") => float.MinValue,
                ("double", "max_value") => double.MaxValue,
                ("double", "min_value") => double.MinValue,
                ("decimal", "max_value") => decimal.MaxValue,
                ("decimal", "min_value") => decimal.MinValue,

                ("int", "parse") => ParseAs<int>(interpreter),
                ("short", "parse") => ParseAs<short>(interpreter),
                ("long", "parse") => ParseAs<long>(interpreter),
                ("uint", "parse") => ParseAs<uint>(interpreter),
                ("ushort", "parse") => ParseAs<ushort>(interpreter),
                ("ulong", "parse") => ParseAs<ulong>(interpreter),
                ("byte", "parse") => ParseAs<byte>(interpreter),
                ("sbyte", "parse") => ParseAs<sbyte>(interpreter),
                ("double", "parse") => ParseAs<double>(interpreter),
                ("decimal", "parse") => ParseAs<decimal>(interpreter),

                _ => throw new Exception($"Type '{TypeName}' has no static member '{Member}'")
            };
        }

        private object ParseAs<T>(FabInterpreter interp) where T : struct
        {
            if (Args == null || Args.Count != 1)
                throw new Exception($"{TypeName}.parse() requires 1 argument");
            var raw = Args[0].Eval(interp)?.ToString()
                ?? throw new Exception("parse() argument is null");
            try { return (object)Convert.ChangeType(double.Parse(raw, CultureInfo.InvariantCulture), typeof(T)); }
            catch { throw new Exception($"parse() error: '{raw}' cannot be converted to {TypeName}"); }
        }
    }

    // ── Variable declarations ─────────────────────────────────────────────────

    // int a;  string b;  etc.
    public class FabVarDecl : FabBase
    {
        public string Name { get; }
        public string Type { get; }
        public FabVarDecl(string name, string type) { Name = name; Type = type; }

        public override object Eval(FabInterpreter interpreter)
        {
            interpreter.Scope.Define(Name, null);
            return null;
        }
    }

    // type[] name;                   — unlimited list declaration (no initialiser)
    // type[N] name;                  — limited list declaration
    // type[] name = [...];           — unlimited list with initialiser
    // limited type[N] name = [...];  — limited list with initialiser
    public class FabListDecl : FabBase
    {
        public string Name { get; }
        public string ElementType { get; }
        public int Capacity { get; }          // -1 = unlimited
        public FabBase? Initializer { get; }  // optional [...] literal

        public FabListDecl(string name, string elementType, int capacity = -1, FabBase? initializer = null)
        {
            Name = name; ElementType = elementType; Capacity = capacity; Initializer = initializer;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            if (Initializer != null)
            {
                var val = Initializer.Eval(interpreter);
                if (val is not FabList initList)
                    throw new Exception($"List initializer for '{Name}' must be a list literal");
                var list = new FabList(ElementType, Capacity);
                foreach (var item in initList.Items)
                    list.Add(item);
                interpreter.Scope.Define(Name, list);
            }
            else
            {
                interpreter.Scope.Define(Name, new FabList(ElementType, Capacity));
            }
            return null;
        }
    }

    // (keyType, valType){} name;          /  (keyType, valType){N} name;
    // (keyType, valType){} name = {...};  /  (keyType, valType){N} name = {...};
    public class FabDictDecl : FabBase
    {
        public string Name { get; }
        public string KeyType { get; }
        public string ValType { get; }
        public int Capacity { get; }
        public FabBase? Initializer { get; }  // optional {...} literal

        public FabDictDecl(string name, string keyType, string valType, int capacity = -1, FabBase? initializer = null)
        {
            Name = name; KeyType = keyType; ValType = valType; Capacity = capacity; Initializer = initializer;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            if (Initializer != null)
            {
                var val = Initializer.Eval(interpreter);
                if (val is not FabDict initDict)
                    throw new Exception($"Dict initializer for '{Name}' must be a dict literal");
                var dict = new FabDict(KeyType, ValType, Capacity);
                foreach (var (k, v) in initDict.Pairs)
                    dict.Set(k, v);
                interpreter.Scope.Define(Name, dict);
            }
            else
            {
                interpreter.Scope.Define(Name, new FabDict(KeyType, ValType, Capacity));
            }
            return null;
        }
    }

    // ── Collection literals ───────────────────────────────────────────────────

    /// <summary>[expr, expr, ...]  — produces a FabList at runtime.</summary>
    public class FabListLiteral : FabBase
    {
        public string ElementType { get; }
        public int Capacity { get; }          // -1 = unlimited
        public List<FabBase> Items { get; }

        public FabListLiteral(string elementType, int capacity, List<FabBase> items)
        { ElementType = elementType; Capacity = capacity; Items = items; }

        public override object Eval(FabInterpreter interpreter)
        {
            var list = new FabList(ElementType, Capacity);
            foreach (var item in Items)
                list.Add(item.Eval(interpreter));
            return list;
        }
    }

    /// <summary>{key: val, key: val, ...}  — produces a FabDict at runtime.</summary>
    public class FabDictLiteral : FabBase
    {
        public string KeyType { get; }
        public string ValType { get; }
        public int Capacity { get; }
        public List<(FabBase Key, FabBase Value)> Pairs { get; }

        public FabDictLiteral(string keyType, string valType, int capacity, List<(FabBase, FabBase)> pairs)
        { KeyType = keyType; ValType = valType; Capacity = capacity; Pairs = pairs; }

        public override object Eval(FabInterpreter interpreter)
        {
            var dict = new FabDict(KeyType, ValType, Capacity);
            foreach (var (k, v) in Pairs)
                dict.Set(k.Eval(interpreter), v.Eval(interpreter));
            return dict;
        }
    }

    // ── Tuple literal ─────────────────────────────────────────────────────────

    /// <summary>(expr, expr, ...) — produces a FabTuple at runtime.</summary>
    public class FabTupleLiteral : FabBase
    {
        public List<FabBase> Items { get; }
        public FabTupleLiteral(List<FabBase> items) { Items = items; }

        public override object Eval(FabInterpreter interpreter)
        {
            var values = Items.Select(i => i.Eval(interpreter)).ToArray();
            return new FabTuple(values);
        }
    }

    // ── Lambda literal: (params) => { ... }  /  (params) => expr ───────────────
    // Also reachable via the 'vfree name(params) => ...' declaration shorthand.
    // Evaluates to a FabClosure that snapshots the current lexical frame so it
    // can read/write the enclosing scope's variables when later called.
    public class FabLambdaLiteral : FabBase
    {
        public List<string> Params { get; }
        public List<FabBase> Body { get; }
        public FabLambdaLiteral(List<string> parms, List<FabBase> body) { Params = parms; Body = body; }

        public override object Eval(FabInterpreter interpreter)
        {
            var capturedScope = new ScopeStack(interpreter.Scope.CurrentFrame, interpreter.Scope.GlobalFallback);
            return new FabClosure(Params, Body, capturedScope);
        }
    }

    // ── Tuple destructure: int a, string b = someExpr; ────────────────────────

    /// <summary>
    /// Destructures a FabTuple into multiple typed variables.
    /// Each element of Targets is (declaredType-or-null, variableName).
    /// </summary>
    public class FabTupleDestructure : FabBase
    {
        public List<(string? Type, string Name)> Targets { get; }
        public FabBase Expr { get; }

        public FabTupleDestructure(List<(string?, string)> targets, FabBase expr)
        { Targets = targets; Expr = expr; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Expr.Eval(interpreter);
            FabTuple tuple = val as FabTuple
                ?? throw new Exception("Destructure target must be a tuple");

            if (tuple.Length != Targets.Count)
                throw new Exception(
                    $"Tuple destructure mismatch: tuple has {tuple.Length} elements, " +
                    $"but {Targets.Count} variables were given");

            for (int i = 0; i < Targets.Count; i++)
            {
                var (declType, name) = Targets[i];
                object item = tuple.Items[i];

                if (declType != null)
                {
                    // New typed variable declaration
                    item = FabAssign.CastDeclaredPublic(item, declType, name);
                    interpreter.Scope.Define(name, item);
                }
                else
                {
                    // Assignment to existing variable
                    if (!interpreter.Scope.Contains(name))
                        throw new Exception($"Variable '{name}' is not defined");
                    var slot = interpreter.Scope.GetSlot(name);
                    if (slot.IsConst)
                        throw new Exception($"Cannot assign to const '{name}'");
                    interpreter.Scope.SetSlot(name, new VarSlot(item, slot.IsConst, slot.IsLocked, slot.IsFree));
                }
            }
            return null;
        }
    }

    // ── const ─────────────────────────────────────────────────────────────────
    public class FabConst : FabBase
    {
        public string Name { get; }
        public FabBase Expr { get; }
        public string DeclaredType { get; }

        public FabConst(string name, FabBase expr, string declaredType)
        { Name = name; Expr = expr; DeclaredType = declaredType; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (interpreter.Scope.Contains(Name) && interpreter.Scope.GetSlot(Name).IsConst)
                throw new Exception($"Const '{Name}' is already defined");
            var value = Expr.Eval(interpreter);
            value = FabAssign.CastDeclaredPublic(value, DeclaredType, Name);
            interpreter.Scope.Define(Name, value, isConst: true);
            return null;
        }
    }

    // ── const vfree ───────────────────────────────────────────────────────────
    public class FabConstVFree : FabBase
    {
        public string Name { get; }
        public FabBase Expr { get; }
        public FabConstVFree(string name, FabBase expr) { Name = name; Expr = expr; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (interpreter.Scope.Contains(Name) && interpreter.Scope.GetSlot(Name).IsConst)
                throw new Exception($"Const '{Name}' is already defined");
            var raw = Expr.Eval(interpreter);
            interpreter.Scope.Define(Name, FabVFreeDecl.InferMinType(raw), isConst: true);
            return null;
        }
    }

    // ── Top-level global declaration: 'int a = 1;' / 'static int a = 1;' ──────
    public class FabGlobalDecl : FabBase
    {
        public FabBase Decl { get; }
        public bool IsStatic { get; }
        public FabGlobalDecl(FabBase decl, bool isStatic) { Decl = decl; IsStatic = isStatic; }

        public override object Eval(FabInterpreter interpreter)
        {
            var saved = interpreter.Scope;
            interpreter.Scope = IsStatic ? interpreter.FileGlobals : interpreter.Globals;
            try { Decl.Eval(interpreter); }
            finally { interpreter.Scope = saved; }
            return null;
        }
    }

    // Helper: wraps an already-evaluated value as an expression node
    public class FabLiteral : FabBase
    {
        private readonly object _value;
        public FabLiteral(object value) { _value = value; }
        public override object Eval(FabInterpreter interpreter) => _value;
    }

    // ── vfree ─────────────────────────────────────────────────────────────────

    public class FabVFreeDecl : FabBase
    {
        public string Name { get; }
        public FabBase Expr { get; }
        public bool Locked { get; }
        public FabVFreeDecl(string name, FabBase expr, bool locked) { Name = name; Expr = expr; Locked = locked; }

        public override object Eval(FabInterpreter interpreter)
        {
            var raw = Expr.Eval(interpreter);
            var value = InferMinType(raw);
            // Locked = vfree := (type-pinned), Free = vfree = (widening allowed)
            interpreter.Scope.Define(Name, value, isLocked: Locked, isFree: !Locked);
            return null;
        }

        public static object InferMinType(object val)
        {
            if (val is null) return null;
            if (val is bool) return val;
            if (val is char) return val;
            if (val is string) return val;
            if (val is FabList) return val;
            if (val is FabDict) return val;
            if (val is FabTuple) return val;
            decimal dv = Convert.ToDecimal(val);
            if (dv >= 0 && dv <= byte.MaxValue) return (byte)dv;
            if (dv >= short.MinValue && dv <= short.MaxValue) return (short)dv;
            if (dv >= ushort.MinValue && dv <= ushort.MaxValue) return (ushort)dv;
            if (dv >= int.MinValue && dv <= int.MaxValue) return (int)dv;
            if (dv >= uint.MinValue && dv <= uint.MaxValue) return (uint)dv;
            if (dv >= long.MinValue && dv <= long.MaxValue) return (long)dv;
            if (dv >= ulong.MinValue && dv <= ulong.MaxValue) return (ulong)dv;
            if (dv >= Convert.ToDecimal(float.MinValue) && dv <= Convert.ToDecimal(float.MaxValue)) return (float)dv;
            if (dv >= Convert.ToDecimal(double.MinValue) && dv <= Convert.ToDecimal(double.MaxValue)) return (double)dv;
            return dv;
        }
    }

    // ── Function definition ───────────────────────────────────────────────────

    public class FabFuncDef : FabBase
    {
        public string Name { get; }
        public List<(string Type, string Name)> Params { get; }
        public string? ReturnType { get; }
        public List<FabBase> Body { get; }
        public string Visibility { get; set; } = "public";

        /// <summary>
        /// Set by 'native def name(...) [-> type] [;|{...}|=> expr;]'.
        /// A native function with an empty Body is a bodyless stub: calling
        /// it looks up FabInterpreter.NativeFunctions instead of running a
        /// (nonexistent) Fab-source body, and throws a clear error if no
        /// native implementation was registered. A native function WITH a
        /// body (the '=> expr' delegation form, or a full '{ ... }' block)
        /// behaves exactly like an ordinary function — 'native' there is
        /// purely documentation that it's a thin native-facing wrapper.
        /// </summary>
        public bool IsNative { get; set; } = false;

        /// <summary>
        /// The FileGlobals scope of the interpreter that defined this function/method.
        /// Used so calls always see their own file's 'static' globals, regardless
        /// of which interpreter/caller invokes them (e.g. after 'use "file.fab"').
        /// </summary>
        public ScopeStack? OwnerFileGlobals { get; set; }

        public FabFuncDef(string name, List<(string, string)> parms, string? returnType, List<FabBase> body)
        {
            Name = name; Params = parms; ReturnType = returnType; Body = body;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            OwnerFileGlobals = interpreter.FileGlobals;
            interpreter.Functions[Name] = this;
            return null;
        }
    }

    // ── Field metadata ────────────────────────────────────────────────────────
    public class FabFieldDef
    {
        public string Visibility { get; }   // "public" | "private"
        public string Type { get; }
        public string Name { get; }
        public FabBase? Default { get; }
        public bool isConst { get; }

        public FabFieldDef(string visibility, string type, string name, FabBase? defaultVal = null, bool IsConst = false)
        { Visibility = visibility; Type = type; Name = name; Default = defaultVal; isConst = IsConst; }
    }

    // ── Class definition ──────────────────────────────────────────────────────
    public class FabClasses : FabBase
    {
        public string Name { get; }
        public string? BaseClassName { get; }
        public bool IsStatic { get; }
        // Own (declared) fields and methods — before inheritance is resolved
        public List<FabFieldDef> OwnFields { get; }
        public Dictionary<string, FabFuncDef> OwnMethods { get; }
        public FabFuncDef? OwnConstructor { get; }

        // Effective (merged) fields and methods — populated by Eval once the base is known
        public List<FabFieldDef> Fields { get; set; }
        public Dictionary<string, FabFuncDef> Methods { get; set; }
        public FabFuncDef? Constructor { get; set; }

        public FabClasses(
            string name,
            string? baseClassName,
            List<FabFieldDef> fields,
            Dictionary<string, FabFuncDef> methods,
            FabFuncDef? constructor,
            bool isStatic = false)
        {
            Name = name;
            BaseClassName = baseClassName;
            IsStatic = isStatic;
            OwnFields = fields;
            OwnMethods = methods;
            OwnConstructor = constructor;
            // Until Eval runs, effective == own
            Fields = fields;
            Methods = methods;
            Constructor = constructor;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            if (BaseClassName != null)
            {
                if (!interpreter.Classes.TryGetValue(BaseClassName, out var baseDef))
                    throw new Exception($"Base class '{BaseClassName}' is not defined (must be declared before '{Name}')");

                // Merge fields: base fields first, then child's own (child shadows base on name clash)
                var mergedFields = new List<FabFieldDef>(baseDef.Fields);
                foreach (var f in OwnFields)
                {
                    int idx = mergedFields.FindIndex(bf => bf.Name == f.Name);
                    if (idx >= 0) mergedFields[idx] = f;   // child overrides
                    else mergedFields.Add(f);
                }

                // Merge methods: base methods first, then child's own (override by name)
                var mergedMethods = new Dictionary<string, FabFuncDef>(baseDef.Methods);
                foreach (var kv in OwnMethods)
                    mergedMethods[kv.Key] = kv.Value;

                Fields = mergedFields;
                Methods = mergedMethods;
                // Child's own constructor takes priority; fall back to parent's
                Constructor = OwnConstructor ?? baseDef.Constructor;
            }
            else
            {
                Fields = OwnFields;
                Methods = OwnMethods;
                Constructor = OwnConstructor;
            }

            // ── Tag every method/ctor with this interpreter's file-local globals ──
            if (Constructor != null) Constructor.OwnerFileGlobals = interpreter.FileGlobals;
            foreach (var m in Methods.Values) m.OwnerFileGlobals = interpreter.FileGlobals;

            interpreter.Classes[Name] = this;
            return null;
        }
    }

    // ── Struct definition ─────────────────────────────────────────────────────
    /// <summary>
    /// Value-type aggregate (like C++ struct): all fields public, no methods,
    /// no inheritance. Assignment copies the struct — no shared references.
    /// </summary>
    public class FabStructDef
    {
        public string Name { get; }
        public List<FabFieldDef> Fields { get; }
        public FabStructDef(string name, List<FabFieldDef> fields) { Name = name; Fields = fields; }
    }

    /// <summary>
    /// Runtime value of a struct variable. Always copied on assignment.
    /// </summary>
    public class FabStructValue
    {
        public FabStructDef StructDef { get; }
        public Dictionary<string, object> Fields { get; }

        public FabStructValue(FabStructDef def, FabInterpreter interpreter)
        {
            StructDef = def;
            Fields = new Dictionary<string, object>();
            foreach (var f in def.Fields)
                Fields[f.Name] = f.Default?.Eval(interpreter) ?? null;
        }

        /// <summary>Deep-copy constructor used for value semantics.</summary>
        private FabStructValue(FabStructDef def, Dictionary<string, object> fields)
        {
            StructDef = def;
            Fields = new Dictionary<string, object>(fields.Count);
            foreach (var kv in fields)
                Fields[kv.Key] = kv.Value is FabStructValue sv ? sv.Clone() : kv.Value;
        }

        public FabStructValue Clone() => new FabStructValue(StructDef, Fields);

        public override string ToString()
        {
            var pairs = Fields.Select(kv => $"{kv.Key}: {AST.FormatValue(kv.Value)}");
            return $"{StructDef.Name}{{ {string.Join(", ", pairs)} }}";
        }
    }

    // ── struct StructName varName;  /  struct StructName varName = { ... }; ──
    public class FabStructDecl : FabBase
    {
        public string StructName { get; }
        public string VarName { get; }
        public FabBase? Initializer { get; }   // optional { field: val, ... }

        public FabStructDecl(string structName, string varName, FabBase? initializer = null)
        { StructName = structName; VarName = varName; Initializer = initializer; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Structs.TryGetValue(StructName, out var def))
                throw new Exception($"Struct '{StructName}' is not defined");

            var sv = new FabStructValue(def, interpreter);

            if (Initializer != null)
            {
                var raw = Initializer.Eval(interpreter);
                // Copy from another struct variable (value semantics)
                if (raw is FabStructValue srcSv)
                {
                    if (srcSv.StructDef.Name != StructName)
                        throw new Exception($"Cannot initialise struct '{StructName}' from struct '{srcSv.StructDef.Name}'");
                    sv = srcSv.Clone();
                    interpreter.Scope.Define(VarName, sv);
                    return null;
                }
                if (raw is not FabDict initDict)
                    throw new Exception($"Struct initializer must be a '{{field: value, ...}}' literal or another '{StructName}' struct");
                foreach (var (k, v) in initDict.Pairs)
                {
                    string fname = AST.FormatValue(k);
                    if (!sv.Fields.ContainsKey(fname))
                        throw new Exception($"Struct '{StructName}' has no field '{fname}'");
                    sv.Fields[fname] = v;
                }
            }

            interpreter.Scope.Define(VarName, sv);
            return null;
        }
    }

    // ── struct definition statement ───────────────────────────────────────────
    public class FabStructDefStmt : FabBase
    {
        public FabStructDef Def { get; }
        public FabStructDefStmt(FabStructDef def) { Def = def; }

        public override object Eval(FabInterpreter interpreter)
        {
            interpreter.Structs[Def.Name] = Def;
            return null;
        }
    }


    public class FabInstance
    {
        public FabClasses ClassDef { get; }
        public Dictionary<string, object> Fields { get; } = new();

        public FabInstance(FabClasses classDef, FabInterpreter interpreter)
        {
            ClassDef = classDef;
            foreach (var f in classDef.Fields)
                Fields[f.Name] = f.Default?.Eval(interpreter) ?? null;
        }

        public override string ToString()
        {
            var pairs = Fields.Select(kv => $"{kv.Key}: {AST.FormatValue(kv.Value)}");
            return $"{ClassDef.Name}{{ {string.Join(", ", pairs)} }}";
        }
    }

    // ── new ClassName(args) ───────────────────────────────────────────────────
    public class FabNew : FabBase
    {
        public string ClassName { get; }
        public List<FabBase> Args { get; }
        public FabNew(string className, List<FabBase> args) { ClassName = className; Args = args; }

        public override object Eval(FabInterpreter interpreter)
        {
            // ── Native error types: new TypeError("msg") etc. ──────────────────
            // Not a class instantiation at all — produces a first-class FabError
            // value directly. Checked before the Classes lookup so a script can
            // never accidentally shadow these by declaring its own same-named
            // class (classes and error types live in separate namespaces).
            if (FabErrorTypes.IsKnown(ClassName))
            {
                if (Args.Count != 1)
                    throw new Exception($"'{ClassName}' expects 1 argument (message), got {Args.Count}");
                var msgVal = Args[0].Eval(interpreter);
                string msg = msgVal is string ms ? ms : FormatValue(msgVal);
                return new FabError(ClassName, msg);
            }

            if (!interpreter.Classes.TryGetValue(ClassName, out var classDef))
                throw new Exception($"Class '{ClassName}' is not defined");

            if (classDef.IsStatic && classDef is not FabBuiltinClassWithCtor)
                throw new Exception($"Cannot create an instance of static class '{ClassName}'");

            if (classDef is FabBuiltinClassWithCtor builtinCtor)
            {
                if (Args.Count != builtinCtor.Constructor!.Params.Count)
                {
                    throw new Exception($"'{ClassName}' constructor expects {builtinCtor.Constructor.Params.Count} args. got {Args.Count}");
                    var argValues = Args.Select(a => a.Eval(interpreter)).ToArray();
                    return ((FabBuiltinMethod)builtinCtor.Constructor).Invoke(argValues);
                }
            }

            var instance = new FabInstance(classDef, interpreter);

            if (classDef.Constructor != null)
            {
                var ctor = classDef.Constructor;
                if (Args.Count != ctor.Params.Count)
                    throw new Exception(
                        $"Constructor '{ClassName}' expects {ctor.Params.Count} arguments, got {Args.Count}");

                var argValues = Args.Select(a => a.Eval(interpreter)).ToArray();
                var savedScope = interpreter.Scope;
                interpreter.Scope = interpreter.NewCallScope(ctor);

                // Inject instance fields directly into constructor scope
                foreach (var kv in instance.Fields)
                    interpreter.Scope.Define(kv.Key, kv.Value, isConst: classDef.Fields.FirstOrDefault(f => f.Name == kv.Key)?.isConst ?? false);

                // Bind parameters (may shadow fields — intentional)
                for (int i = 0; i < ctor.Params.Count; i++)
                    interpreter.Scope.Define(ctor.Params[i].Name, argValues[i]);

                // Inject a hidden reference so assignments can write back to the instance
                interpreter.Scope.Define("__instance__", instance);

                try { foreach (var stmt in ctor.Body) stmt.Eval(interpreter); }
                catch (ReturnException) { }
                finally
                {
                    // Write all field names back from scope into instance (includes inherited fields)
                    foreach (var f in classDef.Fields)
                        if (interpreter.Scope.TryGet(f.Name, out var updated))
                            instance.Fields[f.Name] = updated;
                    interpreter.Scope = savedScope;
                }
            }

            return instance;
        }
    }

    // ── obj.field = value  (statement) ───────────────────────────────────────
    public class FabInstanceAssign : FabBase
    {
        public string ObjName { get; }
        public string Field { get; }
        public FabBase Value { get; }
        public FabInstanceAssign(string objName, string field, FabBase value)
        { ObjName = objName; Field = field; Value = value; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.TryGet(ObjName, out var v))
                throw new Exception($"'{ObjName}' is not defined");

            // ── Struct field assignment (value type) ──
            if (v is FabStructValue sv)
            {
                if (!sv.Fields.ContainsKey(Field))
                    throw new Exception($"Struct '{sv.StructDef.Name}' has no field '{Field}'.{AST.DidYouMean(Field, sv.Fields.Keys)}");
                sv.Fields[Field] = Value.Eval(interpreter);
                return null;
            }

            if (v is not FabInstance inst)
                throw new Exception($"'{ObjName}' is not an object instance or struct");

            if (!inst.ClassDef.Fields.Any(f => f.Name == Field))
                throw new Exception($"Class '{inst.ClassDef.Name}' has no field '{Field}'");

            var fieldDef = inst.ClassDef.Fields.First(f => f.Name == Field);
            if (fieldDef.Visibility == "private")
            {
                // Allow if we are inside a method/ctor of the same class
                bool inside = interpreter.Scope.TryGet("__instance__", out var self)
                              && self is FabInstance fi && fi.ClassDef.Name == inst.ClassDef.Name;
                if (!inside)
                    throw new Exception($"Field '{Field}' is private in class '{inst.ClassDef.Name}'");
            }

            inst.Fields[Field] = Value.Eval(interpreter);
            return null;
        }
    }

    // ── Function call ─────────────────────────────────────────────────────────

    public class FabCall : FabBase
    {
        public string Name { get; }
        public List<FabBase> Args { get; }
        public FabCall(string name, List<FabBase> args) { Name = name; Args = args; }

        public override object Eval(FabInterpreter interpreter)
        {
            // ── Calling a value held in a variable (lambda / passed-in callback) ──
            // Only intercepts when the name actually resolves to a FabClosure,
            // so an ordinary same-named global function/parameter is never
            // affected — e.g. a 'def helper(...)' still resolves normally even
            // if some unrelated local variable elsewhere happens to share its name.
            if (interpreter.Scope.TryGet(Name, out var maybeClosure) && maybeClosure is FabClosure closure)
                return InvokeClosure(closure, Args, interpreter);

            if (!interpreter.Functions.ContainsKey(Name))
            {
                // Check directly imported functions (from 'use lib only fn;')
                if (interpreter.ImportedFunctions.TryGetValue(Name, out var imported))
                    return new FabLibCall(imported.LibName, imported.FuncName, Args).Eval(interpreter);

                foreach (var ns in interpreter.ActiveNamespaces)
                {
                    if (interpreter.UsedLibs.Contains(ns) && FabLibRegistry.HasLib(ns))
                        if (FabLibRegistry.Libs[ns].ContainsKey(Name))
                        {
                            if (!interpreter.IsLibFuncAllowed(ns, Name))
                                throw new Exception($"Function '{Name}' was not exported from library '{ns}' (not listed in 'only' clause)");
                            var evaledArgs = Args.Select(a => Convert.ToString(a.Eval(interpreter))).ToArray();
                            return FabLibRegistry.Call(ns, Name, evaledArgs);
                        }
                    if (interpreter.LibFunctions.TryGetValue(ns, out var userLib) && userLib.ContainsKey(Name))
                        return new FabLibCall(ns, Name, Args).Eval(interpreter);
                }
            }

            if (!interpreter.Functions.ContainsKey(Name))
                throw new Exception($"Function '{Name}' is not defined.{DidYouMean(Name, interpreter.Functions.Keys)}");

            var func = interpreter.Functions[Name];

            if (Args.Count != func.Params.Count)
                throw new Exception($"Function '{Name}' expects {func.Params.Count} arguments, got {Args.Count}");

            // ── Native function stub: 'native def name(...) [-> type];' ──────
            // No Fab-source body exists to run — dispatch to a host-registered
            // implementation instead (FabInterpreter.RegisterNative), or fail
            // clearly if none was ever registered.
            if (func.IsNative && func.Body.Count == 0)
            {
                var nativeArgs = new object[func.Params.Count];
                for (int i = 0; i < func.Params.Count; i++)
                {
                    var (npType, npName) = func.Params[i];
                    nativeArgs[i] = CastTo(Args[i].Eval(interpreter), npType, npName);
                }
                if (!interpreter.NativeFunctions.TryGetValue(Name, out var nativeImpl))
                    throw new Exception(
                        $"Native function '{Name}' has no implementation. " +
                        $"It was declared with 'native def {Name}(...);' but never given a body " +
                        $"(use '=> expr;' to delegate to another function, or register a host " +
                        $"implementation via FabInterpreter.RegisterNative(\"{Name}\", ...)).");
                return nativeImpl(nativeArgs);
            }

            // ── Bare intra-class method call ──────────────────────────────────
            // If we are currently inside a method body (__instance__ is in scope)
            // and the function being called belongs to that class, delegate the
            // call through FabMemberAccess so that field injection and write-back
            // are handled correctly — exactly as if the user had written
            // __self__.methodName(args).
            if (interpreter.Scope.TryGet("__instance__", out var currentInst)
                && currentInst is FabInstance selfInst
                && selfInst.ClassDef.Methods.ContainsKey(Name))
            {
                return new FabMemberAccess(
                    new FabLiteral(selfInst), Name, Args
                ).Eval(interpreter);
            }

            // Evaluate args in CALLER scope before switching scope
            var argValues = new object[func.Params.Count];
            for (int i = 0; i < func.Params.Count; i++)
            {
                var (pType, pName) = func.Params[i];
                argValues[i] = CastTo(Args[i].Eval(interpreter), pType, pName);
            }

            // Push a completely fresh scope stack (C++ function isolation — no
            // capture of caller locals). Save and restore caller's scope.
            var savedScope = interpreter.Scope;
            var savedFunctions = interpreter.Functions;
            interpreter.Scope = interpreter.NewCallScope(func);
            interpreter.Functions = savedFunctions;

            // Define parameters in the new function's scope
            for (int i = 0; i < func.Params.Count; i++)
            {
                var (_, pName) = func.Params[i];
                interpreter.Scope.Define(pName, argValues[i]);
            }

            object returnValue = null;
            try { foreach (var stmt in func.Body) stmt.Eval(interpreter); }
            catch (ReturnException ret) { returnValue = ret.Value; }
            finally { interpreter.Scope = savedScope; interpreter.Functions = savedFunctions; }

            // Void functions always return the void sentinel (never a real value)
            if (func.ReturnType == "void")
            {
                if (returnValue != null && returnValue is not FabVoidSentinel)
                    throw new Exception($"Function '{Name}' is declared void but returned a value");
                return FabVoidSentinel.Instance;
            }

            return returnValue;
        }

        /// <summary>
        /// Invokes a FabClosure (lambda). Unlike a regular function call, the
        /// closure's body scope falls back to CapturedScope — the exact
        /// lexical frame active where the lambda literal was evaluated — so
        /// it can read and write its enclosing scope's variables.
        /// </summary>
        private static object InvokeClosure(FabClosure closure, List<FabBase> args, FabInterpreter interpreter)
        {
            if (args.Count != closure.Params.Count)
                throw new Exception($"Lambda expects {closure.Params.Count} argument(s), got {args.Count}");

            // Evaluate args in the CALLER's scope before switching.
            var argValues = new object[args.Count];
            for (int i = 0; i < args.Count; i++)
                argValues[i] = args[i].Eval(interpreter);

            var savedScope = interpreter.Scope;
            interpreter.Scope = new ScopeStack { GlobalFallback = closure.CapturedScope };
            for (int i = 0; i < closure.Params.Count; i++)
                interpreter.Scope.Define(closure.Params[i], argValues[i]);

            object result = null;
            try { foreach (var stmt in closure.Body) stmt.Eval(interpreter); }
            catch (ReturnException ret) { result = ret.Value; }
            finally { interpreter.Scope = savedScope; }
            return result;
        }

        private static object CastTo(object val, string type, string name) => type switch
        {
            "int" => val is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot pass {val} as int '{name}'") : (object)(int)d) : Convert.ToInt32(val),
            "short" => val is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot pass {val} as short '{name}'") : (object)(short)d) : Convert.ToInt16(val),
            "long" => val is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot pass {val} as long '{name}'") : (object)(long)d) : Convert.ToInt64(val),
            "uint" => val is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot pass {val} as uint '{name}'") : (object)(uint)d) : Convert.ToUInt32(val),
            "ushort" => val is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot pass {val} as ushort '{name}'") : (object)(ushort)d) : Convert.ToUInt16(val),
            "ulong" => val is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot pass {val} as ulong '{name}'") : (object)(ulong)d) : Convert.ToUInt64(val),
            "byte" => val is double d ? (d % 1 != 0 || d < 0 || d > 255 ? throw new Exception($"Type error: cannot pass {val} as byte '{name}'") : (object)(byte)d) : Convert.ToByte(val),
            "sbyte" => val is double d ? (d % 1 != 0 || d < sbyte.MinValue || d > sbyte.MaxValue ? throw new Exception($"Type error: cannot pass {val} as sbyte '{name}'") : (object)(sbyte)d) : Convert.ToSByte(val),
            "float" => (object)Convert.ToSingle(val),
            "double" => Convert.ToDouble(val),
            "ldouble" => Convert.ToDecimal(val),
            "string" => val is string ? val : throw new Exception($"Type error: cannot pass {val} as string '{name}'"),
            "bool" => val is bool ? val : throw new Exception($"Type error: cannot pass {val} as bool '{name}'"),
            _ => val
        };
    }

    // ── return / throw ────────────────────────────────────────────────────────────────

    public class FabReturn : FabBase
    {
        public FabBase? Expr { get; }
        public FabReturn(FabBase? expr) { Expr = expr; }
        public override object Eval(FabInterpreter interpreter)
        {
            var value = Expr?.Eval(interpreter);
            throw new ReturnException(value);
        }
    }

    public class FabThrow : FabBase
    {
        public FabBase? Expr { get; }
        public FabThrow(FabBase? expr) { Expr = expr; }
        public override object Eval(FabInterpreter interpreter)
        {
            var value = Expr?.Eval(interpreter);
            throw new FabThrownException(value);
        }
    }

    // ── try / catch / finally ─────────────────────────────────────────────────
    // Catches are tried in written order; the first matching clause runs.
    //   catch (TypeError e)  — matches only a native FabError of that type (or
    //                          any of its subtypes, per FabErrorTypes.Hierarchy).
    //                          Does NOT match plain string throws.
    //   catch (string e)     — universal fallback: matches anything (plain
    //                          throws, FabError values, internal engine
    //                          errors), always binding a formatted string.
    //   catch (e)            — same universal fallback as 'string' (kept
    //                          identical so interpreted and compiled programs
    //                          behave the same way — the C++ target has no
    //                          way to preserve an untyped raw thrown value).
    // BreakException/ContinueException/ReturnException are never intercepted
    // here — they always pass straight through (finally still runs).
    public class FabTry : FabBase
    {
        public List<FabBase> Body { get; }
        public List<(string? TypeName, string VarName, List<FabBase> Body)> Catches { get; }
        public List<FabBase>? FinallyBody { get; }

        public FabTry(List<FabBase> body, List<(string?, string, List<FabBase>)> catches, List<FabBase>? finallyBody)
        { Body = body; Catches = catches; FinallyBody = finallyBody; }

        public override object Eval(FabInterpreter interpreter)
        {
            try
            {
                RunBody(interpreter);
            }
            finally
            {
                if (FinallyBody != null)
                {
                    interpreter.Scope.Push();
                    try { foreach (var stmt in FinallyBody) stmt.Eval(interpreter); }
                    finally { interpreter.Scope.Pop(); }
                }
            }
            return null;
        }

        private void RunBody(FabInterpreter interpreter)
        {
            try
            {
                interpreter.Scope.Push();
                try { foreach (var stmt in Body) stmt.Eval(interpreter); }
                finally { interpreter.Scope.Pop(); }
            }
            catch (ReturnException) { throw; }
            catch (BreakException) { throw; }
            catch (ContinueException) { throw; }
            catch (Exception ex)
            {
                object thrownValue = ex is FabThrownException fte ? fte.Value : (object)ex.Message;

                foreach (var (typeName, varName, cbody) in Catches)
                {
                    if (!CatchMatches(thrownValue, typeName)) continue;

                    object boundValue = (typeName == null || typeName == "string")
                        ? (thrownValue is FabError fe ? fe.Message : (thrownValue is string s ? s : FormatValue(thrownValue)))
                        : thrownValue;

                    interpreter.Scope.Push();
                    try
                    {
                        interpreter.Scope.Define(varName, boundValue);
                        foreach (var stmt in cbody) stmt.Eval(interpreter);
                    }
                    finally { interpreter.Scope.Pop(); }
                    return;
                }
                throw; // no catch clause matched — propagate
            }
        }

        private static bool CatchMatches(object value, string? typeName)
        {
            if (typeName == null || typeName == "string") return true; // universal fallback
            return value is FabError fe && FabErrorTypes.IsOrDescendantOf(fe.TypeName, typeName);
        }
    }

    // ── Arithmetic / comparison / logical ─────────────────────────────────────

    public class FabBinOp : FabBase
    {
        public FabBase Left { get; }
        public string Op { get; }
        public FabBase Right { get; }
        public FabBinOp(FabBase left, string op, FabBase right) { Left = left; Op = op; Right = right; }

        public override object Eval(FabInterpreter interpreter)
        {
            // String concatenation with +
            var lv = Left.Eval(interpreter);
            var rv = Right.Eval(interpreter);

            if (Op == "+" && (lv is string || rv is string))
                return FormatValue(lv) + FormatValue(rv);

            var left = Convert.ToDouble(lv);
            var right = Convert.ToDouble(rv);
            return Op switch
            {
                "+" => left + right,
                "-" => left - right,
                "*" => left * right,
                "/" => left / right,
                "%" => left % right,
                "^" => Math.Pow(left, right),
                _ => throw new Exception($"Unknown operator: {Op}")
            };
        }
    }

    public class FabComparison : FabBase
    {
        public FabBase Left { get; }
        public string Op { get; }
        public FabBase Right { get; }
        public FabComparison(FabBase left, string op, FabBase right) { Left = left; Op = op; Right = right; }

        public override object Eval(FabInterpreter interpreter)
        {
            var left = Left.Eval(interpreter);
            var right = Right.Eval(interpreter);

            // null equality
            if (left is null || right is null)
                return Op switch
                {
                    "==" => left is null && right is null,
                    "!=" => !(left is null && right is null),
                    _ => throw new Exception($"Operator '{Op}' cannot be applied to null")
                };

            if (left is string l && right is string r)
                return Op switch { "==" => l == r, "!=" => l != r, _ => throw new Exception($"Operator '{Op}' cannot be applied to strings") };
            var ld = Convert.ToDouble(left);
            var rd = Convert.ToDouble(right);
            return Op switch
            {
                "==" => ld == rd,
                "!=" => ld != rd,
                "<" => ld < rd,
                ">" => ld > rd,
                "<=" => ld <= rd,
                ">=" => ld >= rd,
                _ => throw new Exception($"Unknown comparison operator: {Op}")
            };
        }
    }

    public class FabLogical : FabBase
    {
        public FabBase Left { get; }
        public string Op { get; }
        public FabBase Right { get; }
        public FabLogical(FabBase left, string op, FabBase right) { Left = left; Op = op; Right = right; }
        public override object Eval(FabInterpreter interpreter)
        {
            bool left = ToBool(Left.Eval(interpreter));
            return Op switch
            {
                "and" => left && ToBool(Right.Eval(interpreter)),
                "or" => left || ToBool(Right.Eval(interpreter)),
                _ => throw new Exception($"Unknown logical operator: {Op}")
            };
        }
        private static bool ToBool(object val) => val is bool b ? b : Convert.ToDouble(val) != 0;
    }

    // ── Control flow ──────────────────────────────────────────────────────────

    public class FabIf : FabBase
    {
        public List<(FabBase Condition, List<FabBase> Body)> Branches { get; }
        public List<FabBase>? ElseBody { get; }
        public FabIf(List<(FabBase, List<FabBase>)> branches, List<FabBase>? elseBody) { Branches = branches; ElseBody = elseBody; }

        public override object Eval(FabInterpreter interpreter)
        {
            foreach (var (condition, body) in Branches)
            {
                var result = condition.Eval(interpreter);
                bool cond = result is bool b ? b : Convert.ToDouble(result) != 0;
                if (cond)
                {
                    interpreter.Scope.Push();
                    try { foreach (var stmt in body) stmt.Eval(interpreter); }
                    finally { interpreter.Scope.Pop(); }
                    return null;
                }
            }
            if (ElseBody != null)
            {
                interpreter.Scope.Push();
                try { foreach (var stmt in ElseBody) stmt.Eval(interpreter); }
                finally { interpreter.Scope.Pop(); }
            }
            return null;
        }
    }

    // ── Assignment ────────────────────────────────────────────────────────────

    public class FabAssign : FabBase
    {
        public string Name { get; }
        public FabBase Expr { get; }
        public string? DeclaredType { get; }
        public FabAssign(string name, FabBase expr, string? declaredType = null) { Name = name; Expr = expr; DeclaredType = declaredType; }

        public override object Eval(FabInterpreter interpreter)
        {
            var value = Expr.Eval(interpreter);

            // Struct value semantics: always copy on assignment
            if (value is FabStructValue svCopy)
                value = svCopy.Clone();

            if (value is FabVoidSentinel)
                throw new Exception($"Cannot assign void to variable '{Name}': the function returns no value");

            // ── Typed declaration (int x = …, string s = …, etc.) ──
            // Always creates a new variable in the CURRENT (innermost) scope,
            // even if the same name exists in an outer scope — this is C++ shadowing.
            if (DeclaredType != null)
            {
                // ── Struct typed declaration: Pos cat = { x: 12, y: 56 }; ──
                // DeclaredType is the struct name (parsed as ID, not a keyword).
                // value is a FabDict produced by the { key: val } literal.
                if (interpreter.Structs.TryGetValue(DeclaredType, out var structDef))
                {
                    var sv = new FabStructValue(structDef, interpreter);
                    if (value is FabDict initDict)
                    {
                        foreach (var (k, v) in initDict.Pairs)
                        {
                            string fname = FormatValue(k);
                            if (!sv.Fields.ContainsKey(fname))
                                throw new Exception(
                                    $"Struct '{DeclaredType}' has no field '{fname}'." +
                                    DidYouMean(fname, sv.Fields.Keys));
                            sv.Fields[fname] = v;
                        }
                    }
                    else if (value is FabStructValue srcSv)
                    {
                        if (srcSv.StructDef.Name != DeclaredType)
                            throw new Exception(
                                $"Cannot initialise struct '{DeclaredType}' from struct '{srcSv.StructDef.Name}'");
                        sv = srcSv.Clone();
                    }
                    else if (value != null)
                    {
                        throw new Exception(
                            $"Type error: cannot assign {FormatValue(value)} to struct '{DeclaredType}' variable '{Name}'");
                    }
                    interpreter.Scope.Define(Name, sv);
                    return null;
                }

                value = CastDeclared(value, DeclaredType, Name);
                interpreter.Scope.Define(Name, value);
                return null;
            }

            // ── Plain assignment (no type keyword) ──
            // Must find the variable somewhere in the scope chain.
            if (interpreter.Scope.Contains(Name))
            {
                var slot = interpreter.Scope.GetSlot(Name);

                if (slot.IsConst)
                    throw new Exception($"Cannot assign to const '{Name}'");

                // ── Struct re-initialisation: pos = { x: 10, y: 20 }; ──
                // If the existing slot holds a FabStructValue and the incoming
                // value is a FabDict (produced by a { key: val } literal), apply
                // each dict entry as a field update rather than replacing the struct.
                if (slot.Value is FabStructValue existingSv && value is FabDict initDict)
                {
                    var sv = existingSv.Clone();
                    foreach (var (k, v) in initDict.Pairs)
                    {
                        string fname = AST.FormatValue(k);
                        if (!sv.Fields.ContainsKey(fname))
                            throw new Exception(
                                $"Struct '{sv.StructDef.Name}' has no field '{fname}'." +
                                AST.DidYouMean(fname, sv.Fields.Keys));
                        sv.Fields[fname] = v;
                    }
                    interpreter.Scope.SetSlot(Name, new VarSlot(sv, slot.IsConst, slot.IsLocked, slot.IsFree));
                    return null;
                }

                if (slot.IsLocked)
                    value = CastToSameType(slot.Value, value, Name);
                else if (slot.IsFree)
                    value = FabVFreeDecl.InferMinType(value);
                else
                    value = CastToSameType(slot.Value, value, Name);

                // Preserve mutability metadata when updating
                interpreter.Scope.SetSlot(Name, new VarSlot(value, slot.IsConst, slot.IsLocked, slot.IsFree));
                return null;
            }

            // ── Variable not found ──
            throw new Exception($"Variable '{Name}' is not defined");
        }

        // Public alias used by FabConst (which can't call private methods of another class)
        public static object CastDeclaredPublic(object value, string declaredType, string name)
            => CastDeclared(value, declaredType, name);

        // New: cast a freshly-declared typed value
        private static object CastDeclared(object value, string declaredType, string name)
        {
            // null is assignable to any non-primitive type
            if (value is null)
                return declaredType switch
                {
                    "int" or "short" or "long" or "uint" or "ushort" or "ulong"
                    or "byte" or "sbyte" or "float" or "double" or "bool" or "char" or "ldouble"
                        => throw new Exception($"Type error: cannot assign null to value-type '{declaredType}' variable '{name}'"),
                    _ => null
                };

            return declaredType switch
            {
                "int" => value is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to int '{name}'") : (object)(int)d) : Convert.ToInt32(value),
                "short" => value is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to short '{name}'") : (object)(short)d) : Convert.ToInt16(value),
                "long" => value is double d ? (d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to long '{name}'") : (object)(long)d) : Convert.ToInt64(value),
                "uint" => value is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot assign {value} to un int '{name}'") : (object)(uint)d) : Convert.ToUInt32(value),
                "ushort" => value is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot assign {value} to un short '{name}'") : (object)(ushort)d) : Convert.ToUInt16(value),
                "ulong" => value is double d ? (d % 1 != 0 || d < 0 ? throw new Exception($"Type error: cannot assign {value} to un long '{name}'") : (object)(ulong)d) : Convert.ToUInt64(value),
                "byte" => value is double d ? (d % 1 != 0 || d < 0 || d > 255 ? throw new Exception($"Type error: cannot assign {value} to byte '{name}'") : (object)(byte)d) : Convert.ToByte(value),
                "sbyte" => value is double d ? (d % 1 != 0 || d < sbyte.MinValue || d > sbyte.MaxValue ? throw new Exception($"Type error: cannot assign {value} to sbyte '{name}'") : (object)(sbyte)d) : Convert.ToSByte(value),
                "float" => (object)Convert.ToSingle(value),
                "double" => Convert.ToDouble(value),
                "ldouble" => Convert.ToDecimal(value),
                "bool" => value is bool ? value : throw new Exception($"Type error: cannot assign {value} to bool '{name}'"),
                "char" => value is char ? value : throw new Exception($"Type error: cannot assign {value} to char '{name}'"),
                "string" => value is string ? value : throw new Exception($"Type error: cannot assign {value} to string '{name}'"),
                string arrType when arrType.EndsWith("[]") =>
                    value is object[]? value : throw new Exception($"Type error: cannot assign non-array value to '{arrType}' variable '{name}'"),
                "tuple" => value is FabTuple
                    ? value
                    : throw new Exception($"Type error: cannot assign non-tuple value to tuple variable '{name}'"),
                _ => value
            };
        }

        private static object CastToSameType(object existing, object value, string name)
        {
            // null can be reassigned to any slot that already holds null or a reference type
            if (value is null)
                return existing switch
                {
                    int _ or short _ or long _ or uint _ or ushort _ or ulong _
                    or byte _ or sbyte _ or float _ or double or decimal _ or bool _ or char _
                        => throw new Exception($"Type error: cannot assign null to value-type variable '{name}'"),
                    _ => null
                };

            return existing switch
            {
                int _ => value is double d && d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to int '{name}'") : (object)(int)Convert.ToDouble(value),
                short _ => value is double d && d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to short '{name}'") : (object)(short)Convert.ToDouble(value),
                long _ => value is double d && d % 1 != 0 ? throw new Exception($"Type error: cannot assign {value} to long '{name}'") : (object)(long)Convert.ToDouble(value),
                uint _ => value is double d && (d % 1 != 0 || d < 0) ? throw new Exception($"Type error: cannot assign {value} to un int '{name}'") : (object)(uint)Convert.ToDouble(value),
                ushort _ => value is double d && (d % 1 != 0 || d < 0) ? throw new Exception($"Type error: cannot assign {value} to un short '{name}'") : (object)(ushort)Convert.ToDouble(value),
                ulong _ => value is double d && (d % 1 != 0 || d < 0) ? throw new Exception($"Type error: cannot assign {value} to un long '{name}'") : (object)(ulong)Convert.ToDouble(value),
                byte _ => value is double d && (d % 1 != 0 || d < 0 || d > 255) ? throw new Exception($"Type error: cannot assign {value} to byte '{name}'") : (object)(byte)Convert.ToDouble(value),
                sbyte _ => value is double d && (d % 1 != 0 || d < sbyte.MinValue || d > sbyte.MaxValue) ? throw new Exception($"Type error: cannot assign {value} to sbyte '{name}'") : (object)(sbyte)Convert.ToDouble(value),
                float _ => (object)Convert.ToSingle(value),
                double _ => Convert.ToDouble(value),
                decimal _ => Convert.ToDecimal(value),
                bool _ => value is bool ? value : throw new Exception($"Type error: cannot assign {value} to bool '{name}'"),
                char _ => value is string s && s.Length == 1 ? Convert.ToChar(s[0]) : throw new Exception($"Type error: cannot assign {value} to char '{name}'"),
                string _ => value is string ? value : throw new Exception($"Type error: cannot assign {value} to string '{name}'"),
                // Lists and dicts: allow reassignment without cast (they are reference types)
                FabList _ => value is FabList ? value : throw new Exception($"Type error: cannot assign non-list value to list '{name}'"),
                FabDict _ => value is FabDict ? value : throw new Exception($"Type error: cannot assign non-dict value to dict '{name}'"),
                _ => value
            };
        }
    }

    // ── Compound assignment ───────────────────────────────────────────────────

    public class FabCompoundAssign : FabBase
    {
        public string Name { get; }
        public string Op { get; }
        public FabBase? Expr { get; }
        public FabCompoundAssign(string name, string op, FabBase? expr = null) { Name = name; Op = op; Expr = expr; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.Contains(Name))
                throw new Exception($"Variable '{Name}' is not defined");

            var slot = interpreter.Scope.GetSlot(Name);
            if (slot.IsConst)
                throw new Exception($"Cannot modify const '{Name}'");

            var current = slot.Value;

            // ── String concatenation: str += "something" ──────────────────
            if (current is string strVal)
            {
                if (Op != "+=")
                    throw new Exception($"Operator '{Op}' cannot be applied to string '{Name}'");
                var appended = Expr?.Eval(interpreter)?.ToString() ?? "";
                interpreter.Scope.SetSlot(Name, new VarSlot(strVal + appended, slot.IsConst, slot.IsLocked, slot.IsFree));
                return null;
            }

            // ── Numeric path ──────────────────────────────────────────────
            var currentNum = Convert.ToDouble(current);
            var operand = Expr != null ? Convert.ToDouble(Expr.Eval(interpreter)) : 1.0;

            double result = Op switch
            {
                "++" => currentNum + 1,
                "--" => currentNum - 1,
                "+=" => currentNum + operand,
                "-=" => currentNum - operand,
                "*=" => currentNum * operand,
                "/=" => currentNum / operand,
                "%=" => currentNum % operand,
                "^=" => Math.Pow(currentNum, operand),
                _ => throw new Exception($"Unknown compound operator: {Op}")
            };

            object newVal = current switch
            {
                int _ => (object)(int)result,
                short _ => (short)result,
                long _ => (long)result,
                uint _ => (uint)result,
                ushort _ => (ushort)result,
                ulong _ => (ulong)result,
                float _ => (float)result,
                byte _ => (byte)result,
                sbyte _ => (object)(sbyte)result,
                _ => result
            };

            interpreter.Scope.SetSlot(Name, new VarSlot(newVal, slot.IsConst, slot.IsLocked, slot.IsFree));
            return null;
        }
    }

    // ── delete ────────────────────────────────────────────────────────────────

    public class FabDelete : FabBase
    {
        public string Name { get; }
        public FabDelete(string name) { Name = name; }
        public override object Eval(FabInterpreter interpreter)
        {
            if (interpreter.Scope.Contains(Name))
            {
                if (interpreter.Scope.GetSlot(Name).IsConst)
                    throw new Exception($"Cannot delete const '{Name}'");
                interpreter.Scope.Remove(Name);
                return null;
            }
            if (interpreter.Functions.ContainsKey(Name)) { interpreter.Functions.Remove(Name); return null; }
            throw new Exception($"'{Name}' is not defined");
        }
    }

    // ── input_in ──────────────────────────────────────────────────────────────

    public class FabInputIn : FabBase
    {
        public string VarName { get; }
        public string? DeclaredType { get; }
        public FabInputIn(string varName, string? declaredType = null) { VarName = varName; DeclaredType = declaredType; }

        public override object Eval(FabInterpreter interpreter)
        {
            if (DeclaredType != null)
            {
                // Must be an actual boxed value of the declared type — not a bare
                // C# null — otherwise the 'existing switch { int _ => ... }' below
                // can never match, and input silently falls through to the
                // untyped '_ => (object)input' arm, storing raw text with no
                // parsing or validation at all regardless of the declared type.
                object zero = DeclaredType switch
                {
                    "int" => 0,
                    "short" => (short)0,
                    "long" => 0L,
                    "uint" => 0u,
                    "ushort" => (ushort)0,
                    "ulong" => 0ul,
                    "float" => 0f,
                    "double" => 0.0,
                    "ldouble" => 0m,
                    "char" => '\0',
                    "byte" => (byte)0,
                    "sbyte" => (sbyte)0,
                    "bool" => false,
                    "string" => "",
                    _ => null
                };
                interpreter.Scope.Define(VarName, zero);
            }
            else if (!interpreter.Scope.Contains(VarName))
                throw new Exception($"Variable '{VarName}' is not defined. Use 'input_in int {VarName};' to declare inline.");

            string input = Console.ReadLine() ?? "";
            var slot = interpreter.Scope.GetSlot(VarName);
            var existing = slot.Value;

            object parsed = existing switch
            {
                int _ => int.TryParse(input, out int i) ? i : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not an int")),
                short _ => short.TryParse(input, out short sh) ? sh : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a short")),
                long _ => long.TryParse(input, out long l) ? l : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a long")),
                uint _ => uint.TryParse(input, out uint ui) ? ui : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not an un int")),
                ushort _ => ushort.TryParse(input, out ushort us) ? us : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not an un short")),
                ulong _ => ulong.TryParse(input, out ulong ul) ? ul : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not an un long")),
                float _ => float.TryParse(input, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out float f) ? f : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a float")),
                double _ => double.TryParse(input, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out double d) ? d : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a double")),
                decimal _ => decimal.TryParse(input, System.Globalization.NumberStyles.Any, System.Globalization.CultureInfo.InvariantCulture, out decimal d) ? d : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a long double")),
                char _ => char.TryParse(input, out char ch) ? ch : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a char")),
                byte _ => byte.TryParse(input, out byte by) ? by : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a byte (0-255)")),
                sbyte _ => sbyte.TryParse(input, out sbyte sby) ? sby : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not an un byte (-128-127)")),
                bool _ => bool.TryParse(input, out bool bl) ? bl : throw new FabThrownException(new FabError("ValueError", $"Input error: '{input}' is not a bool")),
                _ => (object)input
            };

            interpreter.Scope.SetSlot(VarName, new VarSlot(parsed, slot.IsConst, slot.IsLocked, slot.IsFree));
            return null;
        }
    }

    // ── type() / size_of() / system() ────────────────────────────────────────────────────

    public class FabType : FabBase
    {
        public string VarName { get; }
        public FabType(string varName) { VarName = varName; }
        public override object Eval(FabInterpreter interpreter)
        {
            if (!interpreter.Scope.Contains(VarName))
                throw new Exception($"Variable '{VarName}' is not defined");
            var slot = interpreter.Scope.GetSlot(VarName);
            var v = slot.Value;
            string rt = "";
            if (slot.IsConst) rt += "const ";
            if (v is null) return "null";
            if (v is FabVoidSentinel) return "void";
            if (v is FabInstance fi)
                return rt + fi.ClassDef.Name;
            if (v is FabError fe)
                return rt + fe.TypeName;
            if (v is FabStructValue fsv)
            {
                rt += $"struct {fsv.StructDef.Name}";
                return rt;
            }
            if (v is FabTuple ft)
            {
                rt += $"tuple({ft.Length})";
                return rt;
            }
            if (v is FabList fl)
            {
                string et = fl.ElementType.Replace("object", "vfree");
                rt += fl.Capacity == -1 ? $"list<{et}>[]" : $"list<{et}>[{fl.Capacity}]";
                return rt;
            }
            if (v is FabDict fd)
            {
                string kt = fd.KeyType.Replace("object", "vfree");
                string vt = fd.ValType.Replace("object", "vfree");
                rt += fd.Capacity == -1 ? $"dict<{kt}, {vt}>{{}}" : $"dict<{kt}, {vt}>{{{fd.Capacity}}}";
                return rt;
            }
            rt += v.GetType().Name.ToLower() switch
            {
                "int32" => "int",
                "int16" => "short",
                "int64" => "long",
                "uint32" => "un int",
                "uint16" => "un short",
                "uint64" => "un long",
                "single" => "float",
                "double" => "double",
                "decimal" => "long double",
                "char" => "char",
                "bool" => "bool",
                "string" => "string",
                "byte" => "byte",
                "sbyte" => "un byte",
                _ => v.GetType().Name.ToLower()
            };
            return rt;
        }
    }

    public class FabSizeOf : FabBase
    {
        public string VarName { get; }
        public FabSizeOf(string varName) { VarName = varName; }
        public override object Eval(FabInterpreter interpreter)
        {

            if (!interpreter.Scope.Contains(VarName))
            {
                if (interpreter.Classes.ContainsKey(VarName))
                    return (double)interpreter.Classes[VarName].Fields.Count;
                throw new Exception($"Variable '{VarName}' is not defined");
            }
            var v = interpreter.Scope.Get(VarName);
            if (v is FabList fl) return fl.Capacity != -1 ? (double)fl.Capacity : (double)fl.Items.Count;
            if (v is FabDict fd) return fd.Capacity != -1 ? (double)fd.Capacity : (double)fd.Pairs.Count;
            if (v is FabTuple ft2) return (double)ft2.Length;
            if (v is FabInstance fi) return (double)fi.ClassDef.Fields.Count;
            if (v is string s) return (double)s.Length;
            if (v is FabStructValue ft) return (double)ft.Fields.Count;
            return Marshal.SizeOf(v);
        }
    }

    public class FabSystem : FabBase
    {
        public string Command { get; }
        public FabSystem(string command) { Command = command; }
        public override object Eval(FabInterpreter interpreter)
        {
            Process.Start(Command);
            return null;
        }
    }

    // ── Loops ─────────────────────────────────────────────────────────────────

    public class FabWhile : FabBase
    {
        public FabBase Condition { get; }
        public List<FabBase> Body { get; }
        public FabWhile(FabBase condition, List<FabBase> body) { Condition = condition; Body = body; }
        public override object Eval(FabInterpreter interpreter)
        {
            while (true)
            {
                var result = Condition.Eval(interpreter);
                bool cond = result is bool b ? b : Convert.ToDouble(result) != 0;
                if (!cond) break;
                interpreter.Scope.Push();
                try
                {
                    foreach (var stmt in Body) stmt.Eval(interpreter);
                }
                catch (BreakException) { interpreter.Scope.Pop(); break; }
                catch (ContinueException) { interpreter.Scope.Pop(); continue; }
                interpreter.Scope.Pop();
            }
            return null;
        }
    }

    public class FabFor : FabBase
    {
        public FabBase Init { get; }
        public FabBase Condition { get; }
        public FabBase Step { get; }
        public List<FabBase> Body { get; }
        public string CounterName { get; }
        public FabFor(FabBase init, FabBase condition, FabBase step, List<FabBase> body, string counterName)
        { Init = init; Condition = condition; Step = step; Body = body; CounterName = counterName; }

        public override object Eval(FabInterpreter interpreter)
        {
            // The for-loop counter lives in its own scope pushed here,
            // so it is destroyed automatically when the loop exits — like C++.
            interpreter.Scope.Push();
            try
            {
                Init.Eval(interpreter);
                while (true)
                {
                    var result = Condition.Eval(interpreter);
                    bool cond = result is bool b ? b : Convert.ToDouble(result) != 0;
                    if (!cond) break;
                    interpreter.Scope.Push();
                    try
                    {
                        foreach (var stmt in Body) stmt.Eval(interpreter);
                    }
                    catch (BreakException) { interpreter.Scope.Pop(); break; }
                    catch (ContinueException) { interpreter.Scope.Pop(); Step.Eval(interpreter); continue; }
                    interpreter.Scope.Pop();
                    Step.Eval(interpreter);
                }
            }
            finally { interpreter.Scope.Pop(); }
            return null;
        }
    }

    // ── for (type name in collection) { ... }  /  for (name in collection) { ... } ──
    public class FabForEach : FabBase
    {
        public string VarName { get; }
        public string? DeclaredType { get; }
        public FabBase Collection { get; }
        public List<FabBase> Body { get; }

        public FabForEach(string varName, string? declaredType, FabBase collection, List<FabBase> body)
        { VarName = varName; DeclaredType = declaredType; Collection = collection; Body = body; }

        public override object Eval(FabInterpreter interpreter)
        {
            var col = Collection.Eval(interpreter);
            IEnumerable<object> items = col switch
            {
                FabList list => list.Items,
                object[] arr => arr,
                FabDict dict => dict.Pairs.Select(p => (object)new FabTuple(new object[] { p.Key, p.Value })).ToList(),
                string str => str.Select(c => (object)c).ToList(),
                FabTuple tup => tup.Items,
                _ => throw new Exception($"'for ... in' requires a list, dict, string, tuple or array, got '{col?.GetType().Name ?? "null"}'")
            };

            // The loop variable always lives in its own scope, fresh each run —
            // like a regular for-loop counter — so it shadows (and does not
            // permanently overwrite) any same-named variable from an outer scope.
            interpreter.Scope.Push();
            try
            {
                foreach (var item in items)
                {
                    object value = DeclaredType != null
                        ? FabAssign.CastDeclaredPublic(item, DeclaredType, VarName)
                        : item;
                    interpreter.Scope.Define(VarName, value);

                    interpreter.Scope.Push();
                    try { foreach (var stmt in Body) stmt.Eval(interpreter); }
                    catch (BreakException) { interpreter.Scope.Pop(); break; }
                    catch (ContinueException) { interpreter.Scope.Pop(); continue; }
                    interpreter.Scope.Pop();
                }
            }
            finally { interpreter.Scope.Pop(); }
            return null;
        }
    }

    // ── Generic wrapper used to bundle several statements produced from a
    //    single source construct (e.g. 'int a = 2, j;') into one FabBase ──────
    public class FabStatementList : FabBase
    {
        public List<FabBase> Statements { get; }
        public FabStatementList(List<FabBase> statements) { Statements = statements; }

        public override object Eval(FabInterpreter interpreter)
        {
            object result = null;
            foreach (var s in Statements) result = s.Eval(interpreter);
            return result;
        }
    }

    // ── break / continue ──────────────────────────────────────────────────────

    public class FabBreak : FabBase
    {
        public override object Eval(FabInterpreter interpreter) => throw new BreakException();
    }

    public class FabContinue : FabBase
    {
        public override object Eval(FabInterpreter interpreter) => throw new ContinueException();
    }

    // ── is (type check) ───────────────────────────────────────────────────────
    // expr is int / string / bool / char / float / double / list / dict / null

    public class FabIs : FabBase
    {
        public FabBase Expr { get; }
        public string TypeName { get; }
        public FabIs(FabBase expr, string typeName) { Expr = expr; TypeName = typeName; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Expr.Eval(interpreter);
            return IsOfType(val, TypeName, interpreter);
        }

        /// <summary>
        /// Real type check, not just a CLR-type tag check. Numeric literals are
        /// stored internally as a C# double until they pass through a typed
        /// declaration or cast, so a naive "val is int" check would make
        /// something as simple as "42 is int" come back false. This instead
        /// recognises whole-number doubles/floats that fit the requested
        /// integer range, in addition to values already narrowed to that type.
        /// </summary>
        public static bool IsOfType(object val, string typeName, FabInterpreter interpreter)
        {
            switch (typeName)
            {
                case "int": return val is int || (IsWhole(val, out double di) && di >= int.MinValue && di <= int.MaxValue);
                case "short": return val is short || (IsWhole(val, out double ds) && ds >= short.MinValue && ds <= short.MaxValue);
                case "long": return val is long || IsWhole(val, out _);
                case "uint": return val is uint || (IsWhole(val, out double dui) && dui >= 0 && dui <= uint.MaxValue);
                case "ushort": return val is ushort || (IsWhole(val, out double dus) && dus >= 0 && dus <= ushort.MaxValue);
                case "ulong": return val is ulong || (IsWhole(val, out double dul) && dul >= 0);
                case "byte": return val is byte || (IsWhole(val, out double db) && db >= 0 && db <= 255);
                case "sbyte": return val is sbyte || (IsWhole(val, out double dsb) && dsb >= sbyte.MinValue && dsb <= sbyte.MaxValue);
                case "float": return val is float or double or int or short or long or uint or ushort or ulong or byte or sbyte;
                case "double": return val is double or float or int or short or long or uint or ushort or ulong or byte or sbyte;
                case "ldouble": return val is decimal;
                case "bool": return val is bool;
                case "char": return val is char;
                case "string": return val is string;
                case "list": return val is FabList;
                case "dict": return val is FabDict;
                case "tuple": return val is FabTuple;
                case "struct": return val is FabStructValue;
                case "null": return val is null;
                case "void": return val is FabVoidSentinel;
                // built-in error type name — check native FabError hierarchy
                case var t when FabErrorTypes.IsKnown(t):
                    return val is FabError fe && FabErrorTypes.IsOrDescendantOf(fe.TypeName, t);
                // class name — check instance class hierarchy
                default: return val is FabInstance inst && IsInstanceOf(inst, typeName, interpreter);
            }
        }

        /// <summary>True if val is (or numerically represents) a whole number; outputs it as a double.</summary>
        private static bool IsWhole(object val, out double d)
        {
            d = 0;
            switch (val)
            {
                case int or short or long or uint or ushort or ulong or byte or sbyte:
                    d = Convert.ToDouble(val);
                    return true;
                case double dd:
                    d = dd;
                    return !double.IsNaN(dd) && !double.IsInfinity(dd) && dd == Math.Truncate(dd);
                case float ff:
                    d = ff;
                    return !float.IsNaN(ff) && !float.IsInfinity(ff) && ff == Math.Truncate(ff);
                default:
                    return false;
            }
        }

        private static bool IsInstanceOf(FabInstance inst, string typeName, FabInterpreter interpreter)
        {
            // Walk up the declared class hierarchy
            var current = inst.ClassDef;
            while (current != null)
            {
                if (current.Name == typeName) return true;
                current = current.BaseClassName != null && interpreter.Classes.TryGetValue(current.BaseClassName, out var base_)
                    ? base_ : null;
            }
            return false;
        }
    }

    // ── in (membership test) ─────────────────────────────────────────────────
    // expr in list / dict / string / array

    public class FabIn : FabBase
    {
        public FabBase Item { get; }
        public FabBase Collection { get; }
        public FabIn(FabBase item, FabBase collection) { Item = item; Collection = collection; }

        public override object Eval(FabInterpreter interpreter)
        {
            var item = Item.Eval(interpreter);
            var col = Collection.Eval(interpreter);

            if (col is FabTuple tupleCol)
                return tupleCol.Items.Any(x => ValuesEqual(x, item));

            if (col is FabList list)
                return list.Items.Any(x => ValuesEqual(x, item));

            if (col is FabDict dict)
                return dict.ContainsKey(item);

            if (col is string str)
            {
                string search = item is char c ? c.ToString() : item?.ToString() ?? "";
                return str.Contains(search);
            }

            if (col is object[] arr)
                return arr.Any(x => ValuesEqual(x, item));

            throw new Exception($"'in' requires a list, dict, string or array on the right side");
        }

        private static bool ValuesEqual(object a, object b)
        {
            if (a is null && b is null) return true;
            if (a is null || b is null) return false;
            // Compare via formatted string to handle numeric type differences
            return AST.FormatValue(a) == AST.FormatValue(b);
        }
    }

    // ── switch ────────────────────────────────────────────────────────────────

    /// <summary>
    /// switch (expr) { case v1, v2: stmts  case v3: stmts  default: stmts }
    /// Supports multiple comma-separated values per case.
    /// No implicit fall-through: each case ends at the next case/default/}.
    /// </summary>
    public class FabSwitch : FabBase
    {
        public FabBase Expr { get; }
        public List<(List<FabBase> Values, List<FabBase> Body)> Cases { get; }
        public List<FabBase>? DefaultBody { get; }

        public FabSwitch(
            FabBase expr,
            List<(List<FabBase>, List<FabBase>)> cases,
            List<FabBase>? defaultBody)
        { Expr = expr; Cases = cases; DefaultBody = defaultBody; }

        public override object Eval(FabInterpreter interpreter)
        {
            var val = Expr.Eval(interpreter);

            foreach (var (values, body) in Cases)
            {
                bool matched = values.Any(v =>
                {
                    var cv = v.Eval(interpreter);
                    // Numeric comparison via double; string/bool/char via formatted string
                    if (val is null && cv is null) return true;
                    if (val is null || cv is null) return false;
                    if (val is bool || cv is bool)
                        return FormatValue(val) == FormatValue(cv);
                    try { return Convert.ToDouble(val) == Convert.ToDouble(cv); }
                    catch { return FormatValue(val) == FormatValue(cv); }
                });

                if (matched)
                {
                    interpreter.Scope.Push();
                    try { foreach (var stmt in body) stmt.Eval(interpreter); }
                    catch (BreakException) { }
                    finally { interpreter.Scope.Pop(); }
                    return null;
                }
            }

            if (DefaultBody != null)
            {
                interpreter.Scope.Push();
                try { foreach (var stmt in DefaultBody) stmt.Eval(interpreter); }
                catch (BreakException) { }
                finally { interpreter.Scope.Pop(); }
            }
            return null;
        }
    }

    // ── Output ────────────────────────────────────────────────────────────────

    public class FabWrite : FabBase
    {
        public FabBase Expr { get; }
        public FabWrite(FabBase expr) { Expr = expr; }
        public override object Eval(FabInterpreter interpreter)
        { Console.Write(FormatValue(Expr.Eval(interpreter))); return null; }
    }

    public class FabWriteln : FabBase
    {
        public FabBase Expr { get; }
        public FabWriteln(FabBase expr) { Expr = expr; }
        public override object Eval(FabInterpreter interpreter)
        { Console.WriteLine(FormatValue(Expr.Eval(interpreter))); return null; }
    }

    /// <summary>
    /// Shared value-to-string formatter: handles bool, arrays, FabList, FabDict,
    /// and all primitives.
    /// </summary>
    public static string FormatValue(object val) => val switch
    {
        null => "null",
        FabVoidSentinel => "",
        bool b => b ? "true" : "false",
        double d when d == Math.Floor(d) && !double.IsInfinity(d) => ((long)d).ToString(),
        decimal de when de == Math.Floor(de) => ((long)de).ToString(),
        float f when f == Math.Floor(f) && !float.IsInfinity(f) => ((long)f).ToString(),
        float f => f.ToString(CultureInfo.InvariantCulture),
        object[] arr => "[" + string.Join(", ", arr.Select(FormatValue)) + "]",
        FabList list => list.ToString(),
        FabDict dict => dict.ToString(),
        FabInstance fi => fi.ToString(),
        FabTuple tup => tup.ToString(),
        FabStructValue sv => sv.ToString(),
        FabColor col => col.ToString(),
        FabError fe => fe.Message,
        _ => val.ToString() ?? "null"
    };

    // ── "Did you mean?" helper ────────────────────────────────────────────────

    /// <summary>
    /// Returns a " Did you mean 'x'?" hint string when a close match is found
    /// among <paramref name="candidates"/>, or an empty string when nothing is
    /// close enough to be useful.
    /// Uses the Levenshtein distance; the threshold scales with word length so
    /// short names need a tighter match than long ones.
    /// </summary>
    public static string DidYouMean(string input, IEnumerable<string> candidates)
    {
        if (string.IsNullOrEmpty(input)) return "";

        string? best = null;
        int bestDist = int.MaxValue;

        foreach (var c in candidates)
        {
            int d = Levenshtein(input, c);
            if (d < bestDist) { bestDist = d; best = c; }
        }

        // Allow 1 edit per ~4 characters, minimum threshold 1, maximum 3
        int threshold = Math.Clamp(input.Length / 4, 1, 3);
        return (best != null && bestDist <= threshold && best != input)
            ? $" Did you mean '{best}'?"
            : "";
    }

    private static int Levenshtein(string a, string b)
    {
        int n = a.Length, m = b.Length;
        if (n == 0) return m;
        if (m == 0) return n;
        var prev = new int[m + 1];
        var curr = new int[m + 1];
        for (int j = 0; j <= m; j++) prev[j] = j;
        for (int i = 1; i <= n; i++)
        {
            curr[0] = i;
            for (int j = 1; j <= m; j++)
            {
                int cost = a[i - 1] == b[j - 1] ? 0 : 1;
                curr[j] = Math.Min(Math.Min(curr[j - 1] + 1, prev[j] + 1), prev[j - 1] + cost);
            }
            (prev, curr) = (curr, prev);
        }
        return prev[m];
    }

    // ── Library registry ──────────────────────────────────────────────────────

    public static class FabLibRegistry
    {
        // ── Library-exposed types: classes & structs ────────────────────────
        // A builtin library (registered here in C#) can expose full Fab-language
        // classes and structs — not just C#-implemented functions — so that
        // 'use libName;' pulls in real, working types with method bodies written
        // in actual Fab source. These types are executed by the interpreter
        // exactly like a user-defined class (FabUse.Eval merges them into
        // interpreter.Classes / interpreter.Structs), and are transpiled by
        // FabCCompiler exactly like a class declared in the program's own source
        // (see FabCCompiler.CollectLibTypeDecls / EmitLibTypeDecls / EmitLibClassMethods).
        public static readonly Dictionary<string, Dictionary<string, FabClasses>> LibClasses = new();
        public static readonly Dictionary<string, Dictionary<string, FabStructDef>> LibStructs = new();

        public static void RegisterLibClass(string libName, FabClasses classDef)
        {
            if (!LibClasses.TryGetValue(libName, out var map))
                LibClasses[libName] = map = new Dictionary<string, FabClasses>();
            map[classDef.Name] = classDef;
        }

        public static void RegisterLibStruct(string libName, FabStructDef structDef)
        {
            if (!LibStructs.TryGetValue(libName, out var map))
                LibStructs[libName] = map = new Dictionary<string, FabStructDef>();
            map[structDef.Name] = structDef;
        }

        public static bool HasLibClasses(string libName) => LibClasses.ContainsKey(libName);
        public static bool HasLibStructs(string libName) => LibStructs.ContainsKey(libName);
        public static bool HasLibTypes(string libName) => HasLibClasses(libName) || HasLibStructs(libName);

        /// <summary>
        /// Parses a small Fab source fragment containing one or more top-level
        /// 'class'/'struct' definitions and registers every type it finds under
        /// <paramref name="libName"/>. This lets a builtin library ship real
        /// Fab-language types (with genuine method bodies, inheritance, etc.)
        /// instead of C# delegates, so the types behave identically — and are
        /// equally compilable to C++ — whether they came from 'use "file.fab";'
        /// or from a library baked into the interpreter itself.
        /// </summary>
        public static void RegisterLibTypesFromSource(string libName, string fabSource)
        {
            var tokens = new Lexer().Lex(fabSource).ToList();
            var stmts = new Parser(tokens).ParseStatements();
            foreach (var stmt in stmts)
            {
                if (stmt is FabClasses cl) RegisterLibClass(libName, cl);
                else if (stmt is FabStructDefStmt sd) RegisterLibStruct(libName, sd.Def);
            }
        }

        public static readonly Dictionary<string, Dictionary<string, Func<string[], object>>> Libs = new()
        {
            ["math"] = new Dictionary<string, Func<string[], object>>
            {
                ["pow"] = args => { if (args.Length != 2) throw new Exception("math.pow requires 2 arguments"); return Math.Pow(Convert.ToDouble(args[0]), Convert.ToDouble(args[1])); },
                ["sqrt"] = args => { if (args.Length != 1) throw new Exception("math.sqrt requires 1 argument"); return Math.Sqrt(Convert.ToDouble(args[0])); },
                ["cbrt"] = args => { if (args.Length != 1) throw new Exception("math.cbrt requires 1 argument"); return Math.Cbrt(Convert.ToDouble(args[0])); },
                ["sin"] = args => { if (args.Length < 1) throw new Exception("math.sin requires at least 1 argument"); return Math.Sin(Convert.ToDouble(args[0])); },
                ["cos"] = args => { if (args.Length < 1) throw new Exception("math.cos requires at least 1 argument"); return Math.Cos(Convert.ToDouble(args[0])); },
                ["tan"] = args =>
                {
                    if (args.Length == 2)
                    {
                        if (Convert.ToDouble(args[1]) == 0) throw new Exception("math.tg: cos is zero, tg is undefined");
                        return Convert.ToDouble(args[0]) / Convert.ToDouble(args[1]);
                    }
                    if (args.Length == 1) return Math.Tan(Convert.ToDouble(args[0]));
                    throw new Exception("math.tg requires 1 or 2 arguments");
                },
                ["abs"] = args => { if (args.Length != 1) throw new Exception("math.abs requires 1 argument"); return Math.Abs(Convert.ToDouble(args[0])); },
                ["floor"] = args => { if (args.Length != 1) throw new Exception("math.floor requires 1 argument"); return Math.Floor(Convert.ToDouble(args[0])); },
                ["ceil"] = args => { if (args.Length != 1) throw new Exception("math.ceil requires 1 argument"); return Math.Ceiling(Convert.ToDouble(args[0])); },
                ["round"] = args => { if (args.Length != 1) throw new Exception("math.round requires 1 argument"); return Math.Round(Convert.ToDouble(args[0])); },
                ["log"] = args =>
                {
                    if (args.Length == 1) return Math.Log(Convert.ToDouble(args[0]));
                    if (args.Length == 2) return Math.Log(Convert.ToDouble(args[0]), Convert.ToDouble(args[1]));
                    throw new Exception("math.log requires 1 or 2 arguments");
                },
                ["max"] = args => { if (args.Length != 2) throw new Exception("math.max requires 2 arguments"); return Math.Max(Convert.ToDouble(args[0]), Convert.ToDouble(args[1])); },
                ["min"] = args => { if (args.Length != 2) throw new Exception("math.min requires 2 arguments"); return Math.Min(Convert.ToDouble(args[0]), Convert.ToDouble(args[1])); },
                ["clamp"] = args => { if (args.Length != 3) throw new Exception("math.clamp requires 3 arguments"); return Math.Clamp(Convert.ToDouble(args[0]), Convert.ToDouble(args[1]), Convert.ToDouble(args[2])); },
                ["rad"] = args => { if (args.Length != 1) throw new Exception("math.rad requires 1 arguments"); return Convert.ToDouble(args[0]) * (Math.PI / 180.0); },
                ["deg"] = args => { if (args.Length != 1) throw new Exception("math.deg requires 1 arguments"); return Convert.ToDouble(args[0]) * (180.0 / Math.PI); },
                ["atan2"] = args => { if (args.Length != 2) throw new Exception("math.atan2 requires 2 arguments"); return Math.Atan2(Convert.ToDouble(args[0]), Convert.ToDouble(args[1])); },
                ["sign"] = args => { if (args.Length != 1) throw new Exception("math.sing requires 1 arguments"); return Math.Sign(Convert.ToDouble(args[0])); },
                ["lerp"] = args =>
                {
                    if (args.Length != 3) throw new Exception("math.lerp requires 3 arguments");
                    double a = Convert.ToDouble(args[0]);
                    double b = Convert.ToDouble(args[1]);
                    double t = Convert.ToDouble(args[2]);
                    return a + (b - a) * t;
                },
                ["hypot"] = args =>
                {
                    if (args.Length != 2) throw new Exception("math.hypot requires 2 arguments");
                    return Math.Sqrt(Math.Pow(Convert.ToDouble(args[0]), 2) + Math.Pow(Convert.ToDouble(args[1]), 2));
                },
                ["gamma"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.gamma requires 1 arguments");
                    return Gamma(Convert.ToDouble(args[0]));
                },
                ["log_gamma"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.log_gamma requires 1 arguments");
                    return LogGamma(Convert.ToDouble(args[0]));
                },
                ["beta"] = args =>
                {
                    if (args.Length != 2) throw new Exception("math.beta requires 2 arguments");
                    double x = Convert.ToDouble(args[0]);
                    double y = Convert.ToDouble(args[1]);
                    return Math.Exp(LogGamma(x) + LogGamma(y) - LogGamma(x + y));
                },
                ["betal"] = args =>
                {
                    if (args.Length != 2) throw new Exception("math.betal requires 2 arguments");
                    double x = Convert.ToInt64(args[0]);
                    double y = Convert.ToInt64(args[1]);
                    return Convert.ToInt64(Math.Exp(LogGamma(x) + LogGamma(y) - LogGamma(x + y)));
                },
                ["exp"] = args => { if (args.Length != 1) throw new Exception("math.exp requires 1 argument"); return Math.Exp(Convert.ToDouble(args[0])); },
                ["expm"] = args => { if (args.Length != 1) throw new Exception("math.expm requires 1 argument"); return Math.Pow(2.7182818, Convert.ToDouble(args[0])) - 1; },
                ["factorial"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.factorial requires 1 arguments");
                    int answer = 1;
                    for (int i = 1; i <= Convert.ToDouble(args[0]); i++) { answer *= i; }
                    return answer;
                },
                ["factoriall"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.factorial requires 1 arguments");
                    long answer = 1;
                    for (long i = 1; i <= Convert.ToDouble(args[0]); i++) { answer *= i; }
                    return answer;
                },
                ["factoriald"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.factorial requires 1 arguments");
                    double answer = 1;
                    for (double i = 1; i <= Convert.ToDouble(args[0]); i++) { answer *= i; }
                    return answer;
                },
                ["factorialld"] = args =>
                {
                    if (args.Length != 1) throw new Exception("math.factorial requires 1 arguments");
                    decimal answer = 1;
                    for (decimal i = 1; i <= Convert.ToDecimal(args[0]); i++) { answer *= i; }
                    return answer;
                },
                ["pi"] = args => Math.PI,
                ["e"] = args => Math.E,
                ["tau"] = args => Math.Tau,
                ["phi"] = args => 1.6180339887498948,
                ["gam"] = args => 0.5772156649015328,
                ["g"] = args => 0.9159655941772190,
                ["sqrt2"] = args => 1.4142135623,
                ["negativ_zero"] = args => double.NegativeZero,
            },
            ["random"] = new Dictionary<string, Func<string[], object>>
            {
                ["randi"] = args => { if (args.Length != 2) throw new Exception("random.randi requires 2 arguments"); var rb = new Random(); return rb.Next(Convert.ToInt32(args[0]), Convert.ToInt32(args[1]) + 1); },
                ["randl"] = args => { if (args.Length != 2) throw new Exception("random.randl requires 2 arguments"); var rb = new Random(); return rb.NextInt64(Convert.ToInt64(args[0]), Convert.ToInt64(args[1]) + 1); },
                ["randf"] = args =>
                {
                    if (args.Length != 2) throw new Exception("random.randf requires 2 arguments");
                    float from = Convert.ToSingle(args[0]), to = Convert.ToSingle(args[1]);
                    if (from > to) throw new Exception("random.randf: 'from' must be <= 'to'");
                    return (object)(from + (float)new Random().NextDouble() * (to - from));
                },
                ["randd"] = args =>
                {
                    if (args.Length != 2) throw new Exception("random.randd requires 2 arguments");
                    double from = Convert.ToDouble(args[0]), to = Convert.ToDouble(args[1]);
                    if (from > to) throw new Exception("random.randd: 'from' must be <= 'to'");
                    return from + new Random().NextDouble() * (to - from);
                },
                ["randb"] = args =>
                {
                    if (args.Length != 2) throw new Exception("random.randb requires 2 arguments");
                    int from = Convert.ToInt32(args[0]), to = Convert.ToInt32(args[1]);
                    if (from < 0 || to > 255) throw new Exception("random.randb: arguments must be in range 0..255");
                    if (from > to) throw new Exception("random.randb: 'from' must be <= 'to'");
                    return (object)(byte)new Random().Next(from, to + 1);
                },
                ["rands"] = args =>
                {
                    if (args.Length != 2) throw new Exception("random.rands requires 2 arguments");
                    short from = Convert.ToInt16(args[0]), to = Convert.ToInt16(args[1]);
                    if (from > to) throw new Exception("random.rands: 'from' must be <= 'to'");
                    return (object)(short)new Random().Next(from, to + 1);
                },
            },
            ["date"] = new Dictionary<string, Func<string[], object>>
            {
                ["date_now"] = args => DateTime.Now.ToString("d"),
                ["time_now"] = args => DateTime.Now.ToString("T"),
                ["day_now"] = args => Convert.ToInt32(DateTime.Now.ToString("dd")),
                ["month_now"] = args => Convert.ToInt32(DateTime.Now.ToString("MM")),
                ["year_now"] = args => Convert.ToInt32(DateTime.Now.ToString("yy")),
                ["sec_now"] = args => Convert.ToInt32(DateTime.Now.ToString("ss")),
                ["min_now"] = args => Convert.ToInt32(DateTime.Now.ToString("mm")),
                ["hours_now"] = args => Convert.ToInt32(DateTime.Now.ToString("T").Substring(0, 2)),
                ["now"] = args => { if (args.Length != 1) throw new Exception("data.now requires 1 argument"); return DateTime.Now.ToString(args[0]); },
                ["max_value"] = args => DateTime.MaxValue.ToString(),
                ["min_value"] = args => DateTime.MinValue.ToString(),
            },
            ["io"] = new Dictionary<string, Func<string[], object>>
            {
                ["read_file"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.read_file requires 1 argument (path)");
                    if (!File.Exists(args[0])) throw new Exception($"io.read_file: file not found '{args[0]}'");
                    return File.ReadAllText(args[0]);
                },
                ["write_file"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.write_file requires 2 arguments (path, text)");
                    File.WriteAllText(args[0], args[1]);
                    return (object)"";
                },
                ["append_file"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.append_file requires 2 arguments (path, text)");
                    File.AppendAllText(args[0], args[1]);
                    return (object)"";
                },
                ["delete_file"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.delete_file requires 1 argument (path)");
                    if (!File.Exists(args[0])) throw new Exception($"io.delete_file: file not found '{args[0]}'");
                    File.Delete(args[0]);
                    return (object)"";
                },
                ["file_exists"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.file_exists requires 1 argument (path)");
                    return (object)(File.Exists(args[0]) ? "true" : "false");
                },
                ["read_lines"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.read_lines requires 1 argument (path)");
                    if (!File.Exists(args[0])) throw new Exception($"io.read_lines: file not found '{args[0]}'");
                    return string.Join("\n", File.ReadAllLines(args[0]));
                },
                ["write_lines"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.write_lines requires 2 arguments (path, text)");
                    File.AppendAllText(args[0], args[1] + Environment.NewLine);
                    return (object)"";
                },
                ["copy_file"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.copy_file requires 2 arguments (src, dst)");
                    if (!File.Exists(args[0])) throw new Exception($"io.copy_file: source not found '{args[0]}'");
                    File.Copy(args[0], args[1], overwrite: true);
                    return (object)"";
                },
                ["move_file"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.move_file requires 2 arguments (src, dst)");
                    if (!File.Exists(args[0])) throw new Exception($"io.move_file: source not found '{args[0]}'");
                    File.Move(args[0], args[1], overwrite: true);
                    return (object)"";
                },
                ["dir_exists"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.dir_exists requires 1 argument (path)");
                    return (object)(Directory.Exists(args[0]) ? "true" : "false");
                },
                ["create_dir"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.create_dir requires 1 argument (path)");
                    Directory.CreateDirectory(args[0]);
                    return (object)"";
                },
                ["delete_dir"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.delete_dir requires 1 argument (path)");
                    if (!Directory.Exists(args[0])) throw new Exception($"io.delete_dir: directory not found '{args[0]}'");
                    Directory.Delete(args[0], recursive: true);
                    return (object)"";
                },
                ["list_files"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.list_files requires 1 argument (path)");
                    if (!Directory.Exists(args[0])) throw new Exception($"io.list_files: directory not found '{args[0]}'");
                    return string.Join(",", Directory.GetFiles(args[0]).Select(Path.GetFileName));
                },
                ["list_dirs"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.list_dirs requires 1 argument (path)");
                    if (!Directory.Exists(args[0])) throw new Exception($"io.list_dirs: directory not found '{args[0]}'");
                    return string.Join(",", Directory.GetDirectories(args[0]).Select(Path.GetFileName));
                },
                ["path_join"] = args =>
                {
                    if (args.Length != 2) throw new Exception("io.path_join requires 2 arguments (a, b)");
                    return Path.Combine(args[0], args[1]);
                },
                ["get_ext"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.get_ext requires 1 argument (path)");
                    return Path.GetExtension(args[0]);
                },
                ["get_name"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.get_name requires 1 argument (path)");
                    return Path.GetFileNameWithoutExtension(args[0]);
                },
                ["file_size"] = args =>
                {
                    if (args.Length != 1) throw new Exception("io.file_size requires 1 argument (path)");
                    if (!File.Exists(args[0])) throw new Exception($"io.file_size: file not found '{args[0]}'");
                    return new FileInfo(args[0]).Length.ToString();
                },
            },

            ["console"] = new Dictionary<string, Func<string[], object>>
            {
                ["clear"] = args =>
                {
                    Console.Clear();
                    return (object)"";
                },
                ["set_fg"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.set_fg requires 1 argument (color name)");
                    if (!Enum.TryParse<ConsoleColor>(args[0], ignoreCase: true, out var color))
                        throw new Exception($"console.set_color: unknown color '{args[0]}'");
                    Console.ForegroundColor = color;
                    return (object)"";
                },
                ["set_bg"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.set_bg requires 1 argument (color name)");
                    if (!Enum.TryParse<ConsoleColor>(args[0], ignoreCase: true, out var color))
                        throw new Exception($"console.set_bg: unknown color '{args[0]}'");
                    Console.BackgroundColor = color;
                    return (object)"";
                },
                ["reset_color"] = args =>
                {
                    Console.ResetColor();
                    return (object)"";
                },
                ["set_title"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.set_title requires 1 argument (title)");
                    Console.Title = args[0];
                    return (object)"";
                },
                ["beep"] = args =>
                {
                    Console.Beep();
                    return (object)"";
                },
                ["set_cursor"] = args =>
                {
                    if (args.Length != 2) throw new Exception("console.set_cursor requires 2 arguments (x, y)");
                    Console.SetCursorPosition(Convert.ToInt32(args[0]), Convert.ToInt32(args[1]));
                    return (object)"";
                },
                ["get_cursor_x"] = args => { return Console.GetCursorPosition().Left; },
                ["get_cursor_y"] = args => { return Console.GetCursorPosition().Top; },
                ["get_cursor_position"] = args => { return Console.GetCursorPosition(); },
                ["hide_cursor"] = args =>
                {
                    Console.CursorVisible = false;
                    return (object)"";
                },
                ["show_cursor"] = args =>
                {
                    Console.CursorVisible = true;
                    return (object)"";
                },
                ["width"] = args => Console.WindowWidth,
                ["height"] = args => Console.WindowHeight,
                ["bold"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.bold requires 1 argument (text)");
                    return $"\x1b[1m{args[0]}\x1b[0m";
                },
                ["underline"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.underline requires 1 argument (text)");
                    return $"\x1b[4m{args[0]}\x1b[0m";
                },
                ["blink"] = args =>
                {
                    if (args.Length != 1) throw new Exception("console.blink requires 1 argument (text)");
                    return $"\x1b[5m{args[0]}\x1b[0m";
                },
            },
            ["environment"] = new Dictionary<string, Func<string[], object>>
            {
                ["sdelay"] = args =>
                {
                    if (args.Length != 1) throw new Exception("environment.delay requires 1 argument");
                    Task.Delay(Convert.ToInt32(args[0]) * 1000).Wait();
                    return (object)"";
                },
                ["mdelay"] = args =>
                {
                    if (args.Length != 1) throw new Exception("environment.delay requires 1 argument");
                    Task.Delay(Convert.ToInt32(args[0])).Wait();
                    return (object)"";
                },
                ["exit"] = args =>
                {
                    if (args.Length != 1) throw new Exception("environment.exit requires 1 argument");
                    Environment.Exit(0);
                    return (object)"";
                },
                ["compute"] = args =>
                {
                    if (args.Length != 2) throw new Exception("environment.compute requires 2 argument");
                    DataTable dt = new DataTable();
                    return dt.Compute(args[0], args[1]);
                },
                ["new_line"] = args => Environment.NewLine,
                ["stack_trace"] = args => Environment.StackTrace,
                ["exit_code"] = args => Environment.ExitCode,
                ["machine_name"] = args => Environment.MachineName,
                ["os_ver"] = args => Environment.OSVersion.Version.ToString(),
                ["os_name"] = args => Environment.OSVersion.Platform.ToString(),
                ["service_pack"] = args => Environment.OSVersion.ServicePack,
                ["current_directory"] = args => Environment.CurrentDirectory,
                ["com_line"] = args => Environment.CommandLine,
                ["process_id"] = args => Environment.ProcessId,
                ["process_count"] = args => Environment.ProcessorCount,
                ["process_path"] = args => Environment.ProcessPath,
                ["username"] = args => Environment.UserName,
                ["user_interactive"] = args => Environment.UserInteractive,
                ["user_domain_name"] = args => Environment.UserDomainName,
                ["tick_count"] = args => Environment.TickCount,
                ["working_set"] = args => Environment.WorkingSet,
            },
            ["ipaddress"] = new Dictionary<string, Func<string[], object>>
            {
                ["IPv6_any"] = args => IPAddress.IPv6Any,
                ["IPv6_loopback"] = args => IPAddress.IPv6Loopback,
                ["IPv6_none"] = args => IPAddress.IPv6None,
                ["loopback"] = args => IPAddress.Loopback,
                ["broadcast"] = args => IPAddress.Broadcast,
                ["none"] = args => IPAddress.None,
                ["is_loopback"] = args => { return IPAddress.IsLoopback(IPAddress.Parse(args[0])); },
                ["host_to_network_order"] = args => { return IPAddress.HostToNetworkOrder(Convert.ToInt32(args[0])); },
                ["network_to_host_order"] = args => { return IPAddress.NetworkToHostOrder(Convert.ToInt16(args[0])); },
                ["parse"] = args => { return IPAddress.Parse(args[0]); },
            },
        };

        // ── Demo library types (registered via RegisterLibTypesFromSource) ──
        // These run once at type-init time, after the 'Libs' field initializer
        // above has already populated the function table (so 'math.sqrt(...)'
        // calls inside the class body below resolve correctly at parse time,
        // since FabLibRegistry.HasLib('math') is already true by then).
        static FabLibRegistry()
        {
            RegisterBuiltinLibTypes();
        }

        private static void RegisterBuiltinLibTypes()
        {

        }

        // Вычисление Гамма функции
        private static double Gamma(double x)
        {
            double[] p = { 676.5203681218851, -1259.1392167224028, 771.32342877765313,
                   -176.61502916214059, 12.507343278686905, -0.13857109526572012,
                   9.9843695780195716e-6, 1.5056327351493116e-7 };

            if (x < 0.5)
                return Math.PI / (Math.Sin(Math.PI * x) * Gamma(1 - x));

            x -= 1;
            double a = 0.99999999999980993;
            double t = x + 7.5;
            for (int i = 0; i < p.Length; i++) a += p[i] / (x + i + 1);

            return Math.Sqrt(2 * Math.PI) * Math.Pow(t, x + 0.5) * Math.Exp(-t) * a;
        }

        // Вспомогательная функция для логарифма Гамма-функции
        private static double LogGamma(double val)
        {
            double[] c = { 76.18009172947146, -86.50532032941677, 24.01409824083091,
                       -1.231739572450155, 0.1208650973866179e-2, -0.5395239384953e-5 };
            double temp = val + 5.5;
            temp -= (val + 0.5) * Math.Log(temp);
            double ser = 1.000000000190015;
            for (int i = 0; i < 6; i++) ser += c[i] / (++val);
            return -temp + Math.Log(2.5066282746310005 * ser / (val - (val - 1.0))); // упрощенный вид
        }

        public static bool HasLib(string name) => Libs.ContainsKey(name);

        public static object Call(string lib, string func, string[] args)
        {
            if (!Libs.TryGetValue(lib, out var libDict))
                throw new Exception($"Library '{lib}' is not loaded");
            if (!libDict.TryGetValue(func, out var fn))
                throw new Exception($"Library '{lib}' has no function '{func}'");
            return fn(args);
        }
    }

    // ── use / namespace ───────────────────────────────────────────────────────

    public class FabUse : FabBase
    {
        public string LibName { get; }
        public string? FilePath { get; }
        /// <summary>
        /// When non-null, only these function names are exported from the library.
        /// An empty set means "import nothing" (allowed but unusual).
        /// A null value means "import everything" (normal use).
        /// </summary>
        public HashSet<string>? OnlyFunctions { get; }

        public FabUse(string libName, string? filePath = null, HashSet<string>? onlyFunctions = null)
        {
            LibName = libName;
            FilePath = filePath;
            OnlyFunctions = onlyFunctions;
        }

        // ── C++-style header-less library files ─────────────────────────────
        // 'use mylib;' (a bare identifier, no quotes) is normally resolved
        // against the builtin FabLibRegistry (math, random, io, ...). If the
        // name isn't a known builtin, this looks for a matching source file
        // living in a 'libs/' folder — with OR without a '.fab' extension,
        // like a C++ header — so users can write plain-text library files
        // named e.g. 'libs/mylib' (no extension) and just do 'use mylib;'.
        // Checked both relative to the current working directory (where the
        // running .fab script lives) and relative to the interpreter/compiler
        // executable, mirroring the resolution order already used for quoted
        // 'use "file.fab";' imports.
        public static string? ResolveLibFile(string libName)
        {
            string[] candidates =
            {
                Path.Combine("libs", libName),
                Path.Combine("libs", libName + ".fab"),
                Path.Combine(AppContext.BaseDirectory, "libs", libName),
                Path.Combine(AppContext.BaseDirectory, "libs", libName + ".fab"),
            };
            foreach (var c in candidates)
                if (File.Exists(c)) return c;
            return null;
        }

        public override object Eval(FabInterpreter interpreter)
        {
            if (FilePath != null)
            {
                // ── Path resolution order ──────────────────────────────────────
                // 1. Absolute path — use as-is.
                // 2. Relative path — try relative to the current working directory
                //    (i.e. the directory where the .fab script lives).
                // 3. Relative path — try inside the 'libs/' folder that sits next
                //    to the interpreter executable (AppContext.BaseDirectory).
                //    This lets users write:  use "libs/math/vectors.fab";
                //    and the interpreter will find  <exe-dir>/libs/math/vectors.fab
                //    regardless of where the script is located.
                string resolvedPath = Path.GetFullPath(FilePath);
                if (!Path.IsPathRooted(FilePath) && !File.Exists(FilePath))
                {
                    string sysLibPath = Path.Combine(AppContext.BaseDirectory, FilePath);
                    if (File.Exists(sysLibPath))
                        resolvedPath = sysLibPath;
                    else
                        throw new Exception(
                            $"Library file not found: '{FilePath}'\n" +
                            $"  Searched: {Path.GetFullPath(FilePath)}\n" +
                            $"  Searched: {Path.GetFullPath(sysLibPath)}");
                }
                string code = File.ReadAllText(resolvedPath);
                var tokens = new Lexer().Lex(code).ToList();
                var stmts = new Parser(tokens).ParseStatements();
                var libInterp = new FabInterpreter();
                foreach (var stmt in stmts)
                    stmt.Eval(libInterp);

                // ── Propagate transitive library dependencies ───────────────────
                // If the imported file itself does 'use math;' / 'use "vectors.fab";'
                // / 'namespace mt = math;' etc., those libraries must also become
                // available in the main interpreter — otherwise calls made from
                // inside the imported file's own function bodies (which run under
                // the *main* interpreter instance once merged in) would fail to
                // resolve their own library dependencies.
                foreach (var lib in libInterp.UsedLibs)
                    interpreter.UsedLibs.Add(lib);

                foreach (var ns in libInterp.ActiveNamespaces)
                    interpreter.ActiveNamespaces.Add(ns);

                foreach (var kv in libInterp.LibAliases)
                    if (!interpreter.LibAliases.ContainsKey(kv.Key))
                        interpreter.LibAliases[kv.Key] = kv.Value;

                foreach (var kv in libInterp.LibOnlyFunctions)
                {
                    if (!interpreter.LibOnlyFunctions.TryGetValue(kv.Key, out var only))
                        interpreter.LibOnlyFunctions[kv.Key] = only = new HashSet<string>();
                    foreach (var fn in kv.Value) only.Add(fn);
                }

                // Nested file/user libraries the imported file pulled in itself
                // (e.g. it did 'use "helpers.fab";') — expose them too, unless
                // the main script already defined something under that name.
                foreach (var kv in libInterp.LibFunctions)
                    if (!interpreter.LibFunctions.ContainsKey(kv.Key))
                        interpreter.LibFunctions[kv.Key] = kv.Value;

                foreach (var kv in libInterp.ImportedFunctions)
                    if (!interpreter.ImportedFunctions.ContainsKey(kv.Key))
                        interpreter.ImportedFunctions[kv.Key] = kv.Value;

                // Always merge all classes from the library into the main interpreter.
                // Classes are always imported wholesale — there is no 'only' filter for them.
                foreach (var kv in libInterp.Classes)
                    interpreter.Classes[kv.Key] = kv.Value;

                // Merge the library's non-static ('extern') globals into the caller's globals.
                // 'static' globals stay private to the library file — its own functions
                // still see them via FabFuncDef.OwnerFileGlobals → libInterp.FileGlobals.
                foreach (var name in libInterp.Globals.AllNames())
                {
                    if (!interpreter.Globals.Contains(name))
                    {
                        var slot = libInterp.Globals.GetSlot(name);
                        interpreter.Globals.Define(name, slot.Value, slot.IsConst, slot.IsLocked, slot.IsFree);
                    }
                }

                if (OnlyFunctions != null)
                {
                    // Validate that all requested functions actually exist
                    foreach (var fn in OnlyFunctions)
                        if (!libInterp.Functions.ContainsKey(fn))
                            throw new Exception($"Library '{LibName}' has no function '{fn}' (requested in 'only' clause)");

                    var filtered = libInterp.Functions
                        .Where(kv => OnlyFunctions.Contains(kv.Key))
                        .ToDictionary(kv => kv.Key, kv => kv.Value);
                    interpreter.LibFunctions[LibName] = filtered;

                    // Import each function directly into the global scope
                    foreach (var fn in OnlyFunctions)
                        interpreter.ImportedFunctions[fn] = (LibName, fn);
                }
                else
                {
                    interpreter.LibFunctions[LibName] = libInterp.Functions;
                }
            }
            else
            {
                bool hasFuncs = FabLibRegistry.HasLib(LibName);
                bool hasTypes = FabLibRegistry.HasLibTypes(LibName);
                if (!hasFuncs && !hasTypes)
                {
                    // Not a builtin — look for 'libs/<LibName>' or
                    // 'libs/<LibName>.fab' and import it as a file library.
                    string? libFile = ResolveLibFile(LibName);
                    if (libFile != null)
                    {
                        new FabUse(LibName, filePath: libFile, onlyFunctions: OnlyFunctions).Eval(interpreter);
                        return null;
                    }
                    throw new Exception(
                        $"Unknown library '{LibName}'. Looked for a builtin library and for " +
                        $"'libs/{LibName}' or 'libs/{LibName}.fab' (relative to the script and to the executable).");
                }

                if (OnlyFunctions != null)
                {
                    if (!hasFuncs)
                        throw new Exception($"Library '{LibName}' exposes no functions to restrict with 'only'");

                    // Validate that all requested functions actually exist in the builtin lib
                    var libDict = FabLibRegistry.Libs[LibName];
                    foreach (var fn in OnlyFunctions)
                        if (!libDict.ContainsKey(fn))
                            throw new Exception($"Library '{LibName}' has no function '{fn}' (requested in 'only' clause)");

                    // Store the restricted view in a special per-lib "only" set
                    if (!interpreter.LibOnlyFunctions.ContainsKey(LibName))
                        interpreter.LibOnlyFunctions[LibName] = new HashSet<string>();
                    foreach (var fn in OnlyFunctions)
                    {
                        interpreter.LibOnlyFunctions[LibName].Add(fn);
                        // Import the function directly into the global scope
                        interpreter.ImportedFunctions[fn] = (LibName, fn);
                    }

                    interpreter.UsedLibs.Add(LibName);
                }
                else if (hasFuncs)
                {
                    interpreter.UsedLibs.Add(LibName);
                    // Unrestricted import clears any prior 'only' restriction on this lib
                    interpreter.LibOnlyFunctions.Remove(LibName);
                }

                // ── Library-exposed types: classes / structs ────────────────────
                // Always imported wholesale (mirrors file-import behaviour above);
                // an 'only' clause restricts functions only, never types.
                if (FabLibRegistry.LibClasses.TryGetValue(LibName, out var libClassMap))
                    foreach (var kv in libClassMap)
                        interpreter.Classes[kv.Key] = kv.Value;

                if (FabLibRegistry.LibStructs.TryGetValue(LibName, out var libStructMap))
                    foreach (var kv in libStructMap)
                        interpreter.Structs[kv.Key] = kv.Value;

                // A types-only library (no callable functions of its own) still
                // counts as "used" so 'use namespace <libName>;' and similar
                // UsedLibs-gated checks work as expected.
                if (hasTypes) interpreter.UsedLibs.Add(LibName);
            }
            return null;
        }
    }

    public class FabNamespace : FabBase
    {
        public string LibName { get; }
        public FabNamespace(string libName) { LibName = libName; }
        public override object Eval(FabInterpreter interpreter)
        {
            bool isBuiltin = FabLibRegistry.HasLib(LibName) && interpreter.UsedLibs.Contains(LibName);
            bool isUserLib = interpreter.LibFunctions.ContainsKey(LibName);
            if (!isBuiltin && !isUserLib)
                throw new Exception($"Cannot open namespace '{LibName}': library not imported. Add 'use {LibName};' before 'namespace {LibName};'");
            interpreter.ActiveNamespaces.Add(LibName);
            return null;
        }
    }

    // ── namespace Name { def foo() {...} class Bar {...} } ──────────────────────
    // Defines a user namespace. After definition, its members are callable as
    // Name.Foo() or without prefix after  using namespace Name;
    public class FabNamespaceDef : FabBase
    {
        public string Name { get; }
        public List<FabFuncDef> Functions { get; }
        public List<FabClasses> Classes { get; }
        public List<FabStructDefStmt> Structs { get; }

        public FabNamespaceDef(string name,
            List<FabFuncDef> functions,
            List<FabClasses> classes,
            List<FabStructDefStmt> structs)
        { Name = name; Functions = functions; Classes = classes; Structs = structs; }

        public override object Eval(FabInterpreter interpreter)
        {
            // Build the function map for this namespace
            var funcMap = new Dictionary<string, FabFuncDef>();
            foreach (var f in Functions)
                funcMap[f.Name] = f;

            // Register so Name.foo() works via FabMemberAccess / FabLibCall
            interpreter.LibFunctions[Name] = funcMap;

            // Classes and structs are global by name
            foreach (var cl in Classes) cl.Eval(interpreter);
            foreach (var st in Structs) st.Eval(interpreter);

            return null;
        }
    }

    // ── namespace alias = target; ─────────────────────────────────────────────
    // Creates a short alias for an already-imported library or user namespace.
    // After:  namespace mt = math;
    // You can call:  mt.pow(2, 10)  instead of  math.pow(2, 10)
    public class FabNamespaceAlias : FabBase
    {
        public string Alias { get; }
        public string Target { get; }
        public FabNamespaceAlias(string alias, string target) { Alias = alias; Target = target; }

        public override object Eval(FabInterpreter interpreter)
        {
            // Target must already be imported as a builtin lib OR a user lib
            bool isBuiltin = FabLibRegistry.HasLib(Target) && interpreter.UsedLibs.Contains(Target);
            bool isUserLib = interpreter.LibFunctions.ContainsKey(Target);

            if (!isBuiltin && !isUserLib)
                throw new Exception(
                    $"namespace alias '{Alias}' = '{Target}': " +
                    $"'{Target}' is not imported. Add 'use {Target};' first.");

            if (isUserLib)
            {
                // Copy the function map under the alias name
                interpreter.LibFunctions[Alias] = interpreter.LibFunctions[Target];
            }
            else
            {
                // Register the alias as a used lib pointing to the same builtin registry
                // We achieve this by adding the alias as a user lib that proxies to the builtin.
                // Simplest approach: copy the FabFuncDef wrappers for each builtin function.
                // Actually simpler: store a redirect entry so FabLibCall can resolve it.
                interpreter.LibAliases[Alias] = Target;
                interpreter.UsedLibs.Add(Alias);
            }
            return null;
        }
    }

    // ── using namespace math;  /  using namespace "file.fab"; ────────────────
    // Combines import + opening the namespace in one statement so functions are
    // callable without the library prefix (like C++ "using namespace std;").
    //
    // Differences from the old "use + namespace" two-liner:
    //   using namespace math;           → import builtin math, open it
    //   using namespace "mylib.fab";    → import file lib, open it
    //   using namespace math only pow;  → restricted import, open it
    //   using namespace MyNS;           → open a user namespace defined with namespace MyNS { }
    public class FabUsingNamespace : FabBase
    {
        public string LibName { get; }
        public string? FilePath { get; }
        public HashSet<string>? OnlyFunctions { get; }

        public FabUsingNamespace(string libName, string? filePath = null, HashSet<string>? onlyFunctions = null)
        { LibName = libName; FilePath = filePath; OnlyFunctions = onlyFunctions; }

        public override object Eval(FabInterpreter interpreter)
        {
            bool isUserNS = interpreter.LibFunctions.ContainsKey(LibName);
            bool isBuiltin = FabLibRegistry.HasLib(LibName) || FabLibRegistry.HasLibTypes(LibName);
            bool isFilePath = FilePath != null;
            bool isLibsFile = !isUserNS && !isBuiltin && !isFilePath && FabUse.ResolveLibFile(LibName) != null;

            if (!isUserNS && !isBuiltin && !isFilePath && !isLibsFile)
                throw new Exception(
                    $"'use namespace {LibName}': '{LibName}' is not a known library " +
                    $"or a user-defined namespace.\n" +
                    $"  Define it first:  namespace {LibName} {{ ... }}\n" +
                    $"  Or place it at 'libs/{LibName}' or 'libs/{LibName}.fab'.");

            // For builtin libs and file imports: run the import step
            if (!isUserNS)
                new FabUse(LibName, FilePath, OnlyFunctions).Eval(interpreter);

            // Open the namespace — members callable without prefix
            interpreter.ActiveNamespaces.Add(LibName);
            return null;
        }
    }

    // ── Library call ─────────────────────────────────────────────────────────

    public class FabLibCall : FabBase
    {
        public string LibName { get; }
        public string FuncName { get; }
        public List<FabBase> Args { get; }

        public FabLibCall(string libName, string funcName, List<FabBase> args)
        { LibName = libName; FuncName = funcName; Args = args; }

        public override object Eval(FabInterpreter interpreter)
        {
            // Resolve namespace alias: 'namespace mt = math;' → mt.pow → math.pow
            string resolvedLib = interpreter.LibAliases.TryGetValue(LibName, out var aliasTarget)
                ? aliasTarget : LibName;

            if (resolvedLib != LibName)
                return new FabLibCall(resolvedLib, FuncName, Args).Eval(interpreter);

            if (interpreter.LibFunctions.TryGetValue(LibName, out var userLib))
            {
                if (!userLib.ContainsKey(FuncName))
                    throw new Exception($"Library '{LibName}' has no function '{FuncName}'");
                // User-defined libs: the allowed set is already filtered at import time,
                // so any function present in userLib is callable.
                var call = new FabCall(FuncName, Args ?? new List<FabBase>());
                var savedFunctions = interpreter.Functions;
                interpreter.Functions = new Dictionary<string, FabFuncDef>(savedFunctions);
                foreach (var kv in userLib) interpreter.Functions[kv.Key] = kv.Value;
                object result;
                try { result = call.Eval(interpreter); }
                finally { interpreter.Functions = savedFunctions; }
                return result;
            }

            if (interpreter.UsedLibs.Contains(LibName))
            {
                // Enforce 'only' restriction for built-in libs
                if (!interpreter.IsLibFuncAllowed(LibName, FuncName))
                    throw new Exception($"Function '{FuncName}' was not exported from library '{LibName}' (not listed in 'only' clause)");

                if (Args == null || Args.Count == 0)
                    return FabLibRegistry.Call(LibName, FuncName, Array.Empty<string>());
                var evaledArgs = Args.Select(a => Convert.ToString(a.Eval(interpreter))).ToArray();
                return FabLibRegistry.Call(LibName, FuncName, evaledArgs);
            }

            throw new Exception($"Library '{LibName}' is not imported. Add 'use {LibName};'");
        }
    }

    // ── Program ───────────────────────────────────────────────────────────────

    public class FabProgram : FabBase
    {
        public List<FabBase> Statements { get; }
        public FabProgram(List<FabBase> statements) { Statements = statements; }

        public override object Eval(FabInterpreter interpreter)
        {
            foreach (var stmt in Statements)
                if (stmt is FabUse || stmt is FabUsingNamespace || stmt is FabNamespace || stmt is FabNamespaceDef || stmt is FabNamespaceAlias || stmt is FabFuncDef || stmt is FabClasses || stmt is FabStructDefStmt || stmt is FabGlobalDecl) stmt.Eval(interpreter);

            if (!interpreter.Functions.ContainsKey("main"))
                throw new Exception("Program must have a 'main' function");

            new FabCall("main", new List<FabBase>()).Eval(interpreter);
            return null;
        }
    }

    // ── Scope infrastructure ──────────────────────────────────────────────────

    /// <summary>
    /// A single variable slot: holds value plus its mutability metadata.
    /// IsConst   — value can never change (const keyword).
    /// IsLocked  — type is pinned (vfree := …); value can change within same type.
    /// IsFree    — inferred-type vfree (vfree = …); type can widen on each assign.
    /// </summary>
    public struct VarSlot
    {
        public object Value;
        public bool IsConst;
        public bool IsLocked;
        public bool IsFree;

        public VarSlot(object value, bool isConst = false, bool isLocked = false, bool isFree = false)
        { Value = value; IsConst = isConst; IsLocked = isLocked; IsFree = isFree; }
    }

    /// <summary>
    /// One lexical scope frame. Frames are linked by Parent, forming a chain
    /// that mirrors the block-nesting depth — exactly like C++ stack frames.
    /// </summary>
    public class ScopeFrame
    {
        public readonly Dictionary<string, VarSlot> Vars = new();
        public readonly ScopeFrame? Parent;
        public ScopeFrame(ScopeFrame? parent = null) { Parent = parent; }
    }

    /// <summary>
    /// The live scope stack for one function activation (or the global scope).
    /// Push/Pop track block entry/exit; Get/Set walk the parent chain.
    /// </summary>
    public class ScopeStack
    {
        private ScopeFrame _top;
        public ScopeStack() { _top = new ScopeFrame(); }

        /// <summary>
        /// Wraps an existing frame chain directly instead of starting fresh —
        /// used to snapshot a lexical scope for a closure. Push()/Pop() on the
        /// *original* ScopeStack this frame came from never affect this
        /// snapshot: Push() replaces _top with a brand-new child ScopeFrame
        /// rather than mutating the frame in place, so a reference to the old
        /// frame (and everything already defined in it) stays exactly as it
        /// was at capture time — real lexical closure semantics.
        /// </summary>
        public ScopeStack(ScopeFrame top, ScopeStack? globalFallback)
        {
            _top = top;
            GlobalFallback = globalFallback;
        }

        /// <summary>The live top frame — captured by lambda literals as their closure.</summary>
        public ScopeFrame CurrentFrame => _top;

        /// <summary>
        /// Consulted when a name isn't found in this stack's own frames.
        /// Used to chain: function-locals → file 'static' globals → module globals.
        /// </summary>
        public ScopeStack? GlobalFallback { get; set; }

        // ── Frame management ──

        /// <summary>Enter a new block scope (if/while/for body).</summary>
        public void Push() => _top = new ScopeFrame(_top);

        /// <summary>Exit a block scope, destroying all locals declared in it.</summary>
        public void Pop()
        {
            if (_top.Parent == null) throw new Exception("Scope underflow");
            _top = _top.Parent;
        }

        // ── Variable lookup ──

        private ScopeFrame? FindFrame(string name)
        {
            for (var f = _top; f != null; f = f.Parent)
                if (f.Vars.ContainsKey(name)) return f;
            return null;
        }

        public bool Contains(string name) =>
            FindFrame(name) != null || (GlobalFallback?.Contains(name) ?? false);

        public bool TryGet(string name, out object value)
        {
            var f = FindFrame(name);
            if (f != null) { value = f.Vars[name].Value; return true; }
            if (GlobalFallback != null) return GlobalFallback.TryGet(name, out value);
            value = null;
            return false;
        }

        public object Get(string name)
        {
            var f = FindFrame(name);
            if (f != null) return f.Vars[name].Value;
            if (GlobalFallback != null) return GlobalFallback.Get(name);
            throw new Exception($"Variable '{name}' is not defined");
        }

        public VarSlot GetSlot(string name)
        {
            var f = FindFrame(name);
            if (f != null) return f.Vars[name];
            if (GlobalFallback != null) return GlobalFallback.GetSlot(name);
            throw new Exception($"Variable '{name}' is not defined");
        }

        // ── Variable definition (always in innermost frame) ──

        public void Define(string name, object value,
            bool isConst = false, bool isLocked = false, bool isFree = false)
            => _top.Vars[name] = new VarSlot(value, isConst, isLocked, isFree);

        // ── Variable assignment (finds existing frame, updates in place) ──

        /// <summary>
        /// Assign a new value to an existing variable, walking up the scope
        /// chain to find it, then the global fallback chain. Throws if not found.
        /// </summary>
        public void Set(string name, object value)
        {
            var f = FindFrame(name);
            if (f != null) { var slot = f.Vars[name]; slot.Value = value; f.Vars[name] = slot; return; }
            if (GlobalFallback != null) { GlobalFallback.Set(name, value); return; }
            throw new Exception($"Variable '{name}' is not defined");
        }

        /// <summary>Update entire slot (used when metadata changes too).</summary>
        public void SetSlot(string name, VarSlot slot)
        {
            var f = FindFrame(name);
            if (f != null) { f.Vars[name] = slot; return; }
            if (GlobalFallback != null) { GlobalFallback.SetSlot(name, slot); return; }
            throw new Exception($"Variable '{name}' is not defined");
        }

        // ── Removal (delete statement) ──

        public bool Remove(string name)
        {
            var f = FindFrame(name);
            if (f != null) { f.Vars.Remove(name); return true; }
            return GlobalFallback?.Remove(name) ?? false;
        }

        // ── Enumeration (for "did you mean?" hints) ──

        /// <summary>Returns all variable names visible in any scope frame, deduplicated.</summary>
        public IEnumerable<string> AllNames()
        {
            var seen = new HashSet<string>();
            for (var f = _top; f != null; f = f.Parent)
                foreach (var k in f.Vars.Keys)
                    if (!k.StartsWith("__") && seen.Add(k))
                        yield return k;
            if (GlobalFallback != null)
                foreach (var n in GlobalFallback.AllNames())
                    if (seen.Add(n))
                        yield return n;
        }
    }

    // ── Color runtime value ───────────────────────────────────────────────────
    public class FabColor
    {
        public byte R { get; set; }
        public byte G { get; set; }
        public byte B { get; set; }
        public byte A { get; set; }

        public FabColor(byte r, byte g, byte b, byte a = 255)
        {
            R = r; G = g; B = b; A = a;
        }
        public override string ToString() => A == 255 ? $"Color({R}, {G}, {B})" : $"Color({R}, {G}, {B}, {A})";
    }

    // ── Interpreter ───────────────────────────────────────────────────────────

    public class FabInterpreter
    {
        /// <summary>
        /// The live lexical scope stack for the current function activation.
        /// Replaced wholesale on each function call (C++ semantics: no closure capture).
        /// Push/Pop track block entry/exit within a function.
        /// </summary>
        public ScopeStack Scope { get; set; } = new();

        /// <summary>Module-wide ("extern") globals — visible from every file.</summary>
        public ScopeStack Globals { get; } = new();

        /// <summary>This file's 'static' globals — only visible to functions defined in this file.</summary>
        public ScopeStack FileGlobals { get; } = new();

        public Dictionary<string, FabFuncDef> Functions { get; set; } = new();
        public Dictionary<string, FabClasses> Classes { get; set; } = new();
        public Dictionary<string, FabStructDef> Structs { get; set; } = new();
        public HashSet<string> UsedLibs { get; } = new();
        public HashSet<string> ActiveNamespaces { get; } = new();
        public Dictionary<string, Dictionary<string, FabFuncDef>> LibFunctions { get; } = new();
        public Dictionary<string, HashSet<string>> LibOnlyFunctions { get; } = new();
        /// <summary>Maps alias name → canonical builtin lib name (from 'namespace alias = lib;').</summary>
        public Dictionary<string, string> LibAliases { get; } = new();
        public Dictionary<string, (string LibName, string FuncName)> ImportedFunctions { get; } = new();

        /// <summary>
        /// Implementations for bodyless 'native def name(...);' stubs. Not
        /// populated from Fab source — this is the extension point for a host
        /// program embedding FabInterpreter to supply real native behavior
        /// (e.g. filesystem/network/FFI hooks) under a given function name.
        /// Calling a native function whose Body is empty and whose name isn't
        /// registered here throws a clear "no implementation" error.
        /// </summary>
        public Dictionary<string, Func<object[], object>> NativeFunctions { get; } = new();

        public void RegisterNative(string name, Func<object[], object> impl) => NativeFunctions[name] = impl;

        // Static field storage: ["ClassName.FieldName"] = value
        private readonly Dictionary<string, object> _staticFields = new();

        public object GetStaticField(string className, string fieldName)
        {
            _staticFields.TryGetValue($"{className}.{fieldName}", out var val);
            return val;
        }

        public void SetStaticField(string className, string fieldName, object value)
            => _staticFields[$"{className}.{fieldName}"] = value;

        // Convenience helpers that delegate to VarSlot metadata
        public bool IsConst(string name) => Scope.Contains(name) && Scope.GetSlot(name).IsConst;
        public bool IsLocked(string name) => Scope.Contains(name) && Scope.GetSlot(name).IsLocked;
        public bool IsFree(string name) => Scope.Contains(name) && Scope.GetSlot(name).IsFree;

        public bool IsLibFuncAllowed(string libName, string funcName)
        {
            if (!LibOnlyFunctions.TryGetValue(libName, out var allowed)) return true;
            return allowed.Contains(funcName);
        }

        public object Eval(FabBase node) => node.Eval(this);

        /// <summary>
        /// Builds the fresh scope used to run a function/method/constructor body.
        /// Falls back to the 'static' globals of the file that originally defined
        /// the function, then to module-wide globals — never to the caller's locals.
        /// </summary>
        public ScopeStack NewCallScope(FabFuncDef func)
            => new ScopeStack { GlobalFallback = func.OwnerFileGlobals ?? FileGlobals };

        public FabInterpreter()
        {
            FileGlobals.GlobalFallback = Globals;
            RegisterBuiltinClasses();
        }
        private void RegisterBuiltinClasses()
        {
            RegisterConvertClass();
            RegisterColorClass();
            RegisterRegexClass();
        }

        private void RegisterConvertClass()
        {
            FabFuncDef MakeMethod(string name, List<(string, string)> parms, Func<object[], object> impl)
            {
                return new FabBuiltinMethod(name, parms, impl);
            }

            var methods = new Dictionary<string, FabFuncDef>
            {
                ["to_int"] = MakeMethod("to_int",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (int.TryParse(s, out int r)) return (object)(int)r;
                    throw new Exception($"Convert.to_int({s}): cannot convert to int");
                }),

                ["to_short"] = MakeMethod("to_short",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (short.TryParse(s, out short r)) return (object)(short)r;
                    throw new Exception($"Convert.to_short({s}): cannot convert to float");
                }),

                ["to_long"] = MakeMethod("to_long",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (long.TryParse(s, out long r)) return (object)(int)r;
                    throw new Exception($"Convert.to_long({s}): cannot convert to long");
                }),

                ["to_byte"] = MakeMethod("to_byte",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (byte.TryParse(s, out byte r)) return (object)(byte)r;
                    throw new Exception($"Convert.to_byte({s}): cannot convert to int");
                }),

                ["to_uint"] = MakeMethod("to_uint",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (uint.TryParse(s, out uint r)) return (object)(uint)r;
                    throw new Exception($"Convert.to_uint({s}): cannot convert to uint");
                }),

                ["to_ushort"] = MakeMethod("to_ushort",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (ushort.TryParse(s, out ushort r)) return (object)(ushort)r;
                    throw new Exception($"Convert.to_ushort({s}): cannot convert to ushort");
                }),

                ["to_ulong"] = MakeMethod("to_ulong",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (ulong.TryParse(s, out ulong r)) return (object)(ulong)r;
                    throw new Exception($"Convert.to_ulong({s}): cannot convert to ulong");
                }),

                ["to_ubyte"] = MakeMethod("to_ubyte",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (sbyte.TryParse(s, out sbyte r)) return (object)(sbyte)r;
                    throw new Exception($"Convert.to_ubyte({s}): cannot convert to sbyte");
                }),

                ["to_float"] = MakeMethod("to_float",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (float.TryParse(s, out float r)) return r;
                    throw new Exception($"Convert.to_float({s}): cannot convert to float");
                }),

                ["to_double"] = MakeMethod("to_double",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString() ?? "";
                    if (double.TryParse(s, out double r)) return (object)(double)r;
                    throw new Exception($"Convert.to_double({s}): cannot convert to double");
                }),

                ["to_bool"] = MakeMethod("to_bool",
                new List<(string, string)> { ("string", "value") },
                args =>
                {
                    string s = args[0]?.ToString()?.ToLower() ?? "";
                    if (s == "true") return (object)true;
                    if (s == "false") return (object)false;
                    throw new Exception($"Convert.to_bool: cannot convert '{s}' to bool");
                }),

                ["to_str"] = MakeMethod("to_str",
                new List<(string, string)> { ("vfree", "value") },
                args => FormatValue(args[0])),

                ["to_bin"] = MakeMethod("to_bin",
                new List<(string, string)> { ("vfree", "value") },
                args =>
                {
                    long n = Convert.ToInt64(Convert.ToDouble(args[0]));
                    return (object)Convert.ToString(n, 2);
                }),

                ["to_hex"] = MakeMethod("to_hex",
                new List<(string, string)> { ("vfree", "value") },
                args =>
                {
                    long n = Convert.ToInt64(Convert.ToDouble(args[0]));
                    return (object)Convert.ToString(n, 16).ToUpper();
                }),

                ["to_oct"] = MakeMethod("to_oct",
                new List<(string, string)> { ("vfree", "value") },
                args =>
                {
                    long n = Convert.ToInt64(Convert.ToDouble(args[0]));
                    return (object)Convert.ToString(n, 8);
                }),

                ["from_bin"] = MakeMethod("from_bin",
                new List<(string, string)> { ("string", "value") },
                args => (object)(double)Convert.ToInt64(args[0]?.ToString(), 2)),

                ["from_hex"] = MakeMethod("from_hex",
                new List<(string, string)> { ("string", "value") },
                args => (object)(double)Convert.ToInt64(args[0]?.ToString(), 16)),

                ["from_oct"] = MakeMethod("from_oct",
                new List<(string, string)> { ("string", "value") },
                args => (object)(double)Convert.ToInt64(args[0]?.ToString(), 8)),

                ["to_char"] = MakeMethod("to_char",
                new List<(string, string)> { ("vfree", "value") },
                args =>
                {
                    var v = args[0];
                    if (v is char c) return (object)c;
                    int code = Convert.ToInt32(Convert.ToDouble(v));
                    return (object)(char)code;
                }),

                ["to_ascii"] = MakeMethod("to_ascii",
                new List<(string, string)> { ("char", "value") },
                args =>
                {
                    if (args[0] is char c) return (object)(double)c;
                    if (args[0] is string s && s.Length > 0) return (object)(double)s[0];
                    throw new Exception("Convert.to_ascii: expected char");
                }),
            };

            var classdef = new FabBuiltinClass("Convert", methods, true);
            Classes["Convert"] = classdef;
        }
        private void RegisterColorClass()
        {
            FabFuncDef MakeMethod(string name, List<(string, string)> parms, Func<object[], object> impl)
                => new FabBuiltinMethod(name, parms, impl);

            // ── Конструктор: Color(r, g, b) / Color(r, g, b, a) ──────────────────
            // В Fab# конструктор — это метод с именем класса.
            // Мы регистрируем через FabBuiltinConstructorClass (см. ниже).

            // ── Статические цвета-константы ──────────────────────────────────────
            var fields = new List<FabFieldDef>
            {
                // FabFieldDef хранит FabBase? Default — передаём FabLiteral<FabColor>
                new FabFieldDef("public", "color", "red",     new FabLiteral(new FabColor(255, 0,   0))),
                new FabFieldDef("public", "color", "green",   new FabLiteral(new FabColor(0,   255, 0))),
                new FabFieldDef("public", "color", "blue",    new FabLiteral(new FabColor(0,   0, 255))),
                new FabFieldDef("public", "color", "white",   new FabLiteral(new FabColor(255, 255, 255))),
                new FabFieldDef("public", "color", "black",   new FabLiteral(new FabColor(0,   0,   0))),
                new FabFieldDef("public", "color", "yellow",  new FabLiteral(new FabColor(255, 255, 0))),
                new FabFieldDef("public", "color", "cyan",    new FabLiteral(new FabColor(0,   255, 255))),
                new FabFieldDef("public", "color", "magenta", new FabLiteral(new FabColor(255, 0,   255))),
                new FabFieldDef("public", "color", "orange",  new FabLiteral(new FabColor(255, 165, 0))),
                new FabFieldDef("public", "color", "purple",  new FabLiteral(new FabColor(128, 0,   128))),
                new FabFieldDef("public", "color", "gray",    new FabLiteral(new FabColor(128, 128, 128))),
                new FabFieldDef("public", "color", "transparent", new FabLiteral(new FabColor(0, 0, 0, 0))),
            };

            var methods = new Dictionary<string, FabFuncDef>
            {
                // ── Разбор из строки "#RRGGBB" или "#RRGGBBAA" ──────────────────
                ["from_hex"] = MakeMethod("from_hex",
                    new List<(string, string)> { ("string", "hex") },
                    args =>
                    {
                        string h = args[0]?.ToString()?.TrimStart('#') ?? "";
                        if (h.Length == 6)
                            return (object)new FabColor(
                                Convert.ToByte(h.Substring(0, 2), 16),
                                Convert.ToByte(h.Substring(2, 2), 16),
                                Convert.ToByte(h.Substring(4, 2), 16));
                        if (h.Length == 8)
                            return (object)new FabColor(
                                Convert.ToByte(h.Substring(0, 2), 16),
                                Convert.ToByte(h.Substring(2, 2), 16),
                                Convert.ToByte(h.Substring(4, 2), 16),
                                Convert.ToByte(h.Substring(6, 2), 16));
                        throw new Exception($"Color.from_hex: invalid hex '{args[0]}'");
                    }),

                // ── Экспорт в hex-строку ─────────────────────────────────────────
                ["to_hex"] = MakeMethod("to_hex",
                    new List<(string, string)> { ("color", "col") },
                    args =>
                    {
                        var c = args[0] as FabColor
                            ?? throw new Exception("Color.to_hex: argument must be a Color");
                        return (object)(c.A == 255
                            ? $"#{c.R:X2}{c.G:X2}{c.B:X2}"
                            : $"#{c.R:X2}{c.G:X2}{c.B:X2}{c.A:X2}");
                    }),

                // ── Смешение двух цветов ─────────────────────────────────────────
                ["lerp"] = MakeMethod("lerp",
                    new List<(string, string)> { ("color", "a"), ("color", "b"), ("float", "t") },
                    args =>
                    {
                        var a = args[0] as FabColor ?? throw new Exception("Color.lerp: a must be Color");
                        var b = args[1] as FabColor ?? throw new Exception("Color.lerp: b must be Color");
                        float t = Convert.ToSingle(args[2]);
                        t = Math.Clamp(t, 0f, 1f);
                        return (object)new FabColor(
                            (byte)(a.R + (b.R - a.R) * t),
                            (byte)(a.G + (b.G - a.G) * t),
                            (byte)(a.B + (b.B - a.B) * t),
                            (byte)(a.A + (b.A - a.A) * t));
                    }),

                // ── Инвертировать цвет ───────────────────────────────────────────
                ["invert"] = MakeMethod("invert",
                    new List<(string, string)> { ("color", "col") },
                    args =>
                    {
                        var c = args[0] as FabColor ?? throw new Exception("Color.invert: argument must be Color");
                        return (object)new FabColor(
                            (byte)(255 - c.R), (byte)(255 - c.G), (byte)(255 - c.B), c.A);
                    }),

                // ── Установить прозрачность ──────────────────────────────────────
                ["with_alpha"] = MakeMethod("with_alpha",
                    new List<(string, string)> { ("color", "col"), ("byte", "alpha") },
                    args =>
                    {
                        var c = args[0] as FabColor ?? throw new Exception("Color.with_alpha: first arg must be Color");
                        byte a = (byte)Convert.ToInt32(Convert.ToDouble(args[1]));
                        return (object)new FabColor(c.R, c.G, c.B, a);
                    }),

                // ── Читать компоненты как числа ──────────────────────────────────
                ["get_r"] = MakeMethod("get_r",
                    new List<(string, string)> { ("color", "col") },
                    args => (object)(double)((FabColor)args[0]).R),

                ["get_g"] = MakeMethod("get_g",
                    new List<(string, string)> { ("color", "col") },
                    args => (object)(double)((FabColor)args[0]).G),

                ["get_b"] = MakeMethod("get_b",
                    new List<(string, string)> { ("color", "col") },
                    args => (object)(double)((FabColor)args[0]).B),

                ["get_a"] = MakeMethod("get_a",
                    new List<(string, string)> { ("color", "col") },
                    args => (object)(double)((FabColor)args[0]).A),

                // ── to_str ───────────────────────────────────────────────────────
                ["to_str"] = MakeMethod("to_str",
                    new List<(string, string)> { ("color", "col") },
                    args => (object)((FabColor)args[0]).ToString()),
            };

            var classdef = new FabBuiltinClassWithCtor("Color", fields, methods,
                ctorParams: new List<(string, string)> { ("double", "r"), ("double", "g"), ("double", "b") },
                ctorImpl: args =>
                {
                    byte r = (byte)Math.Clamp(Convert.ToDouble(args[0]), 0, 255);
                    byte g = (byte)Math.Clamp(Convert.ToDouble(args[1]), 0, 255);
                    byte b = (byte)Math.Clamp(Convert.ToDouble(args[2]), 0, 255);
                    // 4-й аргумент — необязательный alpha (через перегрузку не поддерживается,
                    // поэтому регистрируем отдельно Color4 или просто принимаем 3 аргумента)
                    return new FabColor(r, g, b);
                }, true);

            Classes["Color"] = classdef;
        }
        private void RegisterRegexClass()
        {
            FabFuncDef MakeMethod(string name, List<(string, string)> parms, Func<object[], object> impl)
                => new FabBuiltinMethod(name, parms, impl);

            var methods = new Dictionary<string, FabFuncDef>
            {
                // regex.is_match(input, pattern, flags) → bool
                ["is_match"] = MakeMethod("is_match",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string flags = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        return (object)System.Text.RegularExpressions.Regex.IsMatch(input, pattern, opts);
                    }),

                // regex.match(input, pattern, flags) → string | null
                ["match"] = MakeMethod("match",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string flags = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        var m = System.Text.RegularExpressions.Regex.Match(input, pattern, opts);
                        return m.Success ? (object)m.Value : null;
                    }),

                // regex.find_all(input, pattern, flags) → FabList of strings
                ["find_all"] = MakeMethod("find_all",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string flags = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        var list = new FabList("string");
                        foreach (System.Text.RegularExpressions.Match m in
                            System.Text.RegularExpressions.Regex.Matches(input, pattern, opts))
                            list.Add(m.Value);
                        return (object)list;
                    }),

                // regex.replace(input, pattern, replacement, flags) → string
                ["replace"] = MakeMethod("replace",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "replacement"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string replacement = args[2]?.ToString() ?? "";
                        string flags = args.Length > 3 ? args[3]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        return (object)System.Text.RegularExpressions.Regex.Replace(input, pattern, replacement, opts);
                    }),

                // regex.split(input, pattern, flags) → FabList of strings
                ["split"] = MakeMethod("split",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string flags = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        var list = new FabList("string");
                        foreach (string part in System.Text.RegularExpressions.Regex.Split(input, pattern, opts))
                            list.Add(part);
                        return (object)list;
                    }),

                // regex.groups(input, pattern, flags) → FabList of strings (group 0 + capture groups)
                ["groups"] = MakeMethod("groups",
                    new List<(string, string)> { ("string", "input"), ("string", "pattern"), ("string", "flags") },
                    args =>
                    {
                        string input = args[0]?.ToString() ?? "";
                        string pattern = args[1]?.ToString() ?? "";
                        string flags = args.Length > 2 ? args[2]?.ToString() ?? "" : "";
                        var opts = System.Text.RegularExpressions.RegexOptions.None;
                        if (flags.Contains('i')) opts |= System.Text.RegularExpressions.RegexOptions.IgnoreCase;
                        if (flags.Contains('m')) opts |= System.Text.RegularExpressions.RegexOptions.Multiline;
                        if (flags.Contains('s')) opts |= System.Text.RegularExpressions.RegexOptions.Singleline;
                        var list = new FabList("string");
                        var m = System.Text.RegularExpressions.Regex.Match(input, pattern, opts);
                        if (m.Success)
                            foreach (System.Text.RegularExpressions.Group g in m.Groups)
                                list.Add(g.Value);
                        return (object)list;
                    }),

                // regex.escape(pattern) → string  (escapes all special regex chars)
                ["escape"] = MakeMethod("escape",
                    new List<(string, string)> { ("string", "pattern") },
                    args =>
                    {
                        string pattern = args[0]?.ToString() ?? "";
                        return (object)System.Text.RegularExpressions.Regex.Escape(pattern);
                    }),
            };

            var classdef = new FabBuiltinClass("Regex", methods, true);
            Classes["Regex"] = classdef;
        }
    }

    public class FabBuiltinMethod : FabFuncDef
    {
        private readonly Func<object[], object> _impl;

        public FabBuiltinMethod(string name, List<(string, string)> parms, Func<object[], object> impl) : base(name, parms, null, new List<FabBase>())
        {
            _impl = impl;
        }

        public object Invoke(object[] args) => _impl(args);
    }

    public class FabBuiltinClass : FabClasses
    {
        public FabBuiltinClass(string name, Dictionary<string, FabFuncDef> methods, bool _static)
            : base(name, null,
                   new List<FabFieldDef>(),
                   methods, null, _static)
        {

        }

        public override object Eval(FabInterpreter interpreter)
        {
            interpreter.Classes[Name] = this;
            return null;
        }
    }

    public class FabBuiltinClassWithCtor : FabBuiltinClass
    {
        private readonly List<(string Type, string Name)> _ctorParams;
        private readonly Func<object[], object> _ctorImpl;

        public FabBuiltinClassWithCtor(
            string name,
            List<FabFieldDef> fields,
            Dictionary<string, FabFuncDef> methods,
            List<(string, string)> ctorParams,
            Func<object[], object> ctorImpl,
            bool _static
        ) : base(name, methods, _static)
        {
            _ctorParams = ctorParams;
            _ctorImpl = ctorImpl;
            OwnFields.AddRange(fields);
            Fields = OwnFields;
            Constructor = new FabBuiltinMethod(name, ctorParams, ctorImpl);
        }
    }
}