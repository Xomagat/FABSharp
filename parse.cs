using static AST;

public class Parser
{
    private readonly List<Token> _tokens;
    private int _pos;

    public Parser(List<Token> tokens) { _tokens = tokens.ToList(); }

    private Token? Peek() => _pos < _tokens.Count ? _tokens[_pos] : null;
    private void Advance() => _pos++;
    private int CurrentLine() => Peek()?.Line ?? (_tokens.Count > 0 ? _tokens[^1].Line : 0);

    private Token Expect(string type) =>
        Peek() is { Name: var t } && t == type
            ? _tokens[_pos++]
            : throw new FabException(CurrentLine(), $"Expected {type}, got {Peek()?.Name ?? "EOF"}");

    // Expect a specific single-character operator token by its value (e.g. '<', '>').
    private void ExpectOp(string op)
    {
        if (Peek() is { Name: "OP", Value: var v } && v?.ToString() == op) { Advance(); return; }
        throw new FabException(CurrentLine(), $"Expected '{op}', got {Peek()?.Value ?? "EOF"}");
    }

    // Expect a closing '>' for generics (list<T>, dict<K,V>). The lexer greedily
    // tokenises '>>' as a single ARROW token, so nested generics like
    // 'list<list<int>>' need that token split back into two '>' tokens here.
    private void ExpectGT()
    {
        if (Peek() is { Name: "ARROW", Value: ">>" })
        {
            var tok = _tokens[_pos];
            _tokens[_pos] = new Token("OP", ">", tok.Line);
            _tokens.Insert(_pos + 1, new Token("OP", ">", tok.Line));
        }
        ExpectOp(">");
    }

    public AST.FabProgram Parse()
    {
        var statements = new List<FabBase>();
        while (Peek() is not null)
            statements.Add(TopLevelStatement());
        return new FabProgram(statements);
    }

    public FabBase ParseExpr() => Expr();

    public List<FabBase> ParseStatements()
    {
        var statements = new List<FabBase>();
        while (Peek() is not null)
            statements.Add(TopLevelStatement());
        return statements;
    }

    // ── Top-level only: handles 'static' global variable declarations ─────────
    // 'static class Foo { ... }' is left untouched — that still goes through
    // Statement() → StaticClassStatement() exactly as before.
    private FabBase TopLevelStatement()
    {
        bool isStatic = false;
        if (Peek()?.Name == "STATIC"
            && _pos + 1 < _tokens.Count
            && _tokens[_pos + 1].Name != "CLASS")
        {
            Advance();
            isStatic = true;
        }

        var stmt = Statement();

        bool isVarDecl = stmt is FabAssign or FabVarDecl or FabStatementList
                              or FabConst or FabListDecl or FabDictDecl
                              or FabVFreeDecl or FabConstVFree;

        if (isStatic && !isVarDecl)
            throw new FabException(CurrentLine(), "'static' is only allowed on global variable declarations");

        return isVarDecl ? new FabGlobalDecl(stmt, isStatic) : stmt;
    }

    public FabBase Statement()
    {
        // Tuple destructure: type1 a, type2 b = expr;  (type keyword then ID then comma)
        if (IsDestructureAhead())
            return TupleDestructureStatement();

        if (Peek() is { Name: "OP", Value: "*" })
        {
            Advance();
            var ptrName = Expect("ID").Value.ToString();
            Expect("OP");
            var rhs = Expr();
            Expect("SEMI");
            return new FabDerefAssign(ptrName, rhs);
        }

        return Peek()?.Name switch
        {
            "CLASS" => ClassStatement(isStatic: false),
            "STATIC" => StaticClassStatement(),
            "STRUCT" => StructStatement(),
            "ID" => AssignmentOrCompound(),
            "WRITE" => WriteStatement(),
            "WRITELN" => WritelnStatement(),
            "INPUT_IN" => InputInStatement(),
            "IF" => IfStatement(),
            "WHILE" => WhileStatement(),
            "FOR" => ForStatement(),
            "DELETE" => DeleteStatement(),
            "VFREE" => VFreeStatement(),
            "CONST" => ConstDeclaration(),
            "RETURN" => ReturnStatement(),
            "THROW" => ThrowStatement(),
            "TRY" => TryStatement(),
            "DEF" => FuncDefStatement(),
            "NATIVE" => NativeFuncDefStatement(),
            "USE" => UseStatement(),
            "NAMESPACE" => NamespaceDefStatement(),
            "BREAK" => BreakStatement(),
            "CONTINUE" => ContinueStatement(),
            "SWITCH" => SwitchStatement(),
            // 'limited type[] name;' or 'limited type[N] name;'
            "LIMITED" => LimitedDeclaration(),
            "LIST" => ListGenericDeclaration(),
            "DICT" => DictGenericDeclaration(),
            // '(keyType, valType){} name;'  — unlimited dict (bare syntax)
            // Use 'limited (keyType, valType){N} name;' for capacity-limited dicts
            "LPAREN" => DictDeclaration(),
            "INT" => VarOrListDeclaration(),
            "SHORT" => VarOrListDeclaration(),
            "LONG" => VarOrListDeclaration(),
            "FLOAT" => VarOrListDeclaration(),
            "DOUBLE" => VarOrListDeclaration(),
            "STRING" => VarOrListDeclaration(),
            "BOOL" => VarOrListDeclaration(),
            "CHAR" => VarOrListDeclaration(),
            "BYTE" => VarOrListDeclaration(),
            "UN" => UnVarDeclaration(),
            "TUPLE" => TupleVarDeclaration(),
            _ => Expr(),
        };
    }

    // ── Tuple detection helper ────────────────────────────────────────────────
    // Returns true when current position looks like: type name , type name = 
    // i.e. this is a multi-variable destructure, not a plain var declaration.
    private bool IsDestructureAhead()
    {
        // Pattern: TYPE ID COMMA …
        // Peek() is the type keyword. _pos+1 should be ID, _pos+2 should be COMMA.
        if (_pos + 2 >= _tokens.Count
            || _tokens[_pos + 1].Name != "ID"
            || _tokens[_pos + 2].Name != "COMMA")
            return false;

        // A tuple destructure always unpacks via a single trailing '= expr;'
        // whose expression has no top-level (depth-0) comma. If a top-level
        // comma appears AFTER the first top-level '=' (e.g. 'int x, y = 5, z;'),
        // or no '=' ever appears before the statement's ';' (e.g. 'int a, b;'),
        // this is a plain multi-declaration list instead — not a destructure —
        // so VarOrListDeclaration() should handle it.
        int depth = 0;
        int eqPos = -1;
        for (int i = _pos; i < _tokens.Count; i++)
        {
            var t = _tokens[i];
            if (t.Name is "LPAREN" or "LBRACKET" or "LBRACE") depth++;
            else if (t.Name is "RPAREN" or "RBRACKET" or "RBRACE") depth--;
            else if (depth == 0 && t.Name == "SEMI")
                return eqPos >= 0;
            else if (depth == 0 && t.Name == "OP" && t.Value?.ToString() == "=")
            {
                if (eqPos < 0) eqPos = i;
            }
            else if (depth == 0 && t.Name == "COMMA" && eqPos >= 0)
                return false;
        }
        return false;
    }

    // ── tuple varName = expr; ─────────────────────────────────────────────────
    private FabBase TupleVarDeclaration()
    {
        Advance(); // consume 'tuple'
        var name = Expect("ID").Value.ToString();
        if (Peek() is { Name: "SEMI" }) { Advance(); return new FabVarDecl(name, "tuple"); }
        if (Peek() is not { Name: "OP", Value: "=" })
            throw new FabException(CurrentLine(), $"Expected '=' or ';' after '{name}'");
        Advance();
        var expr = Expr();
        Expect("SEMI");
        return new FabAssign(name, expr, "tuple");
    }

