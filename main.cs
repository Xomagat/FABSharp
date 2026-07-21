// main script

using System.ComponentModel.Design;

namespace FAB_Compilator
{
    class Program
    {
        static int Main(string[] args)
        {
            string? fabFile = null;
            string? outputDir = null;
            bool doCompile = false;
            bool emitCpp = false;

            for (int i = 0; i < args.Length; i++)
            {
                switch (args[i])
                {
                    case "--help":
                    case "-h":
                        PrintHelp(); return 0;

                    case "--compile":
                    case "-c":
                        doCompile = true;
                        break;

                    case "--emit-cpp":
                        emitCpp = true;
                        doCompile = true;   // emit-cs implies compile pipeline up to codegen
                        break;

                    case "--output":
                    case "-o":
                        if (i + 1 < args.Length) outputDir = args[++i];
                        else { Console.Error.WriteLine("[error] --output requires a path"); return 1; }
                        break;

                    default:
                        if (!args[i].StartsWith("-") && args[i] != "fab")
                            fabFile = args[i];
                        break;
                }
            }

            // ── Resolve source file ───────────────────────────────────────────
            if (fabFile == null)
            {
                Console.Error.WriteLine("Usage: fab [--compile] [-o <dir>] <script.fab>");
                Console.Error.WriteLine("       fab --help");
                return 1;
            }

            if (!File.Exists(fabFile))
            {
                Console.Error.WriteLine($"[error] File not found: '{fabFile}'");
                return 1;
            }

            // ── Dispatch ──────────────────────────────────────────────────────
            if (doCompile)
                return CompileScript(fabFile, outputDir, emitCsOnly: emitCpp);

            ExecuteScript(fabFile);
            return 0;
        }

        // ── Compiler mode ─────────────────────────────────────────────────────

        static int CompileScript(string path, string? outputDir, bool emitCsOnly)
        {
            string code = File.ReadAllText(path);
            string fileName = Path.GetFileName(path);
            string baseName = Path.GetFileNameWithoutExtension(path);
            string srcDir = Path.GetDirectoryName(Path.GetFullPath(path)) ?? ".";
            string outDir = outputDir ?? srcDir;
            Directory.CreateDirectory(outDir);

            // ── Step 1: Lex + Parse ───────────────────────────────────────────
            List<Token> tokens;
            AST.FabProgram program;
            try
            {
                tokens = new Lexer().Lex(code).ToList();
                program = new Parser(tokens).Parse();
            }
            catch (FabException ex)
            {
                PrintRuntimeError(fileName, ex.Line, ex.Message, code.Replace("\t", ""));
                return 1;
            }
            catch (Exception ex)
            {
                PrintRuntimeError(fileName, 0, ex.Message, code.Replace("\t", ""));
                return 1;
            }

            // ── Step 2: Transpile → C++ ────────────────────────────────────────
            // Загрузить fab_runtime.h (лежит рядом с исполняемым файлом)
            string rtPath = Path.Combine(AppContext.BaseDirectory, "fab_runtime.h");
            if (!File.Exists(rtPath))
            {
                Console.ForegroundColor = ConsoleColor.Red;
                Console.Error.WriteLine($"[fab:build] fab_runtime.h not found: {rtPath}");
                Console.Error.WriteLine("  Place fab_runtime.h next to the fab executable.");
                Console.ResetColor();
                return 1;
            }
            string header = $"#include {'"'}{rtPath}{'"'}";
            FabCCompiler.RuntimeHeader = header;

            string cSource = "";
            try
            {
                cSource = new FabCCompiler().Compile(program);
            }
            catch (Exception ex)
            {
                PrintCompileError("transpile", ex.Message);
                return 1;
            }

            // --emit-cs: сохранить C-исходник и выйти
            if (emitCsOnly)
            {
                string cFile = Path.Combine(outDir, baseName + ".g.cpp");
                File.WriteAllText(cFile, cSource);
                PrintStep("codegen", cFile);
                PrintStep("done", "C++ source written (--emit-cpp, skipping binary compilation)");
                return 0;
            }

            // ── Step 3: g++/clang++ → native binary ────────────────────────────
            return FabCRunner.Build(cSource, outDir, baseName);
        }

        // ── Interpreter mode ──────────────────────────────────────────────────

        static void ExecuteScript(string path)
        {
            string code = File.ReadAllText(path);
            string fileName = Path.GetFileName(path);

            try
            {
                var tokens = new Lexer().Lex(code).ToList();
                var program = new Parser(tokens).Parse();
                var interpreter = new AST.FabInterpreter();
                interpreter.Eval(program);
            }
            catch (FabException ex)
            {
                PrintRuntimeError(fileName, ex.Line, ex.Message, code.Replace("\t", ""));
                Environment.Exit(1);
            }
            catch (Exception ex)
            {
                PrintRuntimeError(fileName, 0, ex.Message, code.Replace("\t", ""));
                Environment.Exit(1);
            }
        }

