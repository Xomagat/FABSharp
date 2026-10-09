//
// Created by Xomagat on 07.08.2026.
//

#include "Parser.h"

// vars

// funcs
Parser::Parser(std::vector<Token> tokens, std::filesystem::path base_dir, std::filesystem::path exe_dir)
{
    eof = Token(token_type::eof, "");

    this->tokens = tokens;

    size = tokens.size();

    this->base_dir = base_dir;
    this->exe_dir  = exe_dir;

    pos = 0;
}

std::vector<std::unique_ptr<Statement>> Parser::parse()
{
    std::vector<std::unique_ptr<Statement>> result;

    if (!loaded_libs.contains("@types"))
    {
        for (auto& e : std::filesystem::directory_iterator(exe_dir / "lib" / "types"))
            if (e.path().extension() == ".manifest")
                load_stdlib_manifest(e.path());
        loaded_libs.insert("@types");
    }

    while (!match(token_type::eof))
    {
        auto stmt = statement();

        for (auto& imported : pending_imports)
            result.push_back(std::move(imported));
        pending_imports.clear();

        result.push_back(std::move(stmt));
    }

    return result;
}

std::unique_ptr<Statement> Parser::statement()
{
    switch (get(0).get_type())
    {
        case token_type::USE: {
            consume(token_type::USE);
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
                std::string path = (exe_dir / consume(token_type::TEXT).get_text()).string();

                if (!loaded_fab_modules.contains(path))
                {
                    auto module_statements = load_fab_module(path);
                    for (auto& s : module_statements)
                        pending_imports.push_back(std::move(s));
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
            std::unique_ptr<Expression> expr = expression();
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
        case token_type::WORDS: {
            if (get(1).get_type() == token_type::LPARENT ||
                get(1).get_type() == token_type::DOT)
            {
                auto fn = std::make_unique<FunctionStatement>(expression());

                if (!match(token_type::SEMI))
                    throw std::runtime_error("You miss the ;");

                return fn;
            }

            return assigment_statement();
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
    std::string t = consume(token_type::TYPES).get_text();
    if (t == "list")
    {
        consume(token_type::LT);
        t += "<" + parse_type() + ">";
        consume(token_type::GT);
    }
    return t;
}

std::unique_ptr<Statement> Parser::assigment_statement(bool no_semi)
{
    // type name = 33; or type name;
    Token current = get(0);

    if (current.get_type() == token_type::TYPES)
    {
        std::string type = parse_type();
        std::string name = consume(token_type::WORDS).get_text();
        std::unique_ptr<Expression> expr;

        if (match(token_type::EQ))
            expr = expression();
        else
            expr = std::make_unique<ValueExpression>(NullTag{});

        if (!match(token_type::SEMI) && !no_semi)
            throw std::runtime_error("You miss the ;");

        return std::make_unique<AssigementStatement>(type, name, std::move(expr));
    }
    else if (current.get_type() == token_type::WORDS)
    {
        std::string name = current.get_text();

        if (get(1).get_type() == token_type::EQ)
        {
            consume(token_type::WORDS);
            consume(token_type::EQ);
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

        auto it = compoundOps.find(get(1).get_type());
        if (it != compoundOps.end())
        {
            consume(token_type::WORDS);
            pos++;
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
        std::string type = consume(token_type::TYPES).get_text();
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
    std::unique_ptr<Statement> init = assigment_statement();
    std::unique_ptr<Expression> expr = expression();
    consume(token_type::SEMI);
    std::unique_ptr<Statement> increment = assigment_statement(true);
    consume(token_type::RPARENT);
    std::unique_ptr<Statement> for_statement = statement_or_block();

    return std::make_unique<ForStatement>(std::move(init), std::move(expr), std::move(increment), std::move(for_statement));
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
        statements.push_back(statement());
    }

    return std::make_unique<BlockStatement>(std::move(statements));
}

std::unique_ptr<FunctionDefineStatement> Parser::define_function()
{
    std::string type = "void";
    std::string name = consume(token_type::WORDS).get_text();
    consume(token_type::LPARENT);

    std::vector<std::string> arg_type;
    std::vector<std::string> arg_name;

    while (!match(token_type::RPARENT))
    {
        arg_type.push_back(parse_type());
        arg_name.push_back(consume(token_type::WORDS).get_text());
        match(token_type::COMMA);
    }

    if (match(token_type::ARROW))
        type = parse_type();

    std::unique_ptr<Statement> body = statement_or_block();

    return std::make_unique<FunctionDefineStatement>(type, name, arg_type, arg_name, std::move(body));
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
            consume(token_type::LPARENT);

            auto call = std::make_unique<MethodCallExpression>(name, std::move(expr));
            while (!match(token_type::RPARENT))
            {
                call->add_arg(expression());
                match(token_type::COMMA);
            }
            expr = std::move(call);
        }
        else if (match(token_type::LSQUARE))
        {
            auto idx = expression();
            consume(token_type::RSQUARE);
            expr = std::make_unique<IndexExpression>(std::move(expr), std::move(idx));
        }
        else break;
    }
    return expr;
}

std::unique_ptr<Expression> Parser::primary()
{
    Token current = get(0);

    if (match(token_type::NUMBER))
    {
        if (current.get_text().find('.') == std::string::npos)
        {
            try
            {
                return std::make_unique<ValueExpression>(std::stoll(current.get_text()));
            }
            catch (...)
            {
                goto ld_convert;
            }
        }
        ld_convert:
        return std::make_unique<ValueExpression>(std::stold(current.get_text()));
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
    if (match(token_type::NULLVAL))
        return std::make_unique<ValueExpression>(NullTag{});
    if (match(token_type::TRUEVAL))
        return std::make_unique<ValueExpression>(BoolTag{true});
    if (match(token_type::FALSEVAL))
        return std::make_unique<ValueExpression>(BoolTag{false});
    if (match(token_type::HEX_NUMBER))
        return std::make_unique<ValueExpression>(std::stoll(current.get_text(), nullptr, 16));
    if (current.get_type() == token_type::WORDS && get(1).get_type() == token_type::LPARENT)
        return function();
    if (match(token_type::CHARS))
        return std::make_unique<ValueExpression>(current.get_text()[0]);
    if (match(token_type::TEXT))
        return std::make_unique<ValueExpression>(current.get_text());
    if (match(token_type::WORDS))
        return std::make_unique<VariableExpression>(current.get_text());
    if (match(token_type::LPARENT))
    {
        std::unique_ptr<Expression> result = expression();
        match(token_type::RPARENT);
        return result;
    }

    throw std::runtime_error("Unknown expression! " + current.get_text());
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
    if (type == "string") return true;
    if (type == "int")    return true;
    if (type == "short")  return true;
    if (type == "byte")   return true;
    if (type == "long")   return true;
    if (type == "float")  return true;
    if (type == "double") return true;
    if (type == "bool")   return true;
    if (type == "char")   return true;
    if (type == "list")   return true;

    return false;
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

        iss.clear();
        iss.seekg(0);

        std::string fab_name, symbol, ret, colon, arg;
        iss >> fab_name >> symbol >> ret >> colon;

        std::vector<std::string> args;
        while (iss >> arg)
        {
            if (arg.back() == ',') arg.pop_back();
            args.push_back(arg);
        }

        auto dot = fab_name.find('.');
        if (dot == std::string::npos)
        {
            stdlib_symbols[mangle_name(fab_name, args)] = {symbol, ret, args};
            continue;
        }

        std::string type = fab_name.substr(0, dot);
        std::string name = fab_name.substr(dot + 1);

        std::string recv = type;
        if (type == "list<T>") type = "list";

        if (!is_known_type(type))
            throw std::runtime_error("Manifest: unknown receiver type '" + type + "' in " + fab_name);
        if (args.empty() || args[0] != recv)
            throw std::runtime_error("Manifest: first argument of " + fab_name + " must be '" + recv + "'");

        std::vector<std::string> tail(args.begin() + 1, args.end());
        auto key = mangle_method(type, name, tail);

        if (stdlib_methods.contains(key))
            throw std::runtime_error("Manifest: duplicate method " + fab_name);

        stdlib_methods[key] = {symbol, ret, args};
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

    auto tokens = Lexer(buffer.str()).tokenize();
    Parser module_parser(tokens, path.parent_path(), exe_dir);;

    return module_parser.parse();
}