    // ── int a, string b [, ...] = expr; ──────────────────────────────────────
    private FabBase TupleDestructureStatement()
    {
        var targets = new List<(string?, string)>();

        // Parse: type name [, type name]*  =  expr  ;
        while (true)
        {
            string? declType = null;
            // Type keyword or ID (user class) or bare name (existing var)
            if (Peek()?.Name is "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE"
                              or "STRING" or "BOOL" or "CHAR" or "BYTE" or "TUPLE")
            {
                declType = _tokens[_pos].Value.ToString().ToLower();
                Advance();
            }
            else if (Peek()?.Name == "UN")
            {
                Advance();
                declType = Peek()?.Name switch
                {
                    "INT" => "uint",
                    "SHORT" => "ushort",
                    "LONG" => "ulong",
                    "BYTE" => "sbyte",
                    _ => throw new FabException(CurrentLine(), $"'un' cannot be used with type '{Peek()?.Value}'")
                };
                Advance();
            }
            else if (Peek()?.Name == "ID")
            {
                // Could be existing var (no type) or class-typed var
                // If next after ID is ID → it's "ClassName varName"
                if (_pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "ID")
                {
                    declType = _tokens[_pos].Value.ToString();
                    Advance();
                }
                // else: plain existing variable name, declType stays null
            }

            var varName = Expect("ID").Value.ToString();
            targets.Add((declType, varName));

            if (Peek()?.Name == "COMMA") { Advance(); continue; }
            break;
        }

        if (Peek() is not { Name: "OP", Value: "=" })
            throw new FabException(CurrentLine(), "Expected '=' in tuple destructure");
        Advance();
        var expr = Expr();
        Expect("SEMI");
        return new FabTupleDestructure(targets, expr);
    }

    // ── Dictionary declaration ────────────────────────────────────────────────
    // Syntax: (keyType, valType){} name;
    //         (keyType, valType){N} name;
    //         (keyType, valType){} name = {...};
    //         (keyType, valType){N} name = {...};
    private FabBase DictDeclaration()
    {
        // Save position so we can backtrack if this is really a parenthesised expression
        int savedPos = _pos;
        try { return TryParseDictDeclaration(); }
        catch (Exception ex)
        {
            _pos = savedPos;
            return Expr();   // fall back to expression
        }
    }

    private FabBase TryParseDictDeclaration()
    {
        // ParseTypeString now handles '(keyType, valType)' recursively.
        // It consumes the leading '(' and the closing ')'.
        string dictType = ParseTypeString();

        // dictType must be of the form "(keyType, valType)"
        if (!dictType.StartsWith("(") || !dictType.EndsWith(")"))
            throw new FabException(CurrentLine(), $"Expected dict type '(keyType, valType)', got '{dictType}'");

        (string keyType, string valType) = SplitDictTypeString(dictType);

        // Now expect '{' — either '{}' (unlimited) or '{N}' (limited)
        Expect("LBRACE");
        int capacity = -1;
        if (Peek()?.Name == "NUMBER")
            capacity = Convert.ToInt32(_tokens[_pos++].Value);
        Expect("RBRACE");

        var name = Expect("ID").Value.ToString();
        FabBase? init = null;
        if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
        Expect("SEMI");
        return new FabDictDecl(name, keyType, valType, capacity, init);
    }

    /// <summary>
    /// Splits "(keyType, valType)" at the top-level comma, returning both parts.
    /// Handles nested parens correctly, e.g. "(int, (string, bool[]))".
    /// </summary>
    private static (string Key, string Val) SplitDictTypeString(string s)
    {
        string inner = s.Substring(1, s.Length - 2); // strip outer ( )
        int depth = 0;
        for (int i = 0; i < inner.Length; i++)
        {
            if (inner[i] == '(') depth++;
            else if (inner[i] == ')') depth--;
            else if (inner[i] == ',' && depth == 0)
                return (inner.Substring(0, i).Trim(), inner.Substring(i + 1).Trim());
        }
        throw new Exception($"Malformed dict type string: '{s}'");
    }

    // ── list<elemType> name [= [...]]; ────────────────────────────────────────
    private FabBase ListGenericDeclaration()
    {
        string typeName = ParseTypeString(); // consumes 'list<elemType>', returns "elemType[]"
        string elemType = typeName.EndsWith("[]") ? typeName[..^2] : typeName;
        var name = Expect("ID").Value.ToString();
        FabBase? init = null;
        if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
        Expect("SEMI");
        return new FabListDecl(name, elemType, capacity: -1, initializer: init);
    }

    // ── dict<keyType, valType> name [= {...}]; ─────────────────────────────────
    private FabBase DictGenericDeclaration()
    {
        string typeName = ParseTypeString(); // consumes 'dict<keyType, valType>', returns "(keyType, valType)"
        (string keyType, string valType) = SplitDictTypeString(typeName);
        var name = Expect("ID").Value.ToString();
        FabBase? init = null;
        if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
        Expect("SEMI");
        return new FabDictDecl(name, keyType, valType, capacity: -1, initializer: init);
    }

    // ── Limited declaration ───────────────────────────────────────────────────
    // List syntax:  limited type[N] name;
    //               limited type[N] name = [...];
    // Dict syntax:  limited (keyType, valType){N} name;
    //               limited (keyType, valType){N} name = {...};
    private FabBase LimitedDeclaration()
    {
        Advance(); // consume 'limited'

        // If next token is '(' this is a limited dict declaration
        if (Peek()?.Name == "LPAREN")
        {
            string dictType = ParseTypeString(); // consumes (keyType, valType)
            if (!dictType.StartsWith("(") || !dictType.EndsWith(")"))
                throw new FabException(CurrentLine(), $"Expected dict type '(keyType, valType)', got '{dictType}'");
            (string keyType, string valType) = SplitDictTypeString(dictType);

            Expect("LBRACE");
            if (Peek()?.Name != "NUMBER")
                throw new FabException(CurrentLine(), "limited dict requires a capacity: 'limited (keyType, valType){N} name'");
            int dictCap = Convert.ToInt32(_tokens[_pos++].Value);
            Expect("RBRACE");

            var dictName = Expect("ID").Value.ToString();
            FabBase? dictInit = null;
            if (Peek() is { Name: "OP", Value: "=" }) { Advance(); dictInit = Expr(); }
            Expect("SEMI");
            return new FabDictDecl(dictName, keyType, valType, dictCap, dictInit);
        }

        // Otherwise it's a limited list declaration
        var typeName = ParseTypeString();
        Expect("LBRACKET");
        int capacity = Convert.ToInt32(Expect("NUMBER").Value);
        Expect("RBRACKET");
        var name = Expect("ID").Value.ToString();
        FabBase? init = null;
        if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
        Expect("SEMI");
        return new FabListDecl(name, typeName, capacity, init);
    }

    // ── Var-or-list declaration ───────────────────────────────────────────────
    // type name;           → plain variable decl
    // type name = expr;    → typed assignment
    // type[] name;         → unlimited list
    // type[] name = [...]; → unlimited list with initialiser
    private FabBase VarOrListDeclaration()
    {
        string typeName;

        if (_tokens[_pos].Name == "LONG"
            && _pos + 1 < _tokens.Count
            && _tokens[_pos + 1].Name == "DOUBLE")
        {
            typeName = "ldouble";
            Advance();
            Advance();
        }
        else
        {
            typeName = _tokens[_pos].Value.ToString().ToLower();
            Advance();
        }

        if (Peek()?.Name == "OP" && Peek()?.Value?.ToString() == "*")
        {
            Advance(); // consume '*'
            var ptrName = Expect("ID").Value.ToString();
            if (Peek() is not { Name: "OP", Value: "=" })
                throw new FabException(CurrentLine(), $"Pointer '{ptrName}' must be initialized");
            Advance();
            var ptrExpr = Expr();
            Expect("SEMI");
            return new FabAssign(ptrName, ptrExpr, typeName + "*");
        }

        if (Peek()?.Name == "LBRACKET")
        {
            Advance(); // consume '['
            // type[] name  → unlimited list (empty brackets)
            if (Peek()?.Name == "RBRACKET")
            {
                Advance(); // consume ']'
                var listName = Expect("ID").Value.ToString();
                FabBase? init = null;
                if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
                Expect("SEMI");
                return new FabListDecl(listName, typeName, capacity: -1, initializer: init);
            }
            // Backward compat: treat as plain var decl with array type
            Expect("RBRACKET");
            var arrName = Expect("ID").Value.ToString();
            if (Peek() is { Name: "SEMI" }) { Advance(); return new FabVarDecl(arrName, typeName + "[]"); }
            if (Peek() is not { Name: "OP", Value: "=" })
                throw new FabException(CurrentLine(), $"Expected '=' or ';' after '{arrName}'");
            Advance(); var arrExpr = Expr(); Expect("SEMI");
            return new FabAssign(arrName, arrExpr, typeName + "[]");
        }

        var declarations = new List<FabBase> { ParseTypedDeclItem(Expect("ID").Value.ToString(), typeName) };
        while (Peek()?.Name == "COMMA")
        {
            Advance(); // consume ','
            var nextName = Expect("ID").Value.ToString();
            declarations.Add(ParseTypedDeclItem(nextName, typeName));
        }
        Expect("SEMI");
        return declarations.Count == 1 ? declarations[0] : new FabStatementList(declarations);
    }

