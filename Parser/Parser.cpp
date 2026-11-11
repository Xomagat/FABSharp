//
// Created by Xomagat on 07.08.2026.
//

#include "Parser.h"

// vars

// funcs
static void span_to_prev(SrcLoc& l, const std::vector<Token>& t, int pos)
{
    if (pos <= 0) return;
    auto& e = t[pos - 1].loc;
    if (e.line == l.line && e.col + e.len > l.col) l.len = e.col + e.len - l.col;
}

static bool is_member_name(token_type t)
{
    return t == token_type::WORDS || t == token_type::WRITE
        || t == token_type::WRITELN || t == token_type::INPUT_IN;
}

Parser::Parser(std::vector<Token> tokens, std::filesystem::path base_dir, std::filesystem::path exe_dir)
{
    eof = Token(token_type::eof, "");

    this->tokens = tokens;

    if (!tokens.empty()) { eof.loc = tokens.back().loc; eof.loc.col += eof.loc.len; eof.loc.len = 1; }

    size = tokens.size();

    this->base_dir = base_dir;
    this->exe_dir  = exe_dir;

    pos = 0;
}

std::vector<std::unique_ptr<Statement>> Parser::parse()
{
    for (int i = 0; i + 1 < size; ++i)
        if (tokens[i].get_type() == token_type::CLASS && tokens[i + 1].get_type() == token_type::WORDS)
            class_names.insert(tokens[i + 1].get_text());

    std::vector<std::unique_ptr<Statement>> result;

    if (!loaded_libs.contains("@types"))
    {
        for (auto& e : std::filesystem::directory_iterator(exe_dir / "lib" / "types"))
            if (e.path().extension() == ".manifest")
                load_stdlib_manifest(e.path());
        loaded_libs.insert("@types");
    }

    int depth = 0;
    std::vector<std::string> names;
    std::vector<std::pair<int, size_t>> opened;

    for (int i = 0; i + 1 < size; ++i)
    {
        auto t = tokens[i].get_type();

        if (t == token_type::NAMESPACE && tokens[i + 1].get_type() == token_type::WORDS
            && (i == 0 || tokens[i - 1].get_type() != token_type::USE))
        {
            size_t cnt = 0; int j = i + 1;
            while (j < size && tokens[j].get_type() == token_type::WORDS)
            {
                names.push_back(tokens[j].get_text()); ++cnt;
                if (j + 1 < size && tokens[j + 1].get_type() == token_type::SCOPE) j += 2;
                else { ++j; break; }
            }
            opened.push_back({depth + 1, cnt});
            i = j - 1;
        }
        else if (t == token_type::LBRACKET) ++depth;
        else if (t == token_type::RBRACKET)
        {
            if (!opened.empty() && opened.back().first == depth)
            {
                names.resize(names.size() - opened.back().second);
                opened.pop_back();
            }
            --depth;
        }
        else if (t == token_type::CLASS && tokens[i + 1].get_type() == token_type::WORDS)
        {
            std::string q;
            for (auto& n : names) q += n + "::";
            class_names.insert(q + tokens[i + 1].get_text());
        }
    }

    while (!match(token_type::eof)) parse_top(result);

    return result;
}

std::unique_ptr<Statement> Parser::statement()
{
    SrcLoc l = here();
    auto s = statement_impl();
    if (s) { span_to_prev(l, tokens, pos); if (!s->loc.valid()) s->loc = l; }
    return s;
}

