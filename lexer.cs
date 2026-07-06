using System.Text.RegularExpressions;
using System.Globalization;

public class Token
{
    public string Name { get; set; }
    public object Value { get; set; }
    public int Line { get; set; }
    public Token(string name, object value, int line = 0) { Name = name; Value = value; Line = line; }
}

class Lexer
{
    string[] keywords = ["write", "writeln", "int", "string", "float", "bool", "char",
                         "double", "long", "short", "byte", "input_in", "un",
                         "type", "size_of", "delete", "if", "else", "and", "or", "not",
                         "for", "while", "vfree", "def", "return", "true", "false", "use",
                         "namespace", "limited", "const", "only", "is", "in", "break", "continue",
                         "class", "public", "private", "new", "system", "null", "void", "static",
                         "tuple", "struct", "switch", "case", "default", "list", "dict",
                         "cast", "throw", "try", "catch", "finally", "native"];

    (string type, string pattern)[] token_specification = [
        ("COMMENT",  @"//[^\n]*"),
        ("BLOCK_COMMENT", @"/\*[\s\S]*?\*/"),
        ("FSTRING",  @"\$""(?:[^""\\]|\\.)*"""),
        ("VERBATIM", @"@""[^""]*"""),
        ("NUMBER",   @"\d+(\.\d+)?([eE][+-]?\d+)?"),
        ("ID",       @"[a-zA-Z_][a-zA-Z_0-9]*"),
        ("ARROW",    @"->"),
        ("FATARROW", @"=>"),
        ("DOT",      @"\."),
        ("OP",       @"\+\+|--|==|!=|<=|>=|:=|\+=|-=|\*=|/=|%=|\^=|[+\-*/%^=<>&]"),
        ("SKIP",     @"[ \t]+"),
        ("NEWLINE",  @"\n"),
        ("LPAREN",   @"\("),
        ("RPAREN",   @"\)"),
        ("LBRACKET", @"\["),
        ("RBRACKET", @"\]"),
        ("LBRACE",   @"\{"),
        ("RBRACE",   @"\}"),
        ("COMMA",    @","),
        ("COLON",    @":"),
        ("CHAR_LIT", @"'[^']'"),
        ("STRING",   @"""(?:[^""\\]|\\.)*"""),
        ("SEMI",     @";"),
        ("MISMATCH", @"."),
    ];

    public IEnumerable<Token> Lex(string code)
    {
        string tok_regex = "";
        foreach (var spec in token_specification)
        {
            if (tok_regex.Length > 0) tok_regex += "|";
            tok_regex += $"(?<{spec.type}>{spec.pattern})";
        }

        var regex = new Regex(tok_regex, RegexOptions.Compiled);

        int line = 1;

        foreach (Match match in regex.Matches(code))
        {
            string kind = null;
            foreach (var spec in token_specification)
                if (match.Groups[spec.type].Success) { kind = spec.type; break; }

            if (kind == null) continue;
            string value = match.Value;

            switch (kind)
            {
                case "COMMENT":
                case "BLOCK_COMMENT":
                case "SKIP":
                case "MISMATCH":
                    continue;

                case "NEWLINE":
                    line++;
                    continue;

                case "NUMBER":
                    if (value.Contains('.') || value.Contains('e') || value.Contains('E'))
                        yield return new Token(kind, double.Parse(value, CultureInfo.InvariantCulture), line);
                    else if (int.TryParse(value, out int iv))
                        yield return new Token(kind, iv, line);
                    else if (long.TryParse(value, out long lv))
                        yield return new Token(kind, lv, line);
                    else
                        throw new FabException(line, $"Number '{value}' is too large");
                    break;

                case "ID":
                    if (value == "true") yield return new Token("BOOL_LIT", true, line);
                    else if (value == "false") yield return new Token("BOOL_LIT", false, line);
                    else if (value == "null") yield return new Token("NULL_LIT", null, line);  // ← null literal
                    else yield return new Token(
                        keywords.Contains(value) ? value.ToUpper() : "ID",
                        value,
                        line
                    );
                    break;

                case "FSTRING":
                    string fraw = value.Substring(2, value.Length - 3);
                    string fprocessed = Regex.Replace(fraw, @"\\(?![{]).", m => m.Value switch
                    {
                        @"\n" => "\n",
                        @"\r" => "\r",
                        @"\t" => "\t",
                        @"\\" => "\\",
                        "\\\"" => "\"",
                        @"\0" => "\0",
                        @"\a" => "\a",
                        @"\b" => "\b",
                        @"\f" => "\f",
                        @"\v" => "\v",
                        _ => m.Value
                    });
                    yield return new Token("FSTRING", fprocessed, line);
                    break;

                case "CHAR_LIT":
                    yield return new Token(kind, value[1], line);
                    break;

                case "VERBATIM":
                    yield return new Token("STRING", value.Substring(2, value.Length - 3), line);
                    break;

                case "STRING":
                    string raw = value.Substring(1, value.Length - 2);
                    string processed = Regex.Replace(raw, @"\\.", m => m.Value switch
                    {
                        @"\n" => "\n",
                        @"\r" => "\r",
                        @"\t" => "\t",
                        @"\\" => "\\",
                        "\\\"" => "\"",
                        @"\0" => "\0",
                        @"\a" => "\a",
                        @"\b" => "\b",
                        @"\f" => "\f",
                        @"\v" => "\v",
                        _ => m.Value
                    });
                    yield return new Token("STRING", processed, line);
                    break;

                default:
                    yield return new Token(kind, value, line);
                    break;
            }
        }
    }
}