    // Parses the optional '= expr' part of a single declarator within a
    // (possibly comma-separated) variable declaration list, e.g. the 'a = 2'
    // and 'j' parts of 'int a = 2, j;'.
    private FabBase ParseTypedDeclItem(string name, string typeName)
    {
        if (Peek() is { Name: "OP", Value: "=" })
        {
            Advance();
            var expr = Expr();
            return new FabAssign(name, expr, typeName);
        }
        return new FabVarDecl(name, typeName);
    }

    // ── Helper: parse a type string (recursive — handles composites) ──────────
    // Supported forms:
    //   int / string / float / bool / char / byte / un int / ...   (primitive)
    //   type[]                                                       (list type)
    //   (keyType, valType)                                           (dict type)
    //   (keyType, valType)[]                                         (list of dicts)
    //   (keyType, type[])                                            (dict → list)
    //   (keyType, (kT, vT))                                          (dict → dict)
    //   ... and any deeper nesting of the above
    private string ParseTypeString()
    {
        string baseType;

        if (Peek()?.Name == "LPAREN")
        {
            // dict type: (keyType, valType)
            Advance(); // consume '('
            string keyType = ParseTypeString();
            Expect("COMMA");
            string valType = ParseTypeString();
            Expect("RPAREN");
            baseType = $"({keyType}, {valType})";
        }
        else if (Peek()?.Name == "LIST")
        {
            // list<elemType>  →  same internal representation as 'elemType[]'
            Advance(); // consume 'list'
            ExpectOp("<");
            string elemType = ParseTypeString();
            ExpectGT();
            baseType = $"{elemType}[]";
        }
        else if (Peek()?.Name == "DICT")
        {
            // dict<keyType, valType>  →  same internal representation as '(keyType, valType)'
            Advance(); // consume 'dict'
            ExpectOp("<");
            string dKeyType = ParseTypeString();
            Expect("COMMA");
            string dValType = ParseTypeString();
            ExpectGT();
            baseType = $"({dKeyType}, {dValType})";
        }
        else if (Peek()?.Name == "LONG")
        {
            if (_pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "DOUBLE")
            {
                Advance();
                Advance();
                baseType = "ldouble";
            }
            else
            {
                baseType = "long";
                Advance();
            }
        }
        else if (Peek()?.Name == "VOID")
        {
            Advance();
            baseType = "void";
        }
        else if (Peek()?.Name == "TUPLE")
        {
            Advance();
            baseType = "tuple";
        }
        else if (Peek()?.Name == "VFREE")
        {
            Advance();
            baseType = "vfree";
        }
        else if (Peek()?.Name == "UN")
        {
            Advance();
            baseType = Peek()?.Name switch
            {
                "INT" => "uint",
                "SHORT" => "ushort",
                "LONG" => "ulong",
                "BYTE" => "sbyte",
                _ => throw new FabException(CurrentLine(), $"'un' cannot be used with type '{Peek()?.Value}'")
            };
            Advance();
        }
        else
        {
            baseType = Peek()?.Value?.ToString()?.ToLower()
                ?? throw new FabException(CurrentLine(), "Expected type name");
            Advance();
        }

        // Check for trailing [] — makes this a list type
        if (Peek()?.Name == "LBRACKET")
        {
            int savedPos = _pos;
            Advance(); // consume '['
            if (Peek()?.Name == "RBRACKET")
            {
                Advance(); // consume ']'
                baseType = baseType + "[]";
            }
            else
            {
                // Not '[]' — restore position; caller will handle '[N]' for limited lists
                _pos = savedPos;
            }
        }

        return baseType;
    }

    // ── struct definition ─────────────────────────────────────────────────────
    // Syntax:
    //   struct Point { int x = 0; int y = 0; }
    //
    // All fields are public (no visibility keywords needed, but allowed).
    // No methods, no inheritance, no constructor.
    // Value semantics: assignment copies the struct.
    private FabBase StructStatement()
    {
        Advance(); // consume 'struct'

        // struct StructName varName [= {...}];  — variable declaration using a known struct
        // Detected by: ID followed by another ID (struct type then var name)
        if (Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "ID")
        {
            return StructVarDeclaration();
        }

        // struct StructName { ... }  — type definition
        var structName = Expect("ID").Value.ToString();
        Expect("LBRACE");

        var fields = new List<AST.FabFieldDef>();
        while (Peek() is not null && Peek()?.Name != "RBRACE")
        {
            // Optional (ignored) visibility keyword — structs are always public
            if (Peek()?.Name is "PUBLIC" or "PRIVATE") Advance();

            var typeName = ParseTypeString();
            var fieldName = Expect("ID").Value.ToString();
            AST.FabBase? defaultVal = null;
            if (Peek() is { Name: "OP", Value: "=" }) { Advance(); defaultVal = Expr(); }
            Expect("SEMI");
            fields.Add(new AST.FabFieldDef("public", typeName, fieldName, defaultVal));
        }

        Expect("RBRACE");
        return new AST.FabStructDefStmt(new AST.FabStructDef(structName, fields));
    }

    // struct StructName varName [= { field: val, ... }];
    private FabBase StructVarDeclaration()
    {
        var structName = Expect("ID").Value.ToString();
        var varName = Expect("ID").Value.ToString();
        AST.FabBase? init = null;
        if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
        Expect("SEMI");
        return new AST.FabStructDecl(structName, varName, init);
    }

    // ── Class declaration ────────────────────────────────────────────────────
    // Syntax:
    //   class ClassName { ... }
    //   class Child : Parent { ... }
    //   {
    //       public int x;
    //       private string name = "default";
    //
    //       public def ClassName(int px):   // constructor
    //       {
    //           x = px;                     // no 'this' needed
    //       }
    //
    //       public def move(int dx) -> int:
    //       {
    //           x += dx;
    //           return x;
    //       }
    //   }
    private FabBase StaticClassStatement()
    {
        Advance(); // consume 'static'
        if (Peek()?.Name != "CLASS")
            throw new FabException(CurrentLine(), $"Expected 'class' after 'static', got '{Peek()?.Value ?? "EOF"}'");
        return ClassStatement(isStatic: true);
    }

    private FabBase ClassStatement(bool isStatic = false)
    {
        Advance(); // consume 'class'
        var className = Expect("ID").Value.ToString();

        // Optional inheritance: class Dog : Animal
        string? baseClassName = null;
        if (Peek()?.Name == "COLON")
        {
            Advance(); // consume ':'
            baseClassName = Expect("ID").Value.ToString();
        }

        Expect("LBRACE");

        var fields = new List<FabFieldDef>();
        var methods = new Dictionary<string, FabFuncDef>();
        FabFuncDef? ctor = null;

        while (Peek() is not null && Peek()?.Name != "RBRACE")
        {
            string visibility = "public";
            if (Peek()?.Name == "PUBLIC") { visibility = "public"; Advance(); }
            else if (Peek()?.Name == "PRIVATE") { visibility = "private"; Advance(); }

            if (Peek()?.Name == "DEF")
            {
                Advance(); // consume 'def'
                var methodName = Expect("ID").Value.ToString();
                Expect("LPAREN");

                var parms = new List<(string, string)>();
                while (Peek()?.Name != "RPAREN")
                {
                    string pType = ParseTypeString();
                    var pName = Expect("ID").Value.ToString();
                    parms.Add((pType, pName));
                    if (Peek()?.Name == "COMMA") Advance();
                }
                Expect("RPAREN");

                string? returnType = null;
                if (Peek()?.Name == "ARROW") { Advance(); returnType = ParseTypeString(); }

                var body = ParseBlock();

                var funcDef = new FabFuncDef(methodName, parms, returnType, body);
                funcDef.Visibility = visibility;

                if (methodName == className)
                    ctor = funcDef;
                else
                    methods[methodName] = funcDef;

                continue;
            }

            // Field declaration: [visibility] type name [= expr] ;
            bool isConst = false;
            if (Peek()?.Name == "CONST") { isConst = true; Advance(); }

            var typeName = ParseTypeString();
            var fieldName = Expect("ID").Value.ToString();
            FabBase? defaultVal = null;
            if (Peek() is { Name: "OP", Value: "=" }) { Advance(); defaultVal = Expr(); }
            Expect("SEMI");
            fields.Add(new FabFieldDef(visibility, typeName, fieldName, defaultVal, isConst));
        }

        Expect("RBRACE");
        return new FabClasses(className, baseClassName, fields, methods, ctor, isStatic);
    }