std::unique_ptr<Statement> Parser::statement_impl()
{
    switch (get(0).get_type())
    {
        case token_type::USE: {
            consume(token_type::USE);

            if (match(token_type::NAMESPACE))
            {
                std::string n = chain_text(0, chain_len(0)); pos += chain_len(0);
                std::string full = resolve(namespace_names, n);
                if (full.empty()) throw std::runtime_error("Unknown namespace '" + n + "'!");
                usings.push_back(full);
                if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
                return std::make_unique<UsingNamespaceStatement>(full);
            }

            if (get(0).get_type() == token_type::WORDS)
            {
                std::string name = consume(token_type::WORDS).get_text();

                if (!loaded_libs.contains(name))
                {
                    auto manifest = exe_dir / "lib" / (name + ".manifest");
                    load_stdlib_manifest(manifest);
                    loaded_libs.insert(name);
                }

                if (!match(token_type::SEMI))
                    throw std::runtime_error("You miss the ;");

                return std::make_unique<UseStatement>(name);
            }
            else if (get(0).get_type() == token_type::TEXT)
            {
                std::filesystem::path rel = consume(token_type::TEXT).get_text();

                std::filesystem::path found;
                for (auto& dir : {base_dir, exe_dir})
                {
                    auto p = dir / rel;
                    if (std::filesystem::exists(p)) { found = p; break; }
                }
                if (found.empty())
                    throw std::runtime_error("Module '" + rel.string() + "' not found (looked in '"
                                             + base_dir.string() + "' and '" + exe_dir.string() + "')!");

                std::string path = std::filesystem::canonical(found).string();

                if (!loaded_fab_modules.contains(path))
                {
                    auto module_statements = load_fab_module(path);
                    pending_imports.push_back(std::make_unique<FileScopeStatement>(path));
                    for (auto& s : module_statements)
                        pending_imports.push_back(std::move(s));
                    pending_imports.push_back(std::make_unique<FileScopeStatement>(file_id));
                }

                if (!match(token_type::SEMI))
                    throw std::runtime_error("You miss the ;");

                return std::make_unique<UseStatement>(path);
            }
        }
        case token_type::WRITE: {
            consume(token_type::WRITE);
            std::unique_ptr<Expression> expr = expression();
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<WriteStatement>(std::move(expr));
        }
        case token_type::WRITELN: {
            consume(token_type::WRITELN);
            std::unique_ptr<Expression> expr;
            if (get(0).get_type() != token_type::SEMI)
                expr = expression();
            else
                expr = std::make_unique<ValueExpression>("");
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<WritelnStatement>(std::move(expr));
        }
        case token_type::INPUT_IN: {
            consume(token_type::INPUT_IN);
            std::string name = consume(token_type::WORDS).get_text();
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<InputInStatement>(name);
        }
        case token_type::IF: {
            consume(token_type::IF);
            return if_else();
        }
        case token_type::WHILE: {
            consume(token_type::WHILE);
            return while_statement();
        }
        case token_type::DO: {
            consume(token_type::DO);
            return do_while_statement();
        }
        case token_type::FOR: {
            consume(token_type::FOR);
            return for_statement();
        }
        case token_type::BREAK: {
            consume(token_type::BREAK);
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<BreakStatement>();
        }
        case token_type::CONTINUE: {
            consume(token_type::CONTINUE);
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<ContinueStatement>();
        }
        case token_type::RETURN: {
            consume(token_type::RETURN);
            std::unique_ptr<Expression> expr = nullptr;
            if (get(0).get_type() != token_type::SEMI)
                expr = expression();
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<ReturnStatement>(std::move(expr));
        }
        case token_type::DEFINE: {
            consume(token_type::DEFINE);
            return define_function();
        }
        case token_type::CLASS: {
            consume(token_type::CLASS);
            return class_definition();
        }
        case token_type::DELETE_KW: {
            consume(token_type::DELETE_KW);
            std::string name = consume(token_type::WORDS).get_text();
            if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
            return std::make_unique<DeleteStatement>(name);
        }

        case token_type::INC:
        case token_type::DEC: {
            auto e = expression();
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<FunctionStatement>(std::move(e));
        }

        case token_type::WORDS: {
            int len = chain_len(0);
            if (get(len).get_type() == token_type::INC || get(len).get_type() == token_type::DEC)
            {
                auto e = expression();
                if (!match(token_type::SEMI))
                    throw std::runtime_error("You miss the ;");
                return std::make_unique<FunctionStatement>(std::move(e));
            }
            if (at_decl()) return assigment_statement();

            if ((class_names.contains(get(0).get_text()) || is_lib_type(get(0).get_text()))
                    && get(1).get_type() == token_type::WORDS)
                return assigment_statement();

            if (class_names.contains(get(0).get_text()) && get(1).get_type() == token_type::WORDS)
                return assigment_statement();

            if (get(0).get_type() == token_type::WORDS && get(0).get_text() == "super"
                        && get(1).get_type() == token_type::LPARENT)
            {
                consume(token_type::WORDS); consume(token_type::LPARENT);
                auto s = std::make_unique<SuperCallStatement>();
                while (!match(token_type::RPARENT)) { s->add_arg(expression()); match(token_type::COMMA); }
                if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
                return s;
            }
            if (get(0).get_text() == "asm" && get(1).get_type() == token_type::LPARENT)
            {
                consume(token_type::WORDS); consume(token_type::LPARENT);
                std::string code = consume(token_type::TEXT).get_text();
                std::string cons;
                if (match(token_type::COMMA)) cons = consume(token_type::TEXT).get_text();

                auto s = std::make_unique<AsmStatement>(code, cons);
                while (match(token_type::COMMA)) s->add_arg(expression());

                consume(token_type::RPARENT);
                if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
                return s;
            }

            auto t1 = get(len).get_type();
            if (t1 == token_type::LPARENT || t1 == token_type::DOT || t1 == token_type::LSQUARE)
            {
                auto e = expression();
                if (match(token_type::EQ))
                {
                    auto v = expression();
                    if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
                    if (dynamic_cast<FieldExpression*>(e.get()))
                        return std::make_unique<FieldAssignStatement>(std::move(e), std::move(v));
                    return std::make_unique<IndexAssignStatement>(std::move(e), std::move(v));
                }
                if (!match(token_type::SEMI)) throw std::runtime_error("You miss the ;");
                return std::make_unique<FunctionStatement>(std::move(e));
            }
        }
        case token_type::TYPES: {
            return assigment_statement();
        }
        case token_type::CONST: {
            return assigment_statement();
        }
        default: {
            throw std::runtime_error("Unexpected token: " + tokens_string[get(0).get_type()]);
        }
    }
}

std::string Parser::parse_type()
{
    if (get(0).get_type() == token_type::WORDS)
    {
        int len = chain_len(0);
        std::string text = chain_text(0, len);
        std::string cls = resolve_class(text);
        if (!cls.empty())                    { pos += len; return cls; }
        if (len == 1 && is_lib_type(text))   { pos++;      return text; }
    }

    std::string t = consume(token_type::TYPES).get_text();
    if (t == "long" && get(0).get_type() == token_type::TYPES && get(0).get_text() == "double")
    {
        consume(token_type::TYPES);
        t = "ldouble";
    }
    if (t == "list")
    {
        consume(token_type::LT);
        t += "<" + parse_type() + ">";
        consume(token_type::GT);
    }
    else if (t == "dict")
    {
        consume(token_type::LT);
        std::string k = parse_type();
        consume(token_type::COMMA);
        std::string v = parse_type();
        consume(token_type::GT);

        t = "dict<" + k + "," + v + ">";
    }
    return t;
}

std::unique_ptr<Statement> Parser::declarators(const std::string& type, bool is_const,
                                               bool class_decl, bool no_semi)
{
    std::vector<std::unique_ptr<AssigementStatement>> decls;

    do
    {
        std::string name = consume(token_type::WORDS).get_text();
        std::unique_ptr<Expression> expr;

        if (is_const)
        {
            consume(token_type::EQ);
            expr = expression();
        }
        else if (match(token_type::EQ))
            expr = expression();
        else if (!class_decl)
            expr = std::make_unique<ValueExpression>(NullTag{});

        decls.push_back(std::make_unique<AssigementStatement>(type, name, std::move(expr), is_const));
    }
    while (match(token_type::COMMA));

    if (!match(token_type::SEMI) && !no_semi)
        throw std::runtime_error("You miss the ;");

    if (decls.size() == 1)
        return std::move(decls[0]);
    return std::make_unique<DeclListStatement>(std::move(decls));
}

