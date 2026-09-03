//
// Created by Xomagat on 07.08.2026.
//

#include "Parser.h"

// vars

// funcs
Parser::Parser(std::vector<Token> tokens)
{
    eof = Token(token_type::eof, "");

    this->tokens = tokens;

    size = tokens.size();

    pos = 0;
}

std::vector<std::unique_ptr<Statement>> Parser::parse()
{
    std::vector<std::unique_ptr<Statement>> result;

    while (!match(token_type::eof))
    {
        result.push_back(statement());
    }

    return result;
}

std::unique_ptr<Statement> Parser::statement()
{
    switch (get(0).get_type())
    {
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
            std::string name = consume(token_type::WORD).get_text();
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
            std::unique_ptr<Expression> expr = expression();
            if (!match(token_type::SEMI))
                throw std::runtime_error("You miss the ;");
            return std::make_unique<ReturnStatement>(std::move(expr));
        }
        case token_type::DEFINE: {
            consume(token_type::DEFINE);
            return define_function();
        }
        case token_type::WORD: {
            if (get(1).get_type() == token_type::LPARENT)
            {
                std::unique_ptr<FunctionStatement> fn = std::make_unique<FunctionStatement>(function());

                if (!match(token_type::SEMI))
                    throw std::runtime_error("You miss the ;");

                return fn;
            }

            return assigment_statement();
        }
        case token_type::TYPES: {
            return assigment_statement();
        }
        default: {
            throw std::runtime_error("Unexpected token: " + tokens_string[get(0).get_type()]);
        }
    }
}

std::unique_ptr<Statement> Parser::assigment_statement(bool no_semi)
{
    // type name = 33; or type name;
    Token current = get(0);

    if (current.get_type() == token_type::TYPES && get(1).get_type() == token_type::WORD)
    {
        std::string type = consume(token_type::TYPES).get_text();
        std::string name = consume(token_type::WORD).get_text();
        std::unique_ptr<Expression> expr;

        if (match(token_type::EQ))
            expr = expression();
        else
            expr = std::make_unique<ValueExpression>(std::make_unique<NullValue>());

        if (!match(token_type::SEMI) && !no_semi)
            throw std::runtime_error("You miss the ;");

        return std::make_unique<AssigementStatement>(type, name, std::move(expr));
    }
    else if (current.get_type() == token_type::WORD && get(1).get_type() == token_type::EQ)
    {
        std::string name = consume(token_type::WORD).get_text();
        consume(token_type::EQ);

        std::unique_ptr<Expression> expr = expression();
        if (!match(token_type::SEMI) && !no_semi)
            throw std::runtime_error("You miss the ;");

        return std::make_unique<AssigementStatement>("", name, std::move(expr));
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
    std::string name = consume(token_type::WORD).get_text();
    consume(token_type::LPARENT);

    std::vector<std::string> arg_type;
    std::vector<std::string> arg_name;

    Environment env;

    while (!match(token_type::RPARENT))
    {
        arg_type.push_back(consume(token_type::TYPES).get_text());
        arg_name.push_back(consume(token_type::WORD).get_text());
        match(token_type::COMMA);
    }
    std::unique_ptr<Statement> body = statement_or_block();

    return std::make_unique<FunctionDefineStatement>(name, arg_type, arg_name, std::move(body));
}

std::unique_ptr<Expression> Parser::function()
{
    std::string name = consume(token_type::WORD).get_text();
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
    std::unique_ptr<Expression> expr = pow();

    while (true)
    {
        if (match(token_type::MULT))
        {
            expr = std::make_unique<BinExpression>('*', std::move(expr), pow());
            continue;
        }
        if (match(token_type::DIV))
        {
            expr = std::make_unique<BinExpression>('/', std::move(expr), pow());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::pow()
{
    std::unique_ptr<Expression> expr = unary();

    while (true)
    {
        if (match(token_type::POW))
        {
            expr = std::make_unique<BinExpression>('^', std::move(expr), unary());
            continue;
        }
        break;
    }


    return expr;
}

std::unique_ptr<Expression> Parser::unary()
{
    if (match(token_type::MINUS))
        return std::make_unique<UnaryExpression>('-', std::move(primary()));
    if (match(token_type::PLUS))
        return std::make_unique<UnaryExpression>('+', std::move(primary()));

    return primary();
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
    if (match(token_type::HEX_NUMBER))
        return std::make_unique<ValueExpression>(static_cast<int>(std::stoll(current.get_text(), nullptr, 16)));
    if (current.get_type() == token_type::WORD && get(1).get_type() == token_type::LPARENT)
        return function();
    if (match(token_type::TEXT))
        return std::make_unique<ValueExpression>(current.get_text());
    if (match(token_type::WORD))
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