        // ── Help ──────────────────────────────────────────────────────────────

        static void PrintHelp()
        {
            Console.WriteLine(@"
FAB# Interpreter & Compiler

USAGE:
  fab <script.fab>                     Run with interpreter (default)
  fab --compile <script.fab>           Compile to native binary
  fab --compile -o <dir> <script.fab>  Compile, put output in <dir>
  fab --help                           Show this help

COMPILE PIPELINE:
  1. Lex + Parse Fab source
  2. Transpile AST → C++ source  (<name>.g.cpp)
  3. g++ -std=c++17 → self-contained binary (<name> / <name>.exe)

The resulting binary has no runtime dependency on .NET or the Fab interpreter.
");
        }

        // ── Diagnostics ───────────────────────────────────────────────────────

        static void PrintStep(string tag, string msg)
        {
            Clr(ConsoleColor.DarkCyan);
            Console.Write($"[fab:{tag}] ");
            Reset();
            Console.WriteLine(msg);
        }

        static void PrintCompileError(string phase, string message)
        {
            Clr(ConsoleColor.Red);
            Console.Error.Write($"[fab:{phase} error] ");
            Reset();
            Console.Error.WriteLine(message);
        }

        static void PrintRuntimeError(string file, int line, string message, string? source = null)
        {
            const int width = 62;
            string time = DateTime.Now.ToString("HH:mm:ss");

            Clr(ConsoleColor.DarkRed);
            Console.Error.Write("! ");
            Clr(ConsoleColor.Red);
            Console.Error.Write("error");
            Clr(ConsoleColor.DarkGray);
            Console.Error.Write($" [{file}]");
            Clr(ConsoleColor.DarkRed);
            Console.Error.WriteLine(new string('─', Math.Max(0, width - 9 - file.Length)));
            Reset();

            Clr(ConsoleColor.DarkRed);
            Console.Error.Write("│ ");
            Reset();
            Console.Error.Write("  ");
            Clr(ConsoleColor.White);
            Console.Error.WriteLine(message);
            Reset();

            Clr(ConsoleColor.DarkRed);
            Console.Error.Write("│ ");
            Reset();
            Clr(ConsoleColor.DarkCyan);
            Console.Error.Write("  --> ");
            Reset();
            if (line > 0)
            {
                Clr(ConsoleColor.Cyan);
                Console.Error.Write(file);
                Clr(ConsoleColor.DarkGray);
                Console.Error.Write(":");
                Clr(ConsoleColor.Yellow);
                Console.Error.WriteLine($"line {line}");
            }
            else
            {
                Clr(ConsoleColor.Cyan);
                Console.Error.WriteLine(file);
            }
            Reset();

            if (line > 0 && source != null)
            {
                string[] lines = source.Split('\n');
                if (line - 1 < lines.Length)
                {
                    string snippet = lines[line - 1].TrimEnd();
                    string lineNum = line.ToString();

                    Clr(ConsoleColor.DarkRed);
                    Console.Error.WriteLine("│");
                    Reset();

                    Clr(ConsoleColor.DarkRed);
                    Console.Error.Write("│ ");
                    Clr(ConsoleColor.DarkGray);
                    Console.Error.Write($"  {lineNum} │  ");
                    Clr(ConsoleColor.White);
                    Console.Error.WriteLine(snippet.Replace("\t", ""));
                    Reset();

                    Clr(ConsoleColor.DarkRed);
                    Console.Error.Write("│ ");
                    Clr(ConsoleColor.DarkGray);
                    Console.Error.Write($"  {new string(' ', lineNum.Length)} │  ");
                    Clr(ConsoleColor.Red);
                    Console.Error.WriteLine(new string('^', Math.Max(1, snippet.TrimStart().Length)));
                    Reset();
                }
            }

            Clr(ConsoleColor.DarkRed);
            Console.Error.WriteLine("│");
            Console.Error.Write("│ ");
            Reset();
            Clr(ConsoleColor.DarkGray);
            Console.Error.Write($"  at ");
            Clr(ConsoleColor.DarkYellow);
            Console.Error.WriteLine(time);
            Reset();

            Clr(ConsoleColor.DarkRed);
            Console.Error.WriteLine("!" + new string('─', width));
            Reset();
        }

        static void Clr(ConsoleColor c) => Console.ForegroundColor = c;
        static void Reset() => Console.ResetColor();
    }
}