std::unique_ptr<Statement> Parser::assigment_statement(bool no_semi)
{
    // type name = 33; or type name;
    Token current = get(0);

    bool decl = at_decl();

    if (current.get_type() == token_type::TYPES || decl)
    {
        std::string type = parse_type();
        return declarators(type, false, decl && !is_lib_type(type), no_semi);
    }
    else if (current.get_type() == token_type::WORDS)
    {
        int len = chain_len(0);
        std::string name = chain_text(0, len);
        token_type nxt = get(len).get_type();

        if (nxt == token_type::EQ)
        {
            pos += len + 1;
            std::unique_ptr<Expression> expr = expression();
            if (!match(token_type::SEMI) && !no_semi)
                throw std::runtime_error("You miss the ;");
            return std::make_unique<AssigementStatement>("", name, std::move(expr));
        }

        static const std::unordered_map<token_type, char> compoundOps = {
            {token_type::PLUSEQ, '+'}, {token_type::MINUSEQ, '-'},
            {token_type::MULTEQ, '*'}, {token_type::DIVEQ, '/'},
            {token_type::MODEQ, '%'},
        };

        auto it = compoundOps.find(nxt);
        if (it != compoundOps.end())
        {
            pos += len + 1;
            std::unique_ptr<Expression> right = expression();
            auto binExpr = std::make_unique<BinExpression>(it->second,
                std::make_unique<VariableExpression>(name), std::move(right));
            if (!match(token_type::SEMI) && !no_semi)
                throw std::runtime_error("You miss the ;");
            return std::make_unique<AssigementStatement>("", name, std::move(binExpr));
        }
    }
    else if (current.get_type() == token_type::CONST && get(1).get_type() == token_type::TYPES)
    {
        consume(token_type::CONST);
        std::string type = parse_type();
        std::string name = consume(token_type::WORDS).get_text();
        consume(token_type::EQ);
        std::unique_ptr<Expression> expr = expression();

        if (!match(token_type::SEMI) && !no_semi)
            throw std::runtime_error("You miss the ;");

        return std::make_unique<AssigementStatement>(type, name, std::move(expr), true);
    }

    throw std::runtime_error("Variable does have name or type!");
}

std::unique_ptr<Statement> Parser::if_else()
{
    std::unique_ptr<Expression> condition = expression();
    std::unique_ptr<Statement> if_statement = statement_or_block();
    std::unique_ptr<Statement> else_statement = nullptr;

    if (match(token_type::ELSE))
        else_statement = statement_or_block();

    return std::make_unique<IfStatement>(std::move(condition), std::move(if_statement), std::move(else_statement));
}

std::unique_ptr<Statement> Parser::while_statement()
{
    consume(token_type::LPARENT);
    std::unique_ptr<Expression> expr = expression();
    consume(token_type::RPARENT);
    std::unique_ptr<Statement> while_statement = statement_or_block();

    return std::make_unique<WhileStatement>(std::move(expr), std::move(while_statement));
}

std::unique_ptr<Statement> Parser::do_while_statement()
{
    std::unique_ptr<Statement> while_statement = statement_or_block();
    consume(token_type::WHILE);
    consume(token_type::LPARENT);
    std::unique_ptr<Expression> expr = expression();
    consume(token_type::RPARENT);
    consume(token_type::SEMI);

    return std::make_unique<DoWhileStatement>(std::move(expr), std::move(while_statement));
}

std::unique_ptr<Statement> Parser::for_statement()
{
    consume(token_type::LPARENT);

    std::unique_ptr<Statement> init;
    if (!match(token_type::SEMI))
        init = assigment_statement();

    std::unique_ptr<Expression> cond;
    if (get(0).get_type() != token_type::SEMI)
        cond = expression();
    consume(token_type::SEMI);

    std::unique_ptr<Statement> increment;
    if (get(0).get_type() != token_type::RPARENT)
    {
        auto t0  = get(0).get_type();
        int  len = chain_len(0);
        auto t1  = get(len ? len : 1).get_type();

        if (t0 == token_type::INC || t0 == token_type::DEC ||
            (t0 == token_type::WORDS && (t1 == token_type::INC || t1 == token_type::DEC)))
        {
            increment = std::make_unique<FunctionStatement>(expression());
        }
        else if (t0 == token_type::WORDS &&
                 (t1 == token_type::LSQUARE || t1 == token_type::DOT || t1 == token_type::LPARENT))
        {
            auto e = expression();
            if (match(token_type::EQ))
            {
                auto v = expression();
                if (dynamic_cast<FieldExpression*>(e.get()))
                    increment = std::make_unique<FieldAssignStatement>(std::move(e), std::move(v));
                else
                    increment = std::make_unique<IndexAssignStatement>(std::move(e), std::move(v));
            }
            else
                increment = std::make_unique<FunctionStatement>(std::move(e));
        }
        else
            increment = assigment_statement(true);
    }
    consume(token_type::RPARENT);

    std::unique_ptr<Statement> body = statement_or_block();

    return std::make_unique<ForStatement>(std::move(init), std::move(cond),
                                          std::move(increment), std::move(body));
}