    // ─────────────────────────────────────────────────────────────────────────

    private FabBase FuncDefStatement()
    {
        Advance();
        var name = Expect("ID").Value.ToString();
        Expect("LPAREN");

        var parms = new List<(string, string)>();
        while (Peek()?.Name != "RPAREN")
        {
            string pType = ParseTypeString();
            var pName = Expect("ID").Value.ToString();
            parms.Add((pType, pName));
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RPAREN");

        string? returnType = null;
        if (Peek()?.Name == "ARROW")
        {
            Advance();
            returnType = ParseTypeString();
        }

        var body = ParseBlock();
        return new FabFuncDef(name, parms, returnType, body);
    }

    // ── native def name(params) [-> type];                    — bodyless stub;
    //    calling it throws unless a native implementation was registered on
    //    the interpreter (FabInterpreter.RegisterNative) or, in compiled
    //    output, hand-written under the exact same mangled name/signature.
    // ── native def name(params) [-> type] => otherFunc(args);  — delegation
    //    sugar for 'def name(params) [-> type] { return otherFunc(args); }';
    //    fully functional, 'native' here is just documentation that this is
    //    a thin forwarding wrapper.
    // ── native def name(params) [-> type] { ... }              — also
    //    allowed, behaves exactly like a normal 'def' with a real body.
    private FabBase NativeFuncDefStatement()
    {
        Advance(); // consume 'native'
        if (Peek()?.Name != "DEF")
            throw new FabException(CurrentLine(), $"Expected 'def' after 'native', got '{Peek()?.Value ?? "EOF"}'");
        Advance(); // consume 'def'

        var name = Expect("ID").Value.ToString();
        Expect("LPAREN");

        var parms = new List<(string, string)>();
        while (Peek()?.Name != "RPAREN")
        {
            string pType = ParseTypeString();
            var pName = Expect("ID").Value.ToString();
            parms.Add((pType, pName));
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RPAREN");

        string? returnType = null;
        if (Peek()?.Name == "ARROW")
        {
            Advance();
            returnType = ParseTypeString();
        }

        List<FabBase> body;
        if (Peek()?.Name == "FATARROW")
        {
            Advance(); // consume '=>'
            var expr = Expr();
            Expect("SEMI");
            body = new List<FabBase> { new FabReturn(expr) };
        }
        else if (Peek()?.Name == "SEMI")
        {
            Advance(); // bodyless — must be supplied natively at call time
            body = new List<FabBase>();
        }
        else
        {
            body = ParseBlock();
        }

        return new FabFuncDef(name, parms, returnType, body) { IsNative = true };
    }

    private FabBase ReturnStatement()
    {
        Advance();
        if (Peek()?.Name == "SEMI") { Advance(); return new FabReturn(null); }
        var expr = Expr();
        Expect("SEMI");
        return new FabReturn(expr);
    }

    private FabBase ThrowStatement()
    {
        Advance();
        if (Peek()?.Name == "SEMI") { Advance(); throw new Exception("Command 'throw' need argument!"); }
        var expr = Expr();
        Expect("SEMI");
        return new FabThrow(expr);
    }

    // ── try { ... } catch (Type name) { ... } catch (name) { ... } finally { ... } ──
    // Multiple catch clauses are tried in order; the first whose declared type
    // matches the thrown value wins. 'catch (string e)' matches plain 'throw'
    // values (since a bare throw carries no type info beyond its runtime value)
    // as well as internal engine errors (index out of range, undefined
    // variable, etc.), which surface as string messages. 'catch (ErrorType e)'
    // matches instances of that class (or any of its subclasses). A bare
    // 'catch (e)' with no type name, or 'catch { ... }' with no bound
    // variable, matches anything.
    private FabBase TryStatement()
    {
        Advance(); // consume 'try'
        var body = ParseBlock();

        var catches = new List<(string?, string, List<FabBase>)>();
        while (Peek()?.Name == "CATCH")
        {
            Advance(); // consume 'catch'

            if (Peek()?.Name == "LBRACE")
            {
                // catch { ... }  — catch-all, no bound variable
                var allBody = ParseBlock();
                catches.Add((null, "__err__", allBody));
                continue;
            }

            Expect("LPAREN");
            string? typeName;
            string varName;

            if (Peek()?.Name == "STRING")
            {
                // catch (string e)
                typeName = "string";
                Advance();
                varName = Expect("ID").Value.ToString();
            }
            else if (Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "ID")
            {
                // catch (TypeError e)
                typeName = _tokens[_pos++].Value.ToString();
                if (!AST.FabErrorTypes.IsKnown(typeName))
                    throw new FabException(CurrentLine(),
                        $"Unknown error type '{typeName}' in catch clause. Valid types: " +
                        string.Join(", ", AST.FabErrorTypes.Hierarchy.Keys) + ", or 'string'.");
                varName = Expect("ID").Value.ToString();
            }
            else
            {
                // catch (e)  — matches anything
                typeName = null;
                varName = Expect("ID").Value.ToString();
            }

            Expect("RPAREN");
            var cbody = ParseBlock();
            catches.Add((typeName, varName, cbody));
        }

        List<FabBase>? finallyBody = null;
        if (Peek()?.Name == "FINALLY")
        {
            Advance();
            finallyBody = ParseBlock();
        }

        if (catches.Count == 0 && finallyBody == null)
            throw new FabException(CurrentLine(), "'try' must be followed by at least one 'catch' or a 'finally' block");

        return new FabTry(body, catches, finallyBody);
    }

    // ── switch (expr) { case val: ... case val: ... default: ... } ────────────
    // Supports fall-through prevention via break; each case body runs until a
    // break statement or the next case/default/closing brace.
    // Multiple values per case: case 1, 2, 3: (comma-separated)
    private FabBase SwitchStatement()
    {
        Advance(); // consume 'switch'
        Expect("LPAREN");
        var expr = Expr();
        Expect("RPAREN");
        Expect("LBRACE");

        var cases = new List<(List<FabBase> Values, List<FabBase> Body)>();
        List<FabBase>? defaultBody = null;

        while (Peek() is not null && Peek()?.Name != "RBRACE")
        {
            if (Peek()?.Name == "DEFAULT")
            {
                Advance(); // consume 'default'
                Expect("COLON");
                defaultBody = ParseCaseBody();
            }
            else
            {
                Expect("CASE");
                // Parse one or more comma-separated values
                var values = new List<FabBase>();
                values.Add(Expr());
                while (Peek()?.Name == "COMMA") { Advance(); values.Add(Expr()); }
                Expect("COLON");
                var body = ParseCaseBody();
                cases.Add((values, body));
            }
        }

        Expect("RBRACE");
        return new FabSwitch(expr, cases, defaultBody);
    }

    /// <summary>
    /// Reads statements until the next case/default/closing brace or a break.
    /// The break token itself is consumed so it doesn't leak into the outer parser.
    /// </summary>
    private List<FabBase> ParseCaseBody()
    {
        var body = new List<FabBase>();
        while (Peek() is not null
               && Peek()?.Name != "CASE"
               && Peek()?.Name != "DEFAULT"
               && Peek()?.Name != "RBRACE")
        {
            if (Peek()?.Name == "BREAK") { Advance(); Expect("SEMI"); break; }
            body.Add(Statement());
        }
        return body;
    }

    private FabBase BreakStatement() { Advance(); Expect("SEMI"); return new FabBreak(); }

    private FabBase ContinueStatement()
    { Advance(); Expect("SEMI"); return new FabContinue(); }

    private FabBase DeleteStatement()
    {
        Advance();
        var name = Expect("ID").Value.ToString();
        Expect("SEMI");
        return new FabDelete(name);
    }

    private FabBase ConstDeclaration()
    {
        Advance(); // consume 'const'

        // const vfree name := expr  — inferred minimal type, fully immutable
        if (Peek()?.Name == "VFREE")
        {
            Advance(); // consume 'vfree'
            var vfName = Expect("ID").Value.ToString();
            if (Peek() is not { Name: "OP", Value: ":=" })
                throw new FabException(CurrentLine(),
                    $"'const vfree' requires ':=' (got '{Peek()?.Value ?? "EOF"}'); use 'vfree {vfName} = ...' for a mutable free variable");
            Advance();
            var vfExpr = Expr();
            Expect("SEMI");
            return new FabConstVFree(vfName, vfExpr);
        }

        var typeName = _tokens[_pos].Value.ToString().ToLower();
        Advance();
        var name = Expect("ID").Value.ToString();
        if (Peek() is not { Name: "OP", Value: "=" })
            throw new FabException(CurrentLine(), $"const '{name}' must be initialized at declaration");
        Advance();
        var expr = Expr();
        Expect("SEMI");
        return new FabConst(name, expr, typeName);
    }

    private FabBase VFreeStatement()
    {
        Advance();
        var name = Expect("ID").Value.ToString();

        // vfree name(params) => { ... };   /   vfree name(params) => expr;
        // Sugar for: vfree name = (params) => { ... };
        if (Peek()?.Name == "LPAREN")
        {
            var lambda = ParseLambdaTail();
            return new FabVFreeDecl(name, lambda, locked: false);
        }

        var op = Peek()?.Value?.ToString();
        if (op == ":=") { Advance(); var e = Expr(); Expect("SEMI"); return new FabVFreeDecl(name, e, locked: true); }
        if (op == "=") { Advance(); var e = Expr(); Expect("SEMI"); return new FabVFreeDecl(name, e, locked: false); }
        throw new FabException(CurrentLine(), $"Expected '=' or ':=' after vfree '{name}'");
    }

    // ── Lambda literals: (params) => { statements }   /   (params) => expr ──
    // Params are plain untyped names (dynamically typed, like everything else
    // reached through 'vfree'). Used both for the 'vfree name(...) => ...'
    // declaration shorthand and as a general primary expression, so lambdas
    // can be passed as arguments, stored in lists/dicts, or returned from
    // functions. The '(' has NOT been consumed yet when this is called.
    private FabBase ParseLambdaTail()
    {
        Expect("LPAREN");
        var parms = new List<string>();
        while (Peek()?.Name != "RPAREN")
        {
            parms.Add(Expect("ID").Value.ToString());
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RPAREN");

        if (Peek()?.Name != "FATARROW")
            throw new FabException(CurrentLine(), $"Expected '=>' after lambda parameter list, got '{Peek()?.Value ?? "EOF"}'");
        Advance(); // consume '=>'

        List<FabBase> body;
        if (Peek()?.Name == "LBRACE")
        {
            body = ParseBlock();
        }
        else
        {
            var expr = Expr();
            Expect("SEMI");
            body = new List<FabBase> { new FabReturn(expr) };
        }
        return new FabLambdaLiteral(parms, body);
    }

    /// <summary>
    /// Lookahead-only (no tokens consumed): true if the current position looks
    /// like '(a, b, ...) =>', i.e. a lambda expression rather than a
    /// parenthesised expression, tuple literal, or cast.
    /// </summary>
    private bool IsLambdaAhead()
    {
        if (Peek()?.Name != "LPAREN") return false;
        int depth = 0;
        for (int i = _pos; i < _tokens.Count; i++)
        {
            var t = _tokens[i];
            if (t.Name == "LPAREN") depth++;
            else if (t.Name == "RPAREN")
            {
                depth--;
                if (depth == 0)
                    return i + 1 < _tokens.Count && _tokens[i + 1].Name == "FATARROW";
            }
        }
        return false;
    }

    private FabBase IfStatement()
    {
        var branches = new List<(FabBase, List<FabBase>)>();
        List<FabBase>? elseBody = null;

        while (Peek()?.Name == "IF")
        {
            Advance();
            var condition = Condition();
            branches.Add((condition, ParseBlock()));

            if (Peek()?.Name != "ELSE") break;
            Advance();
            if (Peek()?.Name == "IF") continue;
            elseBody = ParseBlock();
            break;
        }
        return new FabIf(branches, elseBody);
    }

    private List<FabBase> ParseBlock()
    {
        Expect("LBRACE");
        var body = new List<FabBase>();
        while (Peek() is not null && Peek()?.Name != "RBRACE")
            body.Add(Statement());
        Expect("RBRACE");
        return body;
    }

    private FabBase Condition() => Expr();

    private FabBase OrExpr()
    {
        var left = AndExpr();
        while (Peek()?.Name == "OR") { Advance(); left = new FabLogical(left, "or", AndExpr()); }
        return left;
    }

    private FabBase AndExpr()
    {
        var left = ComparisonExpr();
        while (Peek()?.Name == "AND") { Advance(); left = new FabLogical(left, "and", ComparisonExpr()); }
        return left;
    }

    // Type-keyword token names recognised on the left side of a reversed
    // 'is' check, e.g. 'int is 42' (equivalent to '42 is int').
    private static readonly HashSet<string> _isTypeTokenNames = new()
        { "INT", "SHORT", "LONG", "FLOAT", "DOUBLE", "STRING", "BOOL", "CHAR", "BYTE",
          "LIST", "DICT", "TUPLE", "VOID" };

    private FabBase ComparisonExpr()
    {
        // Reversed form: 'null is expr'  (e.g. 'null is x')
        if (Peek()?.Name == "NULL_LIT" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "IS")
        {
            Advance(); // consume 'null'
            Advance(); // consume 'is'
            return new FabIs(ArithExpr(), "null");
        }

        // Reversed form: 'un int is expr'  (e.g. 'un int is 250')
        if (Peek()?.Name == "UN" && _pos + 2 < _tokens.Count && _tokens[_pos + 2].Name == "IS")
        {
            Advance(); // consume 'un'
            string unTypeName = Peek()?.Name switch
            {
                "INT" => "uint",
                "SHORT" => "ushort",
                "LONG" => "ulong",
                "BYTE" => "sbyte",
                _ => throw new FabException(CurrentLine(), $"Unknown type after 'un' in 'is': '{Peek()?.Value}'")
            };
            Advance(); // consume the sub-type keyword
            Expect("IS");
            return new FabIs(ArithExpr(), unTypeName);
        }

        // Reversed form: 'typeName is expr'  (e.g. 'int is 42')
        if (Peek() != null && _isTypeTokenNames.Contains(Peek()!.Name)
            && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "IS")
        {
            string revTypeName = Peek()!.Value.ToString()!.ToLower();
            Advance(); // consume the type keyword
            Advance(); // consume 'is'
            return new FabIs(ArithExpr(), revTypeName);
        }

        var left = ArithExpr();

        // 'is' type-check: expr is typeName
        if (Peek()?.Name == "IS")
        {
            Advance();
            // Parse the type name — may be 'un int' etc.
            string typeName;
            if (Peek()?.Name == "UN")
            {
                Advance();
                typeName = Peek()?.Name switch
                {
                    "INT" => "uint",
                    "SHORT" => "ushort",
                    "LONG" => "ulong",
                    "BYTE" => "sbyte",
                    _ => throw new FabException(CurrentLine(), $"Unknown type after 'un' in 'is': '{Peek()?.Value}'")
                };
                Advance();
            }
            else
            {
                var tok = Peek() ?? throw new FabException(CurrentLine(), "Expected type name after 'is'");
                // 'null' is lexed as a NULL_LIT token whose Value is the actual
                // C# null (not the string "null"), so it must be special-cased
                // before calling ToString() on it.
                if (tok.Name == "NULL_LIT")
                {
                    typeName = "null";
                }
                else
                {
                    // Preserve original case for user-defined class names (ID tokens);
                    // built-in type keywords are already lowercased via their token value.
                    typeName = tok.Name == "ID"
                        ? tok.Value.ToString()
                        : tok.Value.ToString().ToLower();
                }
                Advance();
            }
            return new FabIs(left, typeName);
        }

        // 'in' membership: expr in collection
        if (Peek()?.Name == "IN")
        {
            Advance();
            var collection = ArithExpr();
            return new FabIn(left, collection);
        }

        var op = Peek()?.Value?.ToString();
        if (op is "==" or "!=" or "<" or ">" or "<=" or ">=")
        {
            Advance();
            return new FabComparison(left, op, ArithExpr());
        }
        return left;
    }

    private FabBase WhileStatement()
    {
        Advance(); Expect("LPAREN");
        var cond = Condition();
        Expect("RPAREN");
        return new FabWhile(cond, ParseBlock());
    }

    private FabBase ForStatement()
    {
        Advance(); Expect("LPAREN");

        // for (type name in collection) { ... }   /   for (name in collection) { ... }
        if (IsForEachAhead())
            return ForEachStatement();

        string counterName = PeekCounterName();
        FabBase init = Peek()?.Name switch
        {
            "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE" or "BOOL" or "CHAR" or "BYTE" or "SBYTE" => VarOrListDeclaration(),
            "UN" => UnVarDeclaration(),
            _ => throw new FabException(CurrentLine(), "Expected variable declaration in for-loop init")
        };
        var condition = Condition();
        Expect("SEMI");
        var step = ParseStep();
        Expect("RPAREN");
        return new FabFor(init, condition, step, ParseBlock(), counterName);
    }

    // ── for (type name in expr) { ... }  /  for (name in expr) { ... } ────────
    // Detects the construct without consuming tokens (full backtrack on exit).
    private bool IsForEachAhead()
    {
        int saved = _pos;
        try
        {
            if (Peek()?.Name == "UN")
            {
                Advance();
                if (Peek()?.Name is not ("INT" or "SHORT" or "LONG" or "BYTE")) return false;
                Advance();
                return Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "IN";
            }

            if (Peek()?.Name is "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE" or "BOOL" or "CHAR" or "BYTE" or "STRING")
            {
                Advance();
                return Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "IN";
            }

            if (Peek()?.Name == "ID")
            {
                // 'ClassName varName in expr'  or  'varName in expr'
                if (_pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "ID")
                    return _pos + 2 < _tokens.Count && _tokens[_pos + 2].Name == "IN";
                return _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "IN";
            }

            return false;
        }
        finally { _pos = saved; }
    }

    private FabBase ForEachStatement()
    {
        string? typeName = null;
        string varName;

        if (Peek()?.Name == "UN")
        {
            Advance();
            typeName = Peek()?.Name switch
            {
                "INT" => "uint",
                "SHORT" => "ushort",
                "LONG" => "ulong",
                "BYTE" => "sbyte",
                _ => throw new FabException(CurrentLine(), $"'un' cannot be used with type '{Peek()?.Value}'")
            };
            Advance();
            varName = Expect("ID").Value.ToString();
        }
        else if (Peek()?.Name is "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE" or "BOOL" or "CHAR" or "BYTE" or "STRING")
        {
            typeName = _tokens[_pos++].Value.ToString().ToLower();
            varName = Expect("ID").Value.ToString();
        }
        else if (Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "ID")
        {
            typeName = _tokens[_pos++].Value.ToString(); // user class name — preserve case
            varName = Expect("ID").Value.ToString();
        }
        else
        {
            varName = Expect("ID").Value.ToString(); // reuse an existing variable
        }

        Expect("IN");
        var collection = Expr();
        Expect("RPAREN");
        var body = ParseBlock();
        return new FabForEach(varName, typeName, collection, body);
    }

    private string PeekCounterName()
    {
        int offset = Peek()?.Name == "UN" ? 2 : 1;
        return _pos + offset < _tokens.Count
            ? _tokens[_pos + offset].Value.ToString()
            : throw new FabException(CurrentLine(), "Cannot determine for-loop counter name");
    }

    private FabBase ParseStep()
    {
        var name = Expect("ID").Value.ToString();
        var op = Peek()?.Value?.ToString();
        if (op is "++" or "--") { Advance(); return new FabCompoundAssign(name, op); }
        if (op is "+=" or "-=" or "*=" or "/=" or "%=" or "^=") { Advance(); return new FabCompoundAssign(name, op, Expr()); }
        throw new FabException(CurrentLine(), $"Expected ++, --, +=, -=, *=, /=, %= or ^= in for-loop step, got '{op}'");
    }

    private FabBase AssignmentOrCompound()
    {
        var name = Expect("ID").Value.ToString();
        var op = Peek()?.Value?.ToString();

        // ── ClassName[] varName [= expr];  (list of user-defined class) ──
        // Detected by: ID followed by '[' ']' ID
        if (Peek()?.Name == "LBRACKET"
            && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "RBRACKET"
            && _pos + 2 < _tokens.Count && _tokens[_pos + 2].Name == "ID")
        {
            Advance(); // consume '['
            Advance(); // consume ']'
            var listVarName = Expect("ID").Value.ToString();
            FabBase? init = null;
            if (Peek() is { Name: "OP", Value: "=" }) { Advance(); init = Expr(); }
            Expect("SEMI");
            return new FabListDecl(listVarName, name, capacity: -1, initializer: init);
        }

        // ── ClassName varName = expr;  (user-defined class type declaration) ──
        // Detected by: ID followed immediately by another ID (the variable name).
        if (Peek()?.Name == "ID")
        {
            var varName = _tokens[_pos++].Value.ToString();
            if (Peek() is not { Name: "OP", Value: "=" })
                throw new FabException(CurrentLine(), $"Expected '=' after '{varName}' in typed declaration");
            Advance(); // consume '='
            var initExpr = Expr();
            Expect("SEMI");
            // Store as FabAssign with declaredType = className; at runtime the value
            // is stored directly (no primitive cast — class instances pass through the
            // '_' wildcard arm of CastDeclared).
            return new FabAssign(varName, initExpr, name);
        }

        if (Peek()?.Name == "DOT")
        {
            Advance(); // consume '.'
            var memberName = Expect("ID").Value.ToString();

            // obj.field = value  → instance field assignment
            if (Peek() is { Name: "OP", Value: "=" })
            {
                Advance();
                var rhs = Expr();
                Expect("SEMI");
                return new FabInstanceAssign(name, memberName, rhs);
            }

            // obj.method(args) or plain member access
            var args = Peek()?.Name == "LPAREN" ? ParseArgList() : new List<FabBase>();
            Expect("SEMI");
            return new FabMemberAccess(new FabVariable(name), memberName, args);
        }

        if (Peek()?.Name == "LBRACKET")
        {
            Advance();
            var index = Expr();
            Expect("RBRACKET");
            if (Peek() is { Name: "OP", Value: "=" })
            {
                Advance();
                var value = Expr();
                Expect("SEMI");
                return new FabIndexAssign(name, index, value);
            }
            FabBase idxNode = new FabIndexAccess(new FabVariable(name), index);
            idxNode = ParsePostfix(idxNode);
            Expect("SEMI");
            return idxNode;
        }

        if (Peek()?.Name == "LPAREN")
        {
            var call = ParseCallArgs(name);
            Expect("SEMI");
            return call;
        }

        if (op is "++" or "--") { Advance(); Expect("SEMI"); return new FabCompoundAssign(name, op); }
        if (op is "+=" or "-=" or "*=" or "/=" or "%=" or "^=") { Advance(); var e = Expr(); Expect("SEMI"); return new FabCompoundAssign(name, op, e); }
        if (op == "=") { Advance(); var e = Expr(); Expect("SEMI"); return new FabAssign(name, e); }

        throw new FabException(CurrentLine(), $"Invalid statement for '{name}': expected =, ++, --, +=, -=, *=, /=, %= or ^=, or ()");
    }

    private FabBase UseStatement()
    {
        Advance(); // consume 'use'

        // use namespace math;          → import + open namespace (no prefix needed)
        // use namespace "mylib.fab";   → import file + open namespace
        if (Peek()?.Name == "NAMESPACE")
            return UsingNamespaceStatement();

        // use math;           → plain import (prefix required: math.pow(...))
        // use "mylib.fab";    → plain file import
        if (Peek()?.Name == "STRING")
        {
            var path = _tokens[_pos++].Value.ToString();
            string libName = Path.GetFileNameWithoutExtension(path);
            HashSet<string>? only = TryParseOnlyClause();
            Expect("SEMI");
            return new FabUse(libName, filePath: path, onlyFunctions: only);
        }
        var name = Expect("ID").Value.ToString();
        HashSet<string>? onlyFuncs = TryParseOnlyClause();
        Expect("SEMI");
        return new FabUse(name, onlyFunctions: onlyFuncs);
    }

    /// <summary>
    /// If the next token is the 'only' keyword, consumes it and parses a
    /// comma-separated list of identifiers: only pow, sqrt, abs
    /// Returns null if 'only' is not present (unrestricted import).
    /// </summary>
    private HashSet<string>? TryParseOnlyClause()
    {
        if (Peek()?.Name != "ONLY") return null;
        Advance(); // consume 'only'

        var names = new HashSet<string>();
        // Expect at least one function name
        names.Add(Expect("ID").Value.ToString());

        // Collect additional comma-separated names
        while (Peek()?.Name == "COMMA")
        {
            Advance(); // consume ','
            names.Add(Expect("ID").Value.ToString());
        }

        return names;
    }

    // use namespace math;           — import builtin lib and open its namespace (no prefix needed)
    // use namespace "mylib.fab";    — import file and open its namespace
    // use namespace math only pow;  — restricted import + open
    // NOTE: 'use' is already consumed by UseStatement before this is called
    private FabBase UsingNamespaceStatement()
    {
        Advance();                      // consume 'namespace'
        if (Peek()?.Name == "STRING")
        {
            var path = _tokens[_pos++].Value.ToString();
            string libName = Path.GetFileNameWithoutExtension(path);
            HashSet<string>? only = TryParseOnlyClause();
            Expect("SEMI");
            return new FabUsingNamespace(libName, filePath: path, onlyFunctions: only);
        }
        var name = Expect("ID").Value.ToString();
        HashSet<string>? onlyFuncs = TryParseOnlyClause();
        Expect("SEMI");
        return new FabUsingNamespace(name, onlyFunctions: onlyFuncs);
    }

    // namespace Name { def foo() {...}  class Bar {...}  struct S {...} }
    // namespace alias = libname;   — create a short alias for an imported library/namespace
    private FabBase NamespaceDefStatement()
    {
        Advance();                      // consume 'namespace'
        var name = Expect("ID").Value.ToString();

        // namespace alias = target;
        if (Peek() is { Name: "OP", Value: "=" })
        {
            Advance(); // consume '='
            var target = Expect("ID").Value.ToString();
            Expect("SEMI");
            return new FabNamespaceAlias(name, target);
        }

        Expect("LBRACE");

        var funcs = new List<FabFuncDef>();
        var classes = new List<FabClasses>();
        var structs = new List<FabStructDefStmt>();

        while (Peek() is not null && Peek()?.Name != "RBRACE")
        {
            var stmt = Statement();
            switch (stmt)
            {
                case FabFuncDef fd: funcs.Add(fd); break;
                case FabClasses cl: classes.Add(cl); break;
                case FabStructDefStmt st: structs.Add(st); break;
                default:
                    throw new FabException(CurrentLine(),
                        $"Only functions, classes and structs are allowed inside 'namespace {name}'");
            }
        }

        Expect("RBRACE");
        return new FabNamespaceDef(name, funcs, classes, structs);
    }

    private FabCall ParseCallArgs(string name)
    {
        Expect("LPAREN");
        var args = new List<FabBase>();
        while (Peek()?.Name != "RPAREN")
        {
            args.Add(Expr());
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RPAREN");
        return new FabCall(name, args);
    }

    private FabBase UnVarDeclaration()
    {
        Advance();
        var baseType = Peek()?.Name switch
        {
            "INT" => "uint",
            "SHORT" => "ushort",
            "LONG" => "ulong",
            "BYTE" => "sbyte",
            _ => throw new FabException(CurrentLine(), $"'un' cannot be used with type '{Peek()?.Value ?? "EOF"}'")
        };
        Advance();
        var declarations = new List<FabBase> { ParseTypedDeclItem(Expect("ID").Value.ToString(), baseType) };
        while (Peek()?.Name == "COMMA")
        {
            Advance();
            var nextName = Expect("ID").Value.ToString();
            declarations.Add(ParseTypedDeclItem(nextName, baseType));
        }
        Expect("SEMI");
        return declarations.Count == 1 ? declarations[0] : new FabStatementList(declarations);
    }

    private FabBase WriteStatement()
    { Advance(); var e = Expr(); Expect("SEMI"); return new FabWrite(e); }

    private FabBase WritelnStatement()
    { Advance(); var e = Expr(); Expect("SEMI"); return new FabWriteln(e); }

    private FabBase InputInStatement()
    {
        Advance();
        if (Peek()?.Name is "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE" or "STRING" or "BOOL" or "CHAR" or "BYTE")
        {
            var t = _tokens[_pos].Value.ToString(); Advance();
            var n = Expect("ID").Value.ToString(); Expect("SEMI");
            return new FabInputIn(n, t);
        }
        if (Peek()?.Name == "UN")
        {
            Advance();
            var bt = Peek()?.Name switch
            {
                "INT" => "uint",
                "SHORT" => "ushort",
                "LONG" => "ulong",
                "BYTE" => "sbyte",
                _ => throw new FabException(CurrentLine(), $"'un' cannot be used with type '{Peek()?.Value ?? "EOF"}'")
            };
            Advance();
            var n = Expect("ID").Value.ToString(); Expect("SEMI");
            return new FabInputIn(n, bt);
        }
        var vn = Expect("ID").Value.ToString(); Expect("SEMI");
        return new FabInputIn(vn);
    }

    // ── Expression parser ─────────────────────────────────────────────────────

    private FabBase Expr() => OrExpr();

    private FabBase ArithExpr() => ArithTermTail(ArithTerm());

    private FabBase ArithTermTail(FabBase left)
    {
        if (Peek() is not { Name: "OP", Value: "+" or "-" } tok) return left;
        Advance();
        return ArithTermTail(new FabBinOp(left, tok.Value.ToString(), ArithTerm()));
    }

    private FabBase ArithTerm() => ArithFactorTail(Factor());

    private FabBase ArithFactorTail(FabBase left)
    {
        if (Peek() is not { Name: "OP", Value: "*" or "/" or "%" } tok) return left;
        Advance();
        return ArithFactorTail(new FabBinOp(left, tok.Value.ToString(), Factor()));
    }

    private FabBase Factor()
    {
        var tok = Peek();
        FabBase node;

        // Unary & - адрес переменной
        if (tok is { Name: "OP", Value: "&" })
        {
            Advance();
            var varName = Expect("ID").Value.ToString();
            node = new FabAddressOf(varName);
            return ParsePostfix(node);
        }

        // Unary * — разыменование
        if (tok is { Name: "OP", Value: "*" })
        {
            Advance();
            var operand = Factor();
            node = new FabDeref(operand);
            return ParsePostfix(node);
        }

        // Unary 'not'
        if (tok is { Name: "NOT" })
        {
            Advance();
            var operand = Factor();
            node = new FabComparison(operand, "==", new FabBoolLit(false));
            return ParsePostfix(node);
        }

        // Unary minus / plus
        if (tok is { Name: "OP", Value: "-" or "+" })
        {
            Advance();
            var operand = Factor();
            node = tok.Value.ToString() == "-"
                ? new FabBinOp(new FabNumber(0), "-", operand)
                : operand;
            return ParseExponentiation(ParsePostfix(node));
        }

        // Lambda expression: (params) => { ... }  /  (params) => expr
        if (tok?.Name == "LPAREN" && IsLambdaAhead())
        {
            node = ParseLambdaTail();
            return ParseExponentiation(ParsePostfix(node));
        }

        switch (tok?.Name)
        {
            case "NUMBER":
                node = new FabNumber(_tokens[_pos++].Value);
                break;
            case "CHAR_LIT":
                node = new FabChar((char)_tokens[_pos++].Value);
                break;
            case "STRING":
                node = new FabString(_tokens[_pos++].Value.ToString());
                break;
            case "FSTRING":
                node = new FabFString(_tokens[_pos++].Value.ToString());
                break;
            case "BOOL_LIT":
                node = new FabBoolLit((bool)_tokens[_pos++].Value);
                break;
            case "NULL_LIT":
                _pos++;
                node = new FabNullLit();
                break;
            case "TYPE":
                return ParseTypeCall();
            case "SIZE_OF":
                return ParseSizeOfCall();
            case "SYSTEM":
                return ParseSystemCall();
            case "LPAREN":
                node = ParseParenthesizedExpr();
                break;

            case "LBRACKET":
                node = ParseListLiteral();
                break;

            case "LBRACE":
                node = ParseDictLiteral();
                break;

            case "NEW":
                {
                    Advance(); // consume 'new'
                    var newClassName = Expect("ID").Value.ToString();
                    var ctorArgs = Peek()?.Name == "LPAREN" ? ParseArgList() : new List<FabBase>();
                    node = new FabNew(newClassName, ctorArgs);
                    break;
                }

            case "ID":
                {
                    var name = _tokens[_pos++].Value.ToString();

                    if (Peek()?.Name == "DOT" && AST.FabLibRegistry.HasLib(name))
                    {
                        Advance();
                        var funcName = Expect("ID").Value.ToString();
                        List<FabBase>? args = null;
                        if (Peek()?.Name == "LPAREN") args = ParseArgList();
                        node = new FabLibCall(name, funcName, args);
                        break;
                    }

                    if (Peek()?.Name == "LPAREN")
                    {
                        node = ParseCallArgs(name);
                        break;
                    }

                    node = new FabVariable(name);
                    break;
                }

            case "INT":
            case "SHORT":
            case "LONG":
            case "FLOAT":
            case "DOUBLE":
            case "BYTE":
            case "UINT":
            case "USHORT":
            case "ULONG":
                {
                    var typeName = _tokens[_pos++].Value.ToString().ToLower();
                    Expect("DOT");
                    var member = Expect("ID").Value.ToString();
                    List<FabBase>? args = null;
                    if (Peek()?.Name == "LPAREN") args = ParseArgList();
                    node = new FabTypeMethod(typeName, member, args);
                    break;
                }

            default:
                throw new FabException(CurrentLine(), $"Unexpected token '{tok?.Value ?? "EOF"}'");
        }

        node = ParsePostfix(node);
        return ParseExponentiation(node);
    }

    /// <summary>Parse [expr, expr, ...] into a FabListLiteral (elementType="object", capacity=-1).</summary>
    private FabBase ParseListLiteral()
    {
        Expect("LBRACKET");
        var items = new List<FabBase>();
        while (Peek()?.Name != "RBRACKET")
        {
            items.Add(Expr());
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RBRACKET");
        return new FabListLiteral("object", -1, items);
    }

    /// <summary>Parse {key: val, key: val, ...} into a FabDictLiteral.</summary>
    private FabBase ParseDictLiteral()
    {
        Expect("LBRACE");
        var pairs = new List<(FabBase, FabBase)>();
        while (Peek()?.Name != "RBRACE")
        {
            // If the key is a bare identifier followed by ':', treat it as a string key
            // (struct initialiser syntax: { x: 10, y: 20 })
            FabBase key;
            if (Peek()?.Name == "ID" && _pos + 1 < _tokens.Count && _tokens[_pos + 1].Name == "COLON")
                key = new AST.FabString(_tokens[_pos++].Value.ToString());
            else
                key = Expr();
            Expect("COLON");
            var val = Expr();
            pairs.Add((key, val));
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RBRACE");
        return new AST.FabDictLiteral("object", "object", -1, pairs);
    }

    private FabBase ParseExponentiation(FabBase left)
    {
        if (Peek() is not { Name: "OP", Value: "^" }) return left;
        Advance();
        return new FabBinOp(left, "^", ParseExponentiation(Factor()));
    }

    private FabBase ParsePostfix(FabBase node)
    {
        while (true)
        {
            if (Peek()?.Name == "DOT")
            {
                Advance();
                // Support t.0, t.1 — numeric tuple index access
                if (Peek()?.Name == "NUMBER")
                {
                    var numTok = _tokens[_pos++];
                    // Convert to integer string for member name, e.g. "0", "1"
                    int idx = Convert.ToInt32(numTok.Value);
                    node = new FabMemberAccess(node, idx.ToString(), null);
                }
                else
                {
                    var member = Expect("ID").Value.ToString();
                    List<FabBase>? args = null;
                    if (Peek()?.Name == "LPAREN") args = ParseArgList();
                    node = new FabMemberAccess(node, member, args);
                }
            }
            else if (Peek()?.Name == "LBRACKET")
            {
                Advance();
                var idx = Expr();
                Expect("RBRACKET");
                node = new FabIndexAccess(node, idx);
            }
            else break;
        }
        return node;
    }

    private List<FabBase> ParseArgList()
    {
        Expect("LPAREN");
        var args = new List<FabBase>();
        while (Peek()?.Name != "RPAREN")
        {
            args.Add(Expr());
            if (Peek()?.Name == "COMMA") Advance();
        }
        Expect("RPAREN");
        return args;
    }

    private FabBase ParseTypeCall()
    { Advance(); Expect("LPAREN"); var n = Expect("ID").Value.ToString(); Expect("RPAREN"); return new FabType(n); }

    private FabBase ParseSystemCall()
    { Advance(); Expect("LPAREN"); var n = Expect("STRING").Value.ToString(); Expect("RPAREN"); Expect("SEMI"); return new FabSystem(n); }

    private FabBase ParseSizeOfCall()
    { Advance(); Expect("LPAREN"); var n = Expect("ID").Value.ToString(); Expect("RPAREN"); return new FabSizeOf(n); }

    private FabBase ParseParenthesizedExpr()
    {
        Advance(); // consume '('

        // Empty tuple: ()
        if (Peek()?.Name == "RPAREN")
        {
            Advance();
            return new FabTupleLiteral(new List<FabBase>());
        }

        // ── C-style cast: (type)expr ──────────────────────────────────────
        // Detect pattern: ( <type-keyword> ) followed by a non-operator token
        // (i.e. something that starts an expression — not a binary op).
        // Handles: (int)x  (float)3  (string)n  (un int)x  etc.
        if (TryParseCast(out var castNode))
            return castNode!;

        var first = Expr();
        // Single expression in parens — not a tuple
        if (Peek()?.Name == "RPAREN")
        {
            Advance();
            return first;
        }
        // Multiple expressions — tuple literal
        var items = new List<FabBase> { first };
        while (Peek()?.Name == "COMMA")
        {
            Advance();
            items.Add(Expr());
        }
        Expect("RPAREN");
        return new FabTupleLiteral(items);
    }

    /// <summary>
    /// Tries to parse a C-style cast expression: (typeName)expr.
    /// The opening '(' has already been consumed by ParseParenthesizedExpr.
    /// Returns true and sets <paramref name="node"/> if a cast was recognised;
    /// otherwise restores position and returns false.
    /// </summary>
    private bool TryParseCast(out FabBase? node)
    {
        node = null;
        int savedPos = _pos;

        // Recognised type-keyword token names (primitive types only for casting)
        static bool IsTypeKeyword(string name) =>
            name is "INT" or "SHORT" or "LONG" or "FLOAT" or "DOUBLE"
                 or "STRING" or "BOOL" or "CHAR" or "BYTE";

        string? castType = null;

        if (Peek()?.Name == "UN")
        {
            Advance(); // consume 'un'
            castType = Peek()?.Name switch
            {
                "INT" => "uint",
                "SHORT" => "ushort",
                "LONG" => "ulong",
                "BYTE" => "sbyte",
                _ => null
            };
            if (castType == null) { _pos = savedPos; return false; }
            Advance(); // consume the sub-type keyword
        }
        else if (Peek()?.Name != null && IsTypeKeyword(Peek()!.Name))
        {
            castType = Peek()!.Value.ToString()!.ToLower();
            Advance();
        }

        if (castType == null || Peek()?.Name != "RPAREN")
        {
            _pos = savedPos;
            return false;
        }

        Advance(); // consume ')'

        // What follows must start an expression (not a binary operator or comma etc.)
        // If next token looks like it can open a Factor(), we have a cast.
        var next = Peek();
        bool canBeExpr = next?.Name is "NUMBER" or "STRING" or "FSTRING" or "CHAR_LIT"
                                     or "BOOL_LIT" or "NULL_LIT" or "ID"
                                     or "LPAREN" or "LBRACKET" or "LBRACE" or "NEW"
            || (next is { Name: "OP", Value: "-" or "+" })
            || next?.Name == "NOT";

        if (!canBeExpr)
        {
            _pos = savedPos;
            return false;
        }

        var operand = Factor(); // parse the expression being cast
        node = new FabCast(castType, operand);
        return true;
    }
}