std::unique_ptr<Statement> Parser::class_definition()
{
    std::string name = consume(token_type::WORDS).get_text();
    std::string full = qualify(name);
    std::string parent;

    if (match(token_type::COLON))
    {
        int len = chain_len(0);
        if (!len) throw std::runtime_error("Parent class name expected!");

        std::string text = chain_text(0, len);
        parent = resolve_class(text);
        if (parent.empty())
            throw std::runtime_error("Unknown parent class '" + text + "'!");
        pos += len;
    }

    consume(token_type::LBRACKET);
    auto cls = std::make_unique<ClassDefineStatement>(full, parent);
    bool is_public = false;

    while (!match(token_type::RBRACKET))
    {
        if (match(token_type::PRIVATE)) { consume(token_type::COLON); is_public = false; continue; }
        if (match(token_type::PUBLIC))  { consume(token_type::COLON); is_public = true;  continue; }

        bool st = match(token_type::STATIC);
        if (match(token_type::DEFINE))
        {
            auto fn = define_function(false);
            MethodDecl d{fn->get_type(), fn->get_name(), fn->get_arg_types(),
                         fn->get_arg_names(), fn->get_body(), is_public};
            d.is_static = st;
            d.is_static = st;
            d.defaults = fn->get_defaults();
            cls->add_method(std::move(d));
            continue;
        }
        if (st) throw std::runtime_error("'static' must be followed by 'define'!");

        if (match(token_type::TILDE))
        {
            if (consume(token_type::WORDS).get_text() != name)
                throw std::runtime_error("Destructor name must match class name!");
            consume(token_type::LPARENT);
            consume(token_type::RPARENT);
            cls->set_dtor(block());
            continue;
        }

        if (get(0).get_type() == token_type::WORDS && get(0).get_text() == name
            && get(1).get_type() == token_type::LPARENT)
        {
            consume(token_type::WORDS);
            consume(token_type::LPARENT);
            std::vector<std::string> at, an;
            std::vector<std::shared_ptr<Expression>> ad;
            while (!match(token_type::RPARENT))
            {
                at.push_back(parse_type());
                an.push_back(consume(token_type::WORDS).get_text());
                if (match(token_type::EQ)) ad.push_back(expression());
                else                       ad.push_back(nullptr);
                match(token_type::COMMA);
            }

            std::vector<std::shared_ptr<Expression>> base_args;
            if (match(token_type::COLON))
            {
                int blen = chain_len(0);
                if (!blen) throw std::runtime_error("Base class name expected!");

                std::string btext = chain_text(0, blen);
                if (parent.empty() || resolve_class(btext) != parent)
                    throw std::runtime_error("'" + btext + "' is not the parent of '" + name + "'!");
                pos += blen;

                consume(token_type::LPARENT);
                while (!match(token_type::RPARENT))
                {
                    base_args.push_back(expression());
                    match(token_type::COMMA);
                }
            }

            std::shared_ptr<Statement> body = block();
            MethodDecl d{"void", "new", at, an, body, true};
            d.defaults  = std::move(ad);
            d.base_args = std::move(base_args);
            cls->set_ctor(std::move(d));
            continue;
        }

        std::string type = parse_type();
        std::string fname = consume(token_type::WORDS).get_text();
        std::shared_ptr<Expression> init;
        if (match(token_type::EQ))
            init = expression();
        consume(token_type::SEMI);
        cls->add_field({fname, type, is_public, "", init});
    }
    return cls;
}

std::unique_ptr<Statement> Parser::statement_or_block()
{
    if (get(0).get_type() == token_type::LBRACKET)
        return block();
    return statement();
}

std::unique_ptr<Statement> Parser::block()
{
    std::vector<std::unique_ptr<Statement>> statements;
    consume(token_type::LBRACKET);

    while (!match(token_type::RBRACKET))
    {
        if (get(0).get_type() == token_type::eof) fail("Missing '}'");
        int start = pos;
        try { statements.push_back(statement()); }
        catch (const FabError& e)       { g_diags.push_back(e); synchronize(); }
        catch (const std::runtime_error& e) { g_diags.push_back(FabError(e.what(), here())); synchronize(); }
        if (pos == start) pos++;
    }

    return std::make_unique<BlockStatement>(std::move(statements));
}

std::unique_ptr<FunctionDefineStatement> Parser::define_function(bool qualified)
{
    std::string type = "void";
    std::string name = consume(token_type::WORDS).get_text();
    if (qualified) name = qualify(name);
    consume(token_type::LPARENT);

    std::vector<std::string> arg_type;
    std::vector<std::string> arg_name;

    std::vector<std::shared_ptr<Expression>> arg_def;
    bool seen_def = false;

    while (!match(token_type::RPARENT))
    {
        arg_type.push_back(parse_type());
        arg_name.push_back(consume(token_type::WORDS).get_text());

        if (match(token_type::EQ))
            arg_def.push_back(expression());
        else
            arg_def.push_back(nullptr);
        match(token_type::COMMA);
    }

    if (match(token_type::ARROW))
        type = parse_type();

    std::unique_ptr<Statement> body = statement_or_block();

    auto fn = std::make_unique<FunctionDefineStatement>(type, name, arg_type, arg_name, std::move(body));
    fn->set_defaults(std::move(arg_def));
    return fn;
}

std::unique_ptr<Expression> Parser::function()
{
    auto basic_string = consume(token_type::WORDS).get_text();
    std::string name = basic_string;
    consume(token_type::LPARENT);

    std::unique_ptr<FunctionalExpression> function = std::make_unique<FunctionalExpression>(name);

    while (!match(token_type::RPARENT))
    {
        function->add_arg(expression());
        match(token_type::COMMA);
    }

    return function;
}

std::unique_ptr<Expression> Parser::expression()
{
    return logic_or();
}

std::unique_ptr<Expression> Parser::logic_or()
{
    std::unique_ptr<Expression> expr = logic_and();

    while (true)
    {
        if (match(token_type::OR))
        {
            expr = std::make_unique<ConditionalExpression>("||", std::move(expr), logic_and());
            continue;
        }
        break;
    }

    return expr;
}

std::unique_ptr<Expression> Parser::logic_and()
{
    std::unique_ptr<Expression> expr = equality();

    while (true)
    {
        if (match(token_type::AND))
        {
            expr = std::make_unique<ConditionalExpression>("&&", std::move(expr), equality());
            continue;
        }
        break;
    }

    return expr;
}

std::unique_ptr<Expression> Parser::equality()
{
    std::unique_ptr<Expression> expr = conditional();

    while (true)
    {
        if (match(token_type::CEQ))
        {
            expr = std::make_unique<ConditionalExpression>("==", std::move(expr), conditional());
            continue;
        }
        if (match(token_type::NEQ))
        {
            expr = std::make_unique<ConditionalExpression>("!=", std::move(expr), conditional());
            continue;
        }
        break;
    }

    return expr;
}

std::unique_ptr<Expression> Parser::conditional()
{
    std::unique_ptr<Expression> expr = additive();



    while (true)
    {
        if (match(token_type::GTEQ))
        {
            expr = std::make_unique<ConditionalExpression>(">=", std::move(expr), additive());
            continue;
        }
        if (match(token_type::LTEQ))
        {
            expr = std::make_unique<ConditionalExpression>("<=", std::move(expr), additive());
            continue;
        }
        if (match(token_type::GT))
        {
            expr = std::make_unique<ConditionalExpression>(">", std::move(expr), additive());
            continue;
        }
        if (match(token_type::LT))
        {
            expr = std::make_unique<ConditionalExpression>("<", std::move(expr), additive());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::additive()
{

    std::unique_ptr<Expression> expr = multiply();

    while (true)
    {
        if (match(token_type::PLUS))
        {
            expr = std::make_unique<BinExpression>('+', std::move(expr), multiply());
            continue;
        }
        if (match(token_type::MINUS))
        {
            expr = std::make_unique<BinExpression>('-', std::move(expr), multiply());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::multiply()
{
    std::unique_ptr<Expression> expr = unary();

    while (true)
    {
        if (match(token_type::MULT))
        {
            expr = std::make_unique<BinExpression>('*', std::move(expr), unary());
            continue;
        }
        if (match(token_type::DIV))
        {
            expr = std::make_unique<BinExpression>('/', std::move(expr), unary());
            continue;
        }
        if (match(token_type::MOD))
        {
            expr = std::make_unique<BinExpression>('%', std::move(expr), unary());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::unary()
{
    if (match(token_type::INC))
        return std::make_unique<IncDecExpression>(unary(), true, true);
    if (match(token_type::DEC))
        return std::make_unique<IncDecExpression>(unary(), false, true);

    if (match(token_type::NOT))
        return std::make_unique<UnaryExpression>('!', std::move(unary()));
    if (match(token_type::MINUS))
        return std::make_unique<UnaryExpression>('-', std::move(unary()));
    if (match(token_type::PLUS))
        return std::make_unique<UnaryExpression>('+', std::move(unary()));

    return postfix();
}

std::unique_ptr<Expression> Parser::postfix()
{
    auto expr = primary();

    while (true)
    {
        if (match(token_type::DOT))
        {
            std::string name = consume(token_type::WORDS).get_text();
            if (match(token_type::LPARENT))
            {
                auto call = std::make_unique<MethodCallExpression>(name, std::move(expr));
                parse_call_args([&](std::string n, std::unique_ptr<Expression> e)
                                { call->add_arg(std::move(n), std::move(e)); });
                expr = std::move(call);
            }
            else
                expr = std::make_unique<FieldExpression>(std::move(expr), name);
        }
        else if (match(token_type::LSQUARE))
        {
            auto idx = expression();
            consume(token_type::RSQUARE);
            expr = std::make_unique<IndexExpression>(std::move(expr), std::move(idx));
        }
        else if (match(token_type::INC)) {
            expr = std::make_unique<IncDecExpression>(std::move(expr), true, false);
        }
        else if (match(token_type::DEC)) {
            expr = std::make_unique<IncDecExpression>(std::move(expr), false, false);
        }
        else break;
    }
    return expr;
}

std::unique_ptr<Expression> Parser::primary()
{
    SrcLoc l = here();
    auto e = primary_impl();
    span_to_prev(l, tokens, pos);
    e->loc = l;
    return e;
}

std::unique_ptr<Expression> Parser::primary_impl()
{
    Token current = get(0);

    if (current.get_type() == token_type::WORDS)
    {
        int len = chain_len(0);
        std::string text = chain_text(0, len);
        std::string cls = resolve_class(text);

        if ((!cls.empty() || static_classes.contains(text))
            && get(len).get_type() == token_type::DOT
            && is_member_name(get(len + 1).get_type())
            && get(len + 2).get_type() == token_type::LPARENT)
        {
            pos += len + 1;
            std::string name = get(0).get_text(); pos++;
            consume(token_type::LPARENT);
            auto c = std::make_unique<StaticCallExpression>(cls.empty() ? text : cls, name);
            parse_call_args([&](std::string n, std::unique_ptr<Expression> e)
                            { c->add_arg(std::move(n), std::move(e)); });
            return c;
        }

        if (get(len).get_type() == token_type::LPARENT)
        {
            if (len == 1 && text == "size_of")
            {
                consume(token_type::WORDS); consume(token_type::LPARENT);

                bool is_type = get(0).get_type() == token_type::TYPES
                            || (get(0).get_type() == token_type::WORDS
                                && !resolve_class(chain_text(0, chain_len(0))).empty());

                std::unique_ptr<Expression> r = is_type
                    ? std::make_unique<SizeOfExpression>(parse_type())
                    : std::make_unique<SizeOfExpression>(expression());

                consume(token_type::RPARENT);
                return r;
            }
            if (len == 1 && text == "type")
            {
                consume(token_type::WORDS); consume(token_type::LPARENT);
                auto e = expression();
                consume(token_type::RPARENT);
                return std::make_unique<TypeOfExpression>(std::move(e));
            }

            pos += len;
            consume(token_type::LPARENT);
            auto f = std::make_unique<FunctionalExpression>(text);
            while (!match(token_type::RPARENT))
            {
                if (get(0).get_type() == token_type::WORDS && get(1).get_type() == token_type::EQ)
                {
                    std::string an = get(0).get_text();
                    pos += 2;
                    f->add_arg(an, expression());
                }
                else
                {
                    if (f->has_named())
                        throw std::runtime_error("Positional argument after named argument!");
                    f->add_arg(expression());
                }
                match(token_type::COMMA);
            }
            return f;
        }

        pos += len;
        return std::make_unique<VariableExpression>(text);
    }

    if (match(token_type::NUMBER))
    {
        std::string t = current.get_text();
        std::string suffix;
        if (isalpha((unsigned char)t.back())) { suffix = t.back(); t.pop_back(); }

        bool has_frac = t.find_first_of(".e") != std::string::npos;

        std::string type;
        if      (suffix == "f") type = "float";
        else if (suffix == "d") type = "double";
        else if (suffix == "s") type = "short";
        else if (suffix == "L") type = has_frac ? "ldouble" : "long";

        if (!has_frac && type.empty())
        {
            try { return std::make_unique<ValueExpression>(std::stoll(t)); }
            catch (...) {}
        }
        return std::make_unique<ValueExpression>(std::stold(t), t, type);
    }
    if (match(token_type::LBRACKET))
    {
        auto dict = std::make_unique<DictLiteralExpression>();
        while (!match(token_type::RBRACKET))
        {
            auto key = expression();
            consume(token_type::COLON);
            auto val = expression();
            dict->add_pair(std::move(key), std::move(val));
            match(token_type::COMMA);
        }
        return dict;
    }
    if (match(token_type::LSQUARE))
    {
        auto list = std::make_unique<ListLiteralExpression>();
        while (!match(token_type::RSQUARE))
        {
            list->add_item(expression());
            match(token_type::COMMA);
        }
        return list;
    }
    if (match(token_type::NEW))
    {
        int len = chain_len(0);
        if (!len) throw std::runtime_error("Class name expected after 'new'!");
        std::string text = chain_text(0, len);
        std::string cls = resolve_class(text);
        pos += len;

        consume(token_type::LPARENT);
        auto n = std::make_unique<NewExpression>(cls.empty() ? text : cls);
        parse_call_args([&](std::string nm, std::unique_ptr<Expression> e)
                        { n->add_arg(std::move(nm), std::move(e)); });
        return n;
    }
    if (match(token_type::NULLVAL))
        return std::make_unique<ValueExpression>(NullTag{});
    if (match(token_type::TRUEVAL))
        return std::make_unique<ValueExpression>(BoolTag{true});
    if (match(token_type::FALSEVAL))
        return std::make_unique<ValueExpression>(BoolTag{false});
    if (match(token_type::HEX_NUMBER))
        return std::make_unique<ValueExpression>(std::stoll(current.get_text(), nullptr, 16));
    if (match(token_type::CHARS))
        return std::make_unique<ValueExpression>(current.get_text()[0]);
    if (match(token_type::FSTRING))
        return fstring(current.get_text());
    if (match(token_type::TEXT))
        return std::make_unique<ValueExpression>(current.get_text());
    if (match(token_type::LPARENT))
    {
        std::unique_ptr<Expression> result = expression();
        match(token_type::RPARENT);
        return result;
    }

    throw std::runtime_error("Unknown expression! " + current.get_text());
}

std::unique_ptr<Expression> Parser::fstring(const std::string& raw)
{
    std::vector<std::unique_ptr<Expression>> parts;
    std::string lit;

    auto flush = [&]
    {
        if (lit.empty()) return;
        parts.push_back(std::make_unique<ValueExpression>(lit));
        lit.clear();
    };

    for (size_t i = 0; i < raw.size(); ++i)
    {
        char c = raw[i];

        if (c == '{' && i + 1 < raw.size() && raw[i + 1] == '{') { lit.push_back('{'); ++i; }
        else if (c == '}' && i + 1 < raw.size() && raw[i + 1] == '}') { lit.push_back('}'); ++i; }
        else if (c == '}')
            throw std::runtime_error("Single '}' in f-string: write '}}'!");
        else if (c == '{')
        {
            int depth = 1;
            bool in_str = false;
            size_t j = i + 1;
            for (; j < raw.size() && depth > 0; ++j)
            {
                if (raw[j] == '"') in_str = !in_str;
                else if (!in_str && raw[j] == '{') depth++;
                else if (!in_str && raw[j] == '}') depth--;
            }
            std::string code = raw.substr(i + 1, j - i - 2);
            if (code.find_first_not_of(" \t") == std::string::npos)
                throw std::runtime_error("Empty expression in f-string!");

            flush();

            auto toks = Lexer(code).tokenize();
            Parser sub(toks, base_dir, exe_dir);
            sub.file_id = file_id;
            auto e = sub.expression();
            if (sub.pos < sub.size)
                throw std::runtime_error("Unexpected token in f-string expression: " + code);

            parts.push_back(std::make_unique<ToStrExpression>(std::move(e)));
            i = j - 1;      // '}'
        }
        else lit.push_back(c);
    }
    flush();

    if (parts.empty())
        return std::make_unique<ValueExpression>(std::string());

    if (parts.size() == 1) return std::move(parts[0]);
    auto cc = std::make_unique<ConcatExpression>();
    for (auto& p : parts) cc->add(std::move(p));
    return cc;
}

Token Parser::get(int relative_position)
{
    int position = pos + relative_position;
    if (position >= size)
        return eof;

    return tokens[position];
}

bool Parser::match(token_type type)
{
    Token t = get(0);

    if (type != t.get_type())
        return false;

    pos++;
    return true;
}

Token Parser::consume(token_type type)
{
    Token t = get(0);

    if (type != t.get_type())
        throw std::runtime_error("Token " + tokens_string[t.get_type()] + " does not match " + tokens_string[type]);

    pos++;
    return t;
}

bool is_known_type(std::string& type)
{
    if (type == "string")  return true;
    if (type == "int")     return true;
    if (type == "short")   return true;
    if (type == "byte")    return true;
    if (type == "long")    return true;
    if (type == "float")   return true;
    if (type == "double")  return true;
    if (type == "bool")    return true;
    if (type == "char")    return true;
    if (type == "list")    return true;
    if (type == "dict")    return true;
    if (type == "ldouble") return true;
    if (is_lib_type(type)) return true;

    return false;
}

static size_t positional_min(const std::vector<std::string>& defs)
{
    size_t n = 0;
    for (size_t i = 0; i < defs.size(); ++i) if (defs[i].empty()) n = i + 1;
    return n;
}

void Parser::load_stdlib_manifest(const std::filesystem::path& path)
{
    std::ifstream file(path);
    if (!file.is_open())
        throw std::runtime_error("Library '" + path.string() + "' not found!");

    std::string line;

    while (std::getline(file, line))
    {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream iss(line);

        std::string head;
        iss >> head;

        if (head == "getter")
        {
            std::string name, symbol, type;
            iss >> name >> symbol >> type;
            if (name.empty() || symbol.empty() || type.empty())
                throw std::runtime_error("Manifest: bad getter line");
            if (stdlib_vars.contains(name))
                throw std::runtime_error("Manifest: duplicate variable " + name);

            StdlibVarInfo v{symbol, type, "", false};
            v.is_getter = true;
            stdlib_vars[name] = v;
            continue;
        }

        if (head == "const" || head == "var")
        {
            bool is_const = (head == "const");
            std::string name;
            iss >> name;

            if (stdlib_vars.contains(name))
                throw std::runtime_error("Manifest: duplicate variable " + name);

            if (is_const)
            {
                std::string type, value;
                iss >> type;
                std::getline(iss, value);

                auto b = value.find_first_not_of(" \t");
                auto e = value.find_last_not_of(" \t\r");
                if (b == std::string::npos)
                    throw std::runtime_error("Manifest: const " + name + " has no value");
                value = value.substr(b, e - b + 1);

                stdlib_vars[name] = {"", type, value, true};
            }
            else
            {
                std::string symbol, type;
                iss >> symbol >> type;
                stdlib_vars[name] = {symbol, type, "", false};
            }
            continue;
        }
        if (head == "class")
        {
            LibTypeInfo t;
            iss >> t.name;
            if (t.name.empty())
                throw std::runtime_error("Manifest: class has no name");

            std::string tok;
            while (iss >> tok)
            {
                if (tok == ":")         iss >> t.parent;
                else if (tok == "free") iss >> t.free_symbol;
                else throw std::runtime_error("Manifest: unexpected '" + tok + "' in class " + t.name);
            }

            if (lib_types.contains(t.name) || class_names.contains(t.name))
                throw std::runtime_error("Manifest: type " + t.name + " already exists");
            if (!t.parent.empty() && !lib_types.contains(t.parent))
                throw std::runtime_error("Manifest: unknown parent '" + t.parent + "' of " + t.name);

            lib_types[t.name] = t;
            continue;
        }

        iss.clear();
        iss.seekg(0);

        std::string fab_name, symbol, ret, colon, arg;
        iss >> fab_name;
        bool is_static = false;
        if (fab_name == "static") { is_static = true; iss >> fab_name; }
        iss >> symbol >> ret >> colon;

        std::vector<std::string> args, defs, nms;
        while (iss >> arg)
        {
            if (arg.back() == ',') arg.pop_back();
            std::string def, nm;
            if (auto eq = arg.find('='); eq != std::string::npos)
            {
                def = arg.substr(eq + 1);
                arg.resize(eq);
                if (def.empty()) throw std::runtime_error("Manifest: empty default in " + fab_name);
            }
            if (auto c = arg.find(':'); c != std::string::npos)
            {
                nm = arg.substr(c + 1);
                arg.resize(c);
            }
            args.push_back(arg); defs.push_back(def); nms.push_back(nm);
        }

        size_t first_def = positional_min(defs);

        if (is_static)
        {
            auto d = fab_name.find('.');
            if (d == std::string::npos)
                throw std::runtime_error("Manifest: static " + fab_name + " must look like Class.method");

            std::string cls = fab_name.substr(0, d), name = fab_name.substr(d + 1);
            if (class_names.contains(cls) || lib_types.contains(cls))
                throw std::runtime_error("Manifest: type " + cls + " already exists");

            static_classes.insert(cls);

            auto key = mangle_method(cls, name, args);
            if (stdlib_statics.contains(key))
                throw std::runtime_error("Manifest: duplicate static " + fab_name);

            StdlibFunctionInfo info{symbol, ret, args, defs, nms};
            stdlib_statics[key] = info;
            for (size_t n = first_def; n < args.size(); ++n)
                stdlib_statics.emplace(
                    mangle_method(cls, name, std::vector<std::string>(args.begin(), args.begin() + n)), info);
            continue;
        }

        auto dot = fab_name.find('.');
        if (dot == std::string::npos)
        {
            StdlibFunctionInfo info{symbol, ret, args, defs, nms};
            stdlib_symbols[mangle_name(fab_name, args)] = info;

            for (size_t n = first_def; n < args.size(); ++n)
                stdlib_symbols.emplace(
                    mangle_name(fab_name, std::vector<std::string>(args.begin(), args.begin() + n)), info);
            continue;
        }

        std::string type = fab_name.substr(0, dot);
        std::string name = fab_name.substr(dot + 1);

        std::string recv = type;
        if (type == "list<T>")   type = "list";
        if (type == "dict<K,V>") type = "dict";

        if (!is_known_type(type))
            throw std::runtime_error("Manifest: unknown receiver type '" + type + "' in " + fab_name);
        if (args.empty() || args[0] != recv)
            throw std::runtime_error("Manifest: first argument of " + fab_name + " must be '" + recv + "'");

        std::vector<std::string> tail(args.begin() + 1, args.end());
        auto key = mangle_method(type, name, tail);

        if (first_def < 1)
            throw std::runtime_error("Manifest: receiver of " + fab_name + " cannot have a default");

        if (stdlib_methods.contains(key))
            throw std::runtime_error("Manifest: duplicate method " + fab_name);

        StdlibFunctionInfo info{symbol, ret, args, defs, nms};
        stdlib_methods[key] = info;

        for (size_t n = first_def; n < args.size(); ++n)
            stdlib_methods.emplace(
                mangle_method(type, name, std::vector<std::string>(args.begin() + 1, args.begin() + n)), info);
    }
}

std::vector<std::unique_ptr<Statement>> Parser::load_fab_module(const std::filesystem::path& path)
{
    auto canonical = std::filesystem::canonical(path).string();

    if (loaded_fab_modules.contains(canonical))
        return {};

    loaded_fab_modules.insert(canonical);

    std::ifstream file(path);
    std::stringstream buffer;
    buffer << file.rdbuf();

    std::string text = buffer.str();
    int fid = add_source(canonical, text);
    auto tokens = Lexer(text, fid).tokenize();
    Parser module_parser(tokens, path.parent_path(), exe_dir);
    module_parser.file_id = canonical;

    return module_parser.parse();
}

std::string Parser::cur_ns() const
{
    std::string r;
    for (auto& s : ns_stack) r += (r.empty() ? "" : "::") + s;
    return r;
}

std::string Parser::qualify(const std::string& n) const
{
    auto ns = cur_ns();
    return ns.empty() ? n : ns + "::" + n;
}

std::string Parser::resolve(const std::unordered_set<std::string>& known, const std::string& n) const
{
    for (size_t k = ns_stack.size(); k > 0; --k)
    {
        std::string q;
        for (size_t i = 0; i < k; ++i) q += ns_stack[i] + "::";
        q += n;
        if (known.contains(q)) return q;
    }
    for (auto& u : usings)
        if (known.contains(u + "::" + n)) return u + "::" + n;
    return known.contains(n) ? n : "";
}

int Parser::chain_len(int at)
{
    if (get(at).get_type() != token_type::WORDS) return 0;
    int n = 1;
    while (get(at + n).get_type() == token_type::SCOPE && get(at + n + 1).get_type() == token_type::WORDS)
        n += 2;
    return n;
}

std::string Parser::chain_text(int at, int len)
{
    std::string r;
    for (int i = 0; i < len; i += 2) r += (i ? "::" : "") + get(at + i).get_text();
    return r;
}

bool Parser::at_decl()
{
    int len = chain_len(0);
    if (!len) return false;
    std::string t = chain_text(0, len);
    return (!resolve_class(t).empty() || (len == 1 && is_lib_type(t)))
        && get(len).get_type() == token_type::WORDS;
}

void Parser::parse_top(std::vector<std::unique_ptr<Statement>>& out)
{
    int start = pos;
    try { parse_top_impl(out); }
    catch (const FabError& e)           { g_diags.push_back(e); synchronize(); }
    catch (const std::runtime_error& e) { g_diags.push_back(FabError(e.what(), here())); synchronize(); }
    if (pos == start) pos++;
}

void Parser::parse_top_impl(std::vector<std::unique_ptr<Statement>>& out)
{
    if (match(token_type::NAMESPACE)) { parse_namespace(out); return; }

    bool is_static = match(token_type::STATIC);
    auto stmt = statement();

    auto* decl = dynamic_cast<AssigementStatement*>(stmt.get());
    auto* list = dynamic_cast<DeclListStatement*>(stmt.get());
    if (decl && decl->is_declaration()) decl->make_global(is_static);
    else if (list) list->make_global(is_static);
    else if (is_static)
        throw std::runtime_error("'static' at top level must be followed by a variable declaration!");

    if (!pending_imports.empty())
    {
        out.push_back(std::make_unique<NamespaceScopeStatement>(""));
        for (auto& s : pending_imports) out.push_back(std::move(s));
        pending_imports.clear();
        out.push_back(std::make_unique<NamespaceScopeStatement>(cur_ns()));
    }
    if (!stmt) return;
    out.push_back(std::move(stmt));
}

void Parser::parse_namespace(std::vector<std::unique_ptr<Statement>>& out)
{
    std::vector<std::string> parts{consume(token_type::WORDS).get_text()};
    while (match(token_type::SCOPE)) parts.push_back(consume(token_type::WORDS).get_text());
    consume(token_type::LBRACKET);

    for (auto& p : parts)
    {
        if (p == "std") throw std::runtime_error("Namespace name 'std' is reserved!");
        ns_stack.push_back(p);
        namespace_names.insert(cur_ns());
    }
    out.push_back(std::make_unique<NamespaceScopeStatement>(cur_ns()));

    while (!match(token_type::RBRACKET)) parse_top(out);

    for (size_t i = 0; i < parts.size(); ++i) ns_stack.pop_back();
    out.push_back(std::make_unique<NamespaceScopeStatement>(cur_ns()));
}

void Parser::fail(const std::string& msg)
{
    throw FabError(msg, here());
}

void Parser::fail_prev(const std::string& msg)
{
    SrcLoc l = pos > 0 ? tokens[pos - 1].loc : here();
    l.col += l.len; l.len = 1;
    throw FabError(msg, l);
}

void Parser::synchronize()
{
    int depth = 0;
    while (get(0).get_type() != token_type::eof)
    {
        auto t = get(0).get_type();
        if (t == token_type::RBRACKET && depth == 0) return;
        pos++;
        if (t == token_type::LBRACKET) depth++;
        else if (t == token_type::RBRACKET) depth--;
        else if (t == token_type::SEMI && depth == 0) return;
    }
}

void Parser::parse_call_args(const std::function<void(std::string, std::unique_ptr<Expression>)>& add)
{
    bool named = false;
    while (!match(token_type::RPARENT))
    {
        std::string n;
        if (get(0).get_type() == token_type::WORDS && get(1).get_type() == token_type::EQ)
        {
            n = get(0).get_text();
            pos += 2;
            named = true;
        }
        else if (named)
            throw std::runtime_error("Positional argument after named argument!");

        add(n, expression());
        match(token_type::COMMA);
    }
}