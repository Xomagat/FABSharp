# FAB#

*Programming language documentation · version 0.4*

> [!WARNING]
> FAB# is under active development. In version 0.4, the syntax and feature set may still change, so check the documentation when you update the compiler.

## Contents

- [Getting started](#getting-started)
- [Quick start](#quick-start)
- [Output](#output)
- [Variables and types](#variables-and-types)
- [Operators](#operators)
- [Conditionals](#conditionals)
- [Loops](#loops)
- [Functions](#functions)
- [Input](#input)
- [Type methods](#type-methods)
- [Lists](#lists)
- [Dictionaries](#dictionaries)
- [Classes](#classes)
- [Namespaces](#namespaces)
- [Runtime errors](#runtime-errors)
- [Libraries and modules](#libraries-and-modules)
- [Low-level features](#low-level-features)
- [Mini project: FizzBuzz](#mini-project-fizzbuzz)
- [Cheat sheet](#cheat-sheet)
- [Common errors](#common-errors)
- [Not yet supported](#not-yet-supported)
- [Known issues](#known-issues)
- [Version history](#version-history)

## Getting started

FAB# is a compiled programming language written in C++ with LLVM. You write a program in a `.fab` file, the compiler turns it into a regular executable, and from then on it runs on its own, with no interpreter and no extra dependencies.

The idea is simple: as little ceremony as possible. No mandatory headers or "magic" boilerplate — open a file, write a line, and you have a program. And when your program grows, the language has functions, classes with inheritance, namespaces, modules, threads, and libraries.

| What | How |
|---|---|
| File extension | `.fab` |
| Language version | 0.4 |
| Platforms | Windows and Linux (x86-64) |
| Compilation result | a standalone executable file |
| Entry point | the `main` function or code right in the file |
| End of statement | `;` |
| Code blocks | `{ ... }` |
| Comments | `// line` and `/* block */` |

> [!NOTE]
> **What's new in 0.4:** the `++` and `--` operators, template strings `$"..{x}.."`, string comparison via `==` and `<`, the `for (x in list)` loop, namespaces (`namespace`), `throw`, named arguments `f(b = 2)`, default values for methods and constructors, initial field values, bounds checking for lists, division by zero, and null references, nice error messages pointing to the exact place in the code, the `bits`, `strbuf`, and `thread` libraries, plus faster output and string handling. See the full list in [Version history](#version-history).

## Quick start

Three steps from an empty folder to a working program:

1. Create a `hello.fab` file with the code from the example below.
2. Compile it in a terminal: `fab hello.fab`
3. Run the result: `./hello` on Linux or `hello.exe` on Windows.

```cpp
writeln "Hello, World!";
```

The executable appears next to the source file. The compiler removes the intermediate object file itself.

### Compiler options

```
fab [options] file.fab
```

| Option | What it does |
|---|---|
| `-h`, `--help` | shows help |
| `-v`, `--version` | shows the version |
| `-o <file>` | output file name |
| `-O0` `-O1` `-O2` `-O3` | optimization level (default is `-O2`) |
| `-Os`, `-Oz` | optimize for size |
| `-c` | compile to an object file only, without linking |
| `-S` | save assembly (`.s`) |
| `--emit-llvm` | save LLVM IR (`.ll`) |
| `--check` | check syntax only |
| `-r`, `--run` | run the program after building and show its exit code |
| `--keep-obj` | keep the object file after linking |
| `--cpu=<name>` | target CPU (default `x86-64-v3`) |
| `-V`, `--verbose` | show build stages |

For example, `fab -r hello.fab` builds and immediately runs the program, while `fab --check hello.fab` quickly checks whether the syntax has any errors.

If the code has errors, the compiler shows several at once (up to 20): for each one it gives the file, line, and column, and underlines the offending line of code. Warnings (`warning`) are printed the same way but don't stop the build.

> [!TIP]
> Want your program to be a "real" one, with a `main` function? See the [Functions](#functions) section. But for short scripts you don't need it: you can write code right in the file, and it runs top to bottom.

## Output

There are two output commands. They are almost twins and differ in one thing: the line break.

| Command | Behavior |
|---|---|
| `writeln` | prints the value and moves to a new line |
| `write` | prints the value and stays on the same line |

```cpp
write "Hello, ";
write "World!\n";
writeln "Bye!";
writeln;                 // just an empty line
```

Result:

```
Hello, World!
Bye!

```

### What it can print

| Value | Example | What you'll see |
|---|---|---|
| integer | `writeln 42;` | `42` |
| floating-point number | `writeln 0.5;` | `0.5` (up to 17 significant digits, extra zeros are not printed) |
| boolean | `writeln true;` | `true` (and `false`) |
| character | `writeln 'A';` | `A` |
| string | `writeln "Hi";` | `Hi` |
| list | `writeln [1, 2];` | `[1, 2]` |
| dictionary | `writeln d;` | `{a: 1, b: 2}` |

Objects of your own classes and library types (`file`, `date`, `time`, `strbuf`, `thread`...) are not printed directly: print their fields or a method's result (`d.to_str()`), or use a template string.

> [!NOTE]
> Floating-point numbers are printed with maximum precision: `double` — up to 17 significant digits, `float` — up to 9, `long double` — up to 21. So `writeln 4.0;` prints `4`, and `writeln 3.14;` may show `3.1400000000000001` — that's a feature of binary representation, not a bug. Inside lists and dictionaries, floating-point numbers are printed with six digits after the decimal point (`[1.500000]`).

### String interpolation

If you put `$` before the quote, you can write expressions in curly braces inside the string. The value is converted to text automatically; there is no need to call `to_str()`:

```cpp
string name = "Ann";
int age = 20;

writeln $"{name} ({age} years), in a year she'll be {age + 1}";
writeln $"Name length: {name.length()}, in upper case: {name.to_upper()}";
writeln $"Curly braces: {{ and }}";
```

Result:

```
Ann (20 years), in a year she'll be 21
Name length: 3, in upper case: ANN
Curly braces: { and }
```

Rules:

- inside `{ }` there can be any expression, including a function or method call and a string in quotes;
- to print a brace itself, double it: `{{` and `}}`;
- strings, numbers, `bool`, `char`, as well as `date`, `time`, and `strbuf` can be interpolated; lists and dictionaries cannot (use `join()`);
- regular escape sequences (`\n`, `\t`, `\"`) work inside templates;
- a template can be used anywhere a string is needed: `string s = $"x = {x}";`.

### Escape sequences in strings and characters

| Notation | Meaning |
|---|---|
| `\n` | line feed |
| `\t` | tab |
| `\r` | carriage return |
| `\0` | null character |
| `\a` | beep |
| `\"` | a quote inside a string |
| `\'` | an apostrophe inside a character |

Several string literals in a row are concatenated into one, even if they are on different lines. This is handy for long text:

```cpp
string text = "The first part, "
              "the second part, "
              "the third part.";
```

### Comments

Anything the compiler doesn't need to know can be hidden in a comment:

```cpp
// A single-line comment

/*
   And this one can span
   as many lines as you like
*/
writeln "Comments don't interfere with code";

## Variables and types

A variable is a little box with a name that holds a value. The type of the box is set once, when it is created.

| Type | Size | What it holds | Example |
|---|---|---|---|
| `int` | 32 bits | integers from -2,147,483,648 to 2,147,483,647 | `int x = 42;` |
| `long` | 64 bits | very large integers | `long big = 100;` |
| `short` | 16 bits | small integers | `short s = 7;` |
| `double` | 64 bits | floating-point numbers (about 15-17 significant digits) | `double d = 3.14159;` |
| `float` | 32 bits | floating-point numbers with lower precision | `float f = 1.5;` |
| `long double` | 80 bits | high-precision floating-point numbers (x87) | `long double e = 1.5L;` |
| `bool` | 1 bit | `true` or `false` | `bool ready = true;` |
| `char` | 8 bits | one character | `char c = 'a';` |
| `string` | — | text | `string name = "FAB#";` |
| `list<T>` | — | a list of values of type `T` | `list<int> nums = [1, 2, 3];` |
| `dict<K,V>` | — | a "key → value" dictionary | `dict<string, int> ages = {"Anna": 20};` |
| `auto` | — | the type is taken from the value on the right | `auto n = 5;` |
| `void` | — | "nothing": only as a function return type | `define f() -> void` |

Besides these, there are types you declare yourself ([classes](#classes)) and library types: `file` from `io`, `date` and `time` from `date`, `strbuf` from `strbuf`, `thread` and `mutex` from `thread`.

### Declaration and assignment

```cpp
int age = 20;
double pi = 3.14159;
string name = "FAB#";
bool ready = true;
char grade = 'A';
int color = #FF8800;    // a hexadecimal number starts with #
int counter;            // without a value: it will hold 0

age = 21;               // a new value
age += 1;               // age became 22
age++;                  // age became 23
```

Short assignment forms:

| Notation | Equivalent |
|---|---|
| `x += 5;` | `x = x + 5;` |
| `x -= 5;` | `x = x - 5;` |
| `x *= 5;` | `x = x * 5;` |
| `x /= 5;` | `x = x / 5;` |
| `x %= 5;` | `x = x % 5;` |
| `x++;`, `++x;` | `x = x + 1;` |
| `x--;`, `--x;` | `x = x - 1;` |

> [!NOTE]
> If you create a numeric or boolean variable without a value, it gets a zero value (`0` or `false`), and a list or dictionary becomes empty. Strings are better handled differently: always give them an initial value, such as `string s = "";`.

### auto

If the type is obvious from the value, you can leave it out. The compiler will figure it out:

```cpp
auto n = 5;                 // int
auto pi = 3.14;             // double
auto big = 10L;             // long
auto name = "fab".to_upper();   // string
```

`auto` must always have an initial value: `auto x;` and `auto x = null;` won't work.

### Constants

The keyword `const` makes a value immutable. A constant must be given a value right away. Only simple types (`int`, `double`, `bool`, `string`, etc.) can be constants, not lists or dictionaries.

```cpp
const int MAX_PLAYERS = 4;
writeln MAX_PLAYERS;      // 4

// MAX_PLAYERS = 10;      // compilation error: a constant can't be changed
```

### Numbers of different types get along

If you put a floating-point number into an integer variable (or the other way around), the compiler converts it for you:

```cpp
double d = 5;      // becomes 5
int n = 3.99;      // becomes 3: the fractional part is dropped, no rounding
```

For explicit conversions, every type has methods `to_int()`, `to_double()` and others; see [Type methods](#type-methods).

### Kinds of literals in code

| What | How it looks | Examples |
|---|---|---|
| integer | digits | `0`, `42`, `1000` |
| number with separators | `_` between digits | `1_000_000` |
| floating-point number | digits with a dot | `3.14`, `0.5` |
| number with an exponent | `e` or `E` | `1e6`, `2.5e-3` |
| hexadecimal | `#` followed by digits/letters a-f | `#FF`, `#1a2b` |
| boolean | keywords | `true`, `false` |
| character | in single quotes | `'a'`, `'\n'` |
| string | in double quotes | `"Hello"` |
| template string | `$` and quotes | `$"x = {x}"` |
| list | values in square brackets | `[1, 2, 3]` |
| dictionary | `key: value` pairs in curly braces | `{"a": 1, "b": 2}` |
| null value | keyword | `null` (becomes zero of the needed type, or an empty list/dictionary) |

### Number suffixes

Without a suffix, an integer is an `int` (or a `long` if it doesn't fit), and a floating-point number is a `double`. To set the type explicitly, add a letter:

| Suffix | Type | Example |
|---|---|---|
| `s` | `short` | `7s` |
| `L` | `long` (for integers) | `10000000000L` |
| `f` | `float` | `1.5f` |
| `d` | `double` | `2d` |
| `L` | `long double` (for floating-point) | `1.5L` |

```cpp
long double e = 2.718281828459045235L;
float f = 0.1f;
writeln size_of(7s);    // 2
```

> [!NOTE]
> A dictionary literal only knows its type when it is put into a variable with an explicit type: `dict<string, int> d = {"a": 1};`. A list literal with elements can be used on its own (`[1, 2, 3].sum()`), while an empty list `[]` needs an explicit type: `list<int> a = [];`.

### Where a variable "lives"

A variable created inside `{ ... }` disappears at the closing brace.

```cpp
int x = 1;
if x == 1
{
    int y = 2;           // y is visible only inside these braces
    writeln x + y;       // 3
}
```

### Global variables

Variables declared at the top level (outside functions and classes) are global: they are visible from any function.

```cpp
int counter = 0;

define hit()
{
    counter++;
}

hit();
hit();
writeln counter;     // 2
```

If you write `static` before the declaration, the variable is visible only in its own file. This is handy in modules, so internal data doesn't clash with other names (see [Including your own file](#including-your-own-file)):

```cpp
static int secret = 42;     // available only in this file
```

You cannot declare a global variable with the same name twice. If the file has a `main`, the values of global variables are computed at the very beginning of `main`.

## Operators

### Arithmetic

| Operator | What it does | Example | Result |
|---|---|---|---|
| `+` | addition | `7 + 2` | `9` |
| `-` | subtraction | `7 - 2` | `5` |
| `*` | multiplication | `7 * 2` | `14` |
| `/` | division | `7 / 2` | `3` (integer divided by integer gives an integer) |
| `/` | division | `7.0 / 2` | `3.5` |
| `%` | remainder of division | `7 % 2` | `1` |

When an expression contains both integers and floating-point numbers, the result becomes floating-point. Between two integer types, the result takes the wider one. Dividing an integer by zero stops the program with the message `Division by zero` (see [Runtime errors](#runtime-errors)).

Strings can be added: that's how they are glued together.

```cpp
string greeting = "Hello, " + "World!";
writeln greeting;        // Hello, World!
```

You can only glue a string to a string. To attach a number, use a template string or `to_str()`:

```cpp
int n = 5;
writeln "n = " + n.to_str();    // n = 5
writeln $"n = {n}";             // n = 5
```

Unary operators: `-x` flips the sign, `+x` does nothing, `!flag` flips the boolean value.

### Increment and decrement

`++` and `--` add and subtract one. They can go after a variable (`i++`) or before it (`++i`). The difference shows when the result is used in an expression: `i++` yields the old value, while `++i` yields the new one.

```cpp
int i = 5;
i++;                 // 6
++i;                 // 7
int a = i++;         // a = 7, i = 8
int b = ++i;         // b = 9, i = 9

list<int> nums = [1, 2, 3];
nums[0]++;           // [2, 2, 3]
```

They work for integers and floating-point numbers, as well as for list elements and object fields (`this.value++`, `obj.count--`). A constant cannot be changed.

### Comparison

| Operator | Meaning |
|---|---|
| `==` | equal |
| `!=` | not equal |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal |
| `>=` | greater than or equal |

Characters are compared like numbers: `c == 'a'`, `c >= '0'`. Strings can also be compared with these operators: the comparison goes character by character and is case-sensitive (like `compare()`). For a case-insensitive comparison there is `equals_ignore_case()`.

```cpp
string a = "apple";
if a == "apple" { writeln "that very apple"; }
if a < "banana" { writeln "a comes earlier alphabetically"; }
```

A string must be compared with a string: `a == 'x'` or `a == 5` won't work.

### Logic

| Operator | Meaning | Example |
|---|---|---|
| `&&` | AND: true if both sides are true | `x > 0 && x < 10` |
| `\|\|` | OR: true if at least one side is true | `x < 0 \|\| x > 10` |
| `!` | NOT: flips the value | `!ready` |

Evaluation is "lazy": if the answer is already clear from the left side, the right side isn't even evaluated.

### Operator precedence

From strongest to weakest:

| Level | Operators |
|---|---|
| 0 | `++` and `--` (after and before a variable) |
| 1 | unary `-`, `+`, `!` |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-` |
| 4 | `<`, `>`, `<=`, `>=` |
| 5 | `==`, `!=` |
| 6 | `&&` |
| 7 | `\|\|` |

Not sure about the order? Add parentheses: `(a + b) * c`.

> [!WARNING]
> There is no exponentiation operator `^` and no bitwise operators (`&`, `|`, `<<`) in the language. For powers, use the `pow` function from the `math` library, and for bitwise operations the [`bits`](#bits) library.

## Conditionals

The classic "if ... else":

```cpp
int x = 15;

if x == 15
{
    writeln "exactly fifteen";
}
else
{
    writeln "something else";
}
```

If there are more than two options, chain `else if`:

```cpp
int score = 72;

if score >= 90
{
    writeln "excellent";
}
else if score >= 60
{
    writeln "good";
}
else
{
    writeln "needs work";
}
```

A few rules:

- Parentheses around the condition are **not required**: you can write `if x > 5` or `if (x > 5)`.
- Conditions can be combined: `if x > 0 && x < 10`.
- A number also works as a condition: `0` means "false", everything else is "true".
- If a branch has a single statement, you can omit the braces.

## Loops

| Loop | When to use |
|---|---|
| `while` | repeat while a condition holds |
| `do-while` | the same, but the body runs at least once |
| `for` | when you know how many times to repeat |
| `for (x in ...)` | go over all elements of a list, dictionary, or string |

> [!IMPORTANT]
> In loops, parentheses around the condition are **required**, unlike in `if`.

### while

```cpp
int i = 0;
while (i < 3)
{
    writeln i;
    i++;
}
```

Result: `0`, `1`, `2`.

### do-while

First the action, then the check. So the body runs at least once, even if the condition is false from the start:

```cpp
int i = 10;
do
{
    writeln i;
    i++;
} while (i < 5);
```

Result: `10`. Note the `;` after the closing parenthesis of the condition.

### for

Three parts in parentheses: what to do before the start, when to keep going, and what to do after each step.

```cpp
for (int i = 1; i <= 5; i++)
{
    writeln i;
}
```

Result: `1`, `2`, `3`, `4`, `5`.

This is a convenient way to walk over a list:

```cpp
list<string> names = ["Ann", "Bob", "Vicky"];
for (int i = 0; i < names.length(); i++)
{
    writeln names[i];
}
```

### for ... in

When you don't need the element's index, it's easier to go over the elements themselves:

```cpp
list<string> names = ["Ann", "Bob", "Vicky"];

for (string n in names)
{
    writeln n;
}

for (auto x in [1, 2, 3]) { writeln x * 10; }    // 10, 20, 30

for (char c in "abc") { writeln c; }             // a, b, c
```

- You can write the variable's type explicitly (`string n`), put `auto`, or omit it entirely: `for (n in names)`.
- A **list** is walked element by element, a **string** character by character (`char`), a **dictionary** by keys. The value for a key is taken with `d.get(k)`.
- The loop variable is a copy: changing it does not change the list itself.
- The variable's numeric type may differ from the element type (`for (long x in ints)`); the compiler converts it.
- `break` and `continue` work as in regular loops.

```cpp
dict<string, int> ages = {"Ann": 20, "Bob": 31};
for (string k in ages)
    writeln $"{k}: {ages.get(k)}";
```

### break and continue

| Command | What it does |
|---|---|
| `break;` | immediately exits the loop |
| `continue;` | skips the rest of the step and moves to the next one |

```cpp
int i = 0;
while (true)
{
    i++;
    if i % 2 == 0 { continue; }   // skip even numbers
    if i > 7 { break; }           // exit after 7
    writeln i;
}
```

Result: `1`, `3`, `5`, `7`.

> [!TIP]
> In a `for` loop, `continue` won't get stuck forever: it moves to the `i++` step, not straight to the condition check.

## Functions

A function is a named piece of code you can call as many times as you like.

```cpp
define name(type arg1, type arg2) -> resultType
{
    // body
}
```

- `define` declares a function.
- The result type goes after the arrow `->`. If there is no arrow, the function returns nothing (`void`).
- The result is returned with the `return` statement.
- Arguments and results can use any types, including `list<int>`, `dict<string, int>`, your own classes, and library types.

### A function with no result

```cpp
define greet(string name)
{
    writeln "Hello, " + name + "!";
}

greet("FAB#");        // Hello, FAB#!
```

### A function with a result

```cpp
define square(int x) -> int
{
    return x * x;
}

writeln square(7);    // 49
```

If the body is a single statement, you don't need the braces:

```cpp
define twice(int x) -> int
    return x * 2;
```

### Default values

An argument can have a default value. Then you're allowed to leave it out when calling. Such arguments go at the end of the list:

```cpp
define greet(string name, string hello = "Hi")
{
    writeln hello + ", " + name + "!";
}

greet("Ann");               // Hi, Ann!
greet("Bob", "Hey");        // Hey, Bob!
```

You cannot put a regular argument after an argument with a default value. Methods and class constructors can have default values too.

### Named arguments

An argument can be passed by name: `name = value`. This is handy when a function has many optional arguments and you only need to set one of them. The order of named arguments doesn't matter, but positional arguments must come before named ones.

```cpp
define box(int w, int h = 1, string label = "box")
{
    writeln $"{label}: {w}x{h}";
}

box(5);                       // box: 5x1
box(5, label = "wide");       // wide: 5x1   (h stayed default)
box(h = 3, w = 2);            // box: 2x3
```

This works for functions, methods, constructors (`new Box(w = 2)`), static methods, and library functions that define argument names (for example, `bin_str(5, width = 8)`).

### Recursion

A function can call itself:

```cpp
define factorial(int n) -> int
{
    if n <= 1 { return 1; }
    return n * factorial(n - 1);
}

writeln factorial(5);    // 120
```

### Overloading

You can create several functions with the same name if their argument types differ. The compiler picks the right one itself:

```cpp
define show(int x)
{
    writeln x;
}

define show(string s)
{
    writeln "text: " + s;
}

show(5);          // 5
show("hi");       // text: hi
```

> [!WARNING]
> The argument types when calling your function must match the declaration exactly. If there is only `square(int x)`, then `square(2.5)` won't work: `2.5` is a `double`. Declare a `square(double x)` version or pass an `int`. Library functions get an allowance: if there is no exact match, the compiler picks the closest version with the fewest conversions (that's why `sin(1)` works). If there are several equally good versions, you'll get the `Ambiguous call` error.

### The main function

There are two ways to write a program:

| Way | What it looks like |
|---|---|
| Without `main` | statements sit right in the file and run top to bottom |
| With `main` | the whole program lives inside `main`, as in big languages |

```cpp
define main() -> int
{
    writeln "Hello from main!";
    return 0;
}
```

Writing `return 0;` at the end of `main` is optional: if you forget, the compiler returns `0` itself. The number `main` returns becomes the program's exit code.

> [!IMPORTANT]
> If the file has a `main`, only declarations may live outside it: functions (`define`), classes, namespaces, and global variables. Regular statements like `writeln` must live inside functions. Functions must be declared **above** the place where you call them (classes can also go below).

## Input

The `input_in` statement reads a value from the keyboard straight into a variable. The variable must be created beforehand.

```cpp
int n;
write "Enter a number: ";
input_in n;
writeln n * 2;
```

What can be read:

| Variable type | What is read |
|---|---|
| `int`, `short`, `long` | an integer |
| `float`, `double` | a floating-point number (with a dot) |
| `bool` | a number: `0` — `false`, anything else — `true` |
| `char` | one character (leading spaces and line breaks are skipped) |
| `string` | one word up to the first space |

> [!NOTE]
> `input_in` cannot read a whole line with spaces. For that, include the `console` library and call `console.read_line()` (see [console](#console)). Lists and dictionaries cannot be read via `input_in`.

## Type methods

Values have **methods** — small functions called with a dot: `value.method(arguments)`. Methods work on variables and on literals alike, and they can be chained:

```cpp
string s = "  fab#  ";
writeln s.trim().to_upper();      // FAB#
writeln 'x'.to_upper();           // X
writeln "42".to_int() + 1;        // 43
```

Methods of `string`, `char`, `bool`, `int`, `short`, `long`, `float`, `double`, `long double`, `list`, and `dict` are always connected; no `use` is needed for them. Below, `T` is the list element type and `K`/`V` are the dictionary key and value types.

### string

| Method | Result | What it does |
|---|---|---|
| `length()` | `int` | string length |
| `empty()` | `bool` | is the string empty? |
| `at(i)` | `char` | character by index (out of range gives `'\0'`) |
| `first()`, `last()` | `char` | the first and the last character |
| `find(x)` | `int` | position of the first occurrence of a string or character `x`, `-1` if not found |
| `rfind(s)` | `int` | position of the last occurrence, `-1` if absent |
| `contains(s)` | `bool` | contains a substring? |
| `count(s)` | `int` | how many times a substring occurs |
| `start_with(s)`, `ends_with(s)` | `bool` | starts with / ends with `s` |
| `substr(start, length)` | `string` | a piece of the string (empty string for invalid bounds) |
| `replace(a, b)` | `string` | replaces all `a` with `b` |
| `to_upper()`, `to_lower()` | `string` | case |
| `capitalize()` | `string` | makes the first letter uppercase |
| `trim()` | `string` | strips whitespace at the ends |
| `reverse()` | `string` | reverses |
| `repeat(n)` | `string` | repeats `n` times |
| `pad_left(width, fill)`, `pad_right(width, fill)` | `string` | pads with the `fill` character up to `width` |
| `compare(s)` | `int` | like `strcmp`: less than zero, zero, or greater than zero |
| `equals_ignore_case(s)` | `bool` | equality ignoring case |
| `is_blank()` | `bool` | only whitespace characters |
| `is_numeric()` | `bool` | only digits |
| `to_int()`, `to_long()`, `to_double()` | number | parses a number out of the string |
| `to_bool()` | `bool` | `true` only if the string equals `"true"` |
| `split(sep)` | `list<string>` | splits the string by a separator |

A string character can also be taken with square brackets: `s[0]`. Brackets don't check the string bounds, while `at(i)` does.

### char

| Method | Result | What it does |
|---|---|---|
| `is_digit()`, `is_alpha()`, `is_alnum()` | `bool` | digit / letter / letter or digit |
| `is_upper()`, `is_lower()` | `bool` | uppercase / lowercase |
| `is_space()`, `is_punct()` | `bool` | whitespace / punctuation mark |
| `is_hex()`, `is_printable()`, `is_ascii()` | `bool` | hex digit / printable / ASCII |
| `to_upper()`, `to_lower()`, `swap_case()` | `char` | case |
| `to_int()`, `to_short()`, `to_long()`, `to_float()`, `to_double()` | number | character code |
| `to_digit()` | `int` | `'0'..'9'` → `0..9`, otherwise `-1` |
| `to_hex()` | `int` | `'0'..'9'`, `'a'..'f'`, `'A'..'F'` → `0..15`, otherwise `-1` |
| `to_str()` | `string` | the character as a string |
| `repeat(n)` | `string` | a string of `n` such characters |
| `compare(c)` | `int` | difference of codes |
| `equals_ignore_case(c)` | `bool` | equality ignoring case |

### int, short, long

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | the number as a string |
| `is_positive()`, `is_negative()`, `is_zero()` | `bool` | sign |
| `is_even()`, `is_odd()` | `bool` | even / odd |
| `abs()` | same type | absolute value |
| `sign()` | `int` | `-1`, `0`, or `1` |
| `to_bool()`, `to_char()` | `bool` / `char` | conversion |
| `to_short()`, `to_int()`, `to_long()`, `to_float()`, `to_double()`, `to_ldouble()` | number | conversion (into its own type isn't needed) |

### float, double, long double

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | the number as a string |
| `is_positive()`, `is_negative()`, `is_zero()` | `bool` | sign |
| `is_nan()`, `is_inf()`, `is_finite()` | `bool` | special values |
| `abs()` | same type | absolute value |
| `sign()` | `int` | `-1`, `0`, or `1` |
| `floor()`, `ceil()`, `trunc()`, `round()` | same type | rounding (`round` — away from zero: `2.5` → `3`) |
| `to_bool()`, `to_char()`, `to_short()`, `to_int()`, `to_long()` | value | conversion; `NaN` gives `0`, too large numbers are clipped to the type's limit |
| `to_float()`, `to_double()`, `to_ldouble()` | floating-point | conversion between floating-point types |

### bool

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | `"true"` or `"false"` |
| `to_bin()` | `string` | `"1"` or `"0"` |
| `to_int()`, `to_short()`, `to_long()`, `to_float()`, `to_double()` | number | `0` or `1` |
| `negate()` | `bool` | NOT |
| `and(b)`, `or(b)`, `xor(b)` | `bool` | AND, OR, exclusive OR |
| `nand(b)`, `nor(b)`, `xnor(b)` | `bool` | negations of the above |
| `implies(b)` | `bool` | "if a, then b" |
| `compare(b)` | `int` | `false < true`: `-1`, `0`, or `1` |

> [!NOTE]
> Unlike `&&` and `||`, the `and` and `or` methods always evaluate both sides.

### Limit constants

These constants are available without `use`:

| Constant | Type | Value |
|---|---|---|
| `INT_MIN`, `INT_MAX` | `int` | `int` bounds |
| `SHORT_MIN`, `SHORT_MAX` | `short` | `short` bounds |
| `LONG_MIN`, `LONG_MAX` | `long` | `long` bounds |
| `CHAR_MIN`, `CHAR_MAX` | `int` | `-128` and `127` |
| `FLOAT_MIN`, `FLOAT_MAX`, `FLOAT_EPSILON` | `float` | smallest and largest positive value, machine precision |
| `DOUBLE_MIN`, `DOUBLE_MAX`, `DOUBLE_EPSILON` | `double` | the same for `double` |
| `LDOUBLE_MIN`, `LDOUBLE_MAX`, `LDOUBLE_EPSILON` | `long double` | the same for `long double` |

## Lists

A list holds a sequence of values of one type. Elements can be `int`, `long`, `short`, `char`, `float`, `double`, `string`, and `bool`.

```cpp
list<int> nums = [5, 2, 9];
nums.addend(7);                // add to the end
nums.sort();

writeln nums;                  // [2, 5, 7, 9]
writeln nums.length();         // 4
writeln nums[0];               // 2
writeln nums.sum();            // 23

nums[0] = 100;                 // write an element
nums[1]++;                     // increase an element by 1
```

An empty list is created without a value, via `null`, or via `[]`:

```cpp
list<string> words;
words.addend("one");
words.addend("two");
writeln words.join(", ");      // one, two

list<int> empty = [];
```

> [!TIP]
> Don't write `>` and `=` together when closing a type: the lexer reads `list<int>= ...` as `>=`. Put a space or the variable name right away: `list<int> nums = ...`.

> [!IMPORTANT]
> Reading and writing through square brackets (`nums[i]`, `nums[i] = x`) check the bounds: with an invalid index the program prints `Index out of range` and exits with code 1. List methods (`get`, `set`, `insert`, `remove_at`) are gentler: `get(100)` on a short list returns the "zero" value of the type (`0`, `false`, or an empty string), while `set`, `insert`, and `remove_at` with an invalid index simply do nothing. If an index might go out of bounds and that's not a reason to stop the program, use `get` and `set`.

### List methods

| Method | Result | What it does |
|---|---|---|
| `addend(x)` | — | adds `x` **to the end** |
| `add(x)` | — | adds `x` **to the beginning** |
| `insert(i, x)` | — | inserts `x` at position `i` |
| `extend(other)` | — | appends all elements of another list to the end |
| `length()` | `int` | number of elements |
| `empty()` | `bool` | is the list empty? |
| `capacity()`, `reserve(n)` | `int` / — | capacity and reserving space |
| `get(i)`, `set(i, x)` | `T` / — | read / write an element |
| `first()`, `last()` | `T` | the first and the last element |
| `remove_at(i)` | — | deletes the element at position `i` |
| `remove(x)` | `bool` | deletes the first `x`; `true` if found |
| `pop()`, `pop_first()` | `T` | takes and deletes the last / first element |
| `clear()` | — | empties the list |
| `contains(x)` | `bool` | is `x` there? |
| `index_of(x)` | `int` | position of the first `x`, or `-1` |
| `count(x)` | `int` | how many times `x` occurs |
| `reverse()` | — | reverses in place |
| `swap(i, j)` | — | swaps two elements |
| `sort()`, `sort_desc()` | — | sort ascending / descending |
| `min()`, `max()` | `T` | smallest / largest |
| `sum()` | `T` | sum (concatenation for strings); unavailable for `char` and `bool` |
| `copy()` | `list<T>` | an independent copy |
| `slice(start, length)` | `list<T>` | a piece of the list |
| `join(sep)` | `string` | glues elements into a string with a separator |

Calling a method on a list whose value is `null` (if the variable was never created) stops the program with the message `Null reference`.

## Dictionaries

A dictionary stores "key → value" pairs and remembers the order in which keys were added. Keys can be `int`, `long`, `short`, `char`, and `string`; values — `int`, `long`, `short`, `char`, `float`, `double`, `string`, and `bool`.

```cpp
dict<string, int> ages = {"Ann": 20, "Bob": 31};
ages.set("Vicky", 25);
ages["Jim"] = 40;                // same as set

writeln ages.get("Ann");          // 20
writeln ages["Bob"];              // 31, same as get
writeln ages.get_or("Josh", -1);  // -1
writeln ages.contains("Bob");     // true
writeln ages.length();            // 4
writeln ages;                     // {Ann: 20, Bob: 31, Vicky: 25, Jim: 40}
```

Square brackets work for any keys: `d[k]` reads a value (like `get`), and `d[k] = v` writes one (like `set`). If the key is absent, reading gives the "zero" value of the type.

The `{...}` literal only knows its type when it creates a variable with the explicit type `dict<K, V>`. An empty dictionary is created without a value: `dict<int, string> d;`.

To go over all keys, use `for ... in`; if you need indexes, use `key_at` and `value_at`:

```cpp
for (string k in ages)
    writeln $"{k}: {ages.get(k)}";

for (int i = 0; i < ages.length(); i++)
    writeln $"{ages.key_at(i)}: {ages.value_at(i)}";
```

### Dictionary methods

| Method | Result | What it does |
|---|---|---|
| `set(k, v)` | — | adds a pair or replaces the value of an existing key |
| `merge(other)` | — | moves all pairs of another dictionary in |
| `get(k)` | `V` | value by key; if the key is absent, the "zero" value of the type |
| `get_or(k, def)` | `V` | the value by key, or `def` if the key is absent |
| `key_at(i)`, `value_at(i)` | `K` / `V` | key / value by ordinal number |
| `contains(k)` | `bool` | is the key there? |
| `contains_value(v)` | `bool` | is the value there? |
| `length()`, `empty()`, `capacity()` | `int` / `bool` | size |
| `remove(k)` | `bool` | deletes a pair; `true` if the key existed |
| `pop(k)` | `V` | deletes a pair and returns its value |
| `clear()` | — | empties the dictionary |
| `keys()`, `values()` | `list<K>` / `list<V>` | lists of keys and values |
| `copy()` | `dict<K,V>` | an independent copy |

## Classes

A class bundles data (fields) and actions over it (methods). Objects are created from a class.

```cpp
class Counter
{
private:
    int value;

public:
    Counter(int start)
    {
        this.value = start;
    }

    define inc()
    {
        this.value++;
    }

    define get() -> int
    {
        return this.value;
    }
}

Counter c = new Counter(10);
c.inc();
c.inc();
writeln c.get();       // 12
delete c;
```

### What a class is made of

| Part | How it's written | What it is |
|---|---|---|
| field | `int value;` or `int hp = 100;` | a variable inside an object (you can set an initial value right away) |
| method | `define name(...) -> type { ... }` | a function of the object |
| constructor | `Name(arguments) { ... }` | runs on creation |
| destructor | `~Name() { ... }` | runs on `delete` |
| static method | `static define name(...) { ... }` | called on the class itself |
| visibility | `private:` / `public:` | who may use the fields and methods that follow |

Visibility stays in effect until the next label. Everything before the first label is `private`. Private fields and methods can be accessed only from inside the class itself; from outside it's a compilation error.

### Creating and deleting objects

```cpp
Counter a = new Counter(5);    // an object with a constructor
Counter b;                     // create an object; a no-argument constructor runs if there is one
Counter c = null;              // no object yet

delete a;                      // run the destructor and free memory
```

After `delete`, the variable becomes `null`. Object memory is **not** freed automatically: if an object is no longer needed, delete it yourself. Lists, dictionaries, and library types stored in class fields are freed automatically by the destructor.

Constructor arguments, like function arguments, can have default values and be passed by name:

```cpp
class Unit
{
public:
    string name;
    int hp = 100;               // initial field value

    Unit(string n, int h = 50)
    {
        this.name = n;
        this.hp = h;
    }
}

Unit u = new Unit("Orc");                  // hp = 50
Unit v = new Unit(h = 10, n = "Elf");      // hp = 10
```

### Fields, methods, and this

A field is read through a dot: `c.value` (if it is `public`). Inside a method, you can refer to your object's fields simply by name, both to read and to write. But calling other methods of your own object requires `this`:

```cpp
define reset()
{
    value = 0;             // can go by name alone...
    this.value = 0;        // ...or through this, it's the same thing
    value += 5;
    this.inc();            // calling a method of your own object only through this
}
```

If an argument name matches a field name, the argument wins, and the field must be written through `this`: `this.name = name;`.

String fields default to `""`, list and dictionary fields are created empty, the rest are zero (unless an initial value is given).

> [!NOTE]
> Compound operators don't work on fields via dot yet (`obj.x += 1`). Use `obj.x++` or `obj.x = obj.x + 1`.

### Inheritance

A class can inherit the fields and methods of another class: `class Child : Parent`. If the parent has a constructor with arguments, the child must call it after the colon:

```cpp
class Animal
{
public:
    string name;

    Animal(string n)
    {
        this.name = n;
    }

    define speak()
    {
        writeln this.name + " makes a sound";
    }
}

class Dog : Animal
{
public:
    Dog(string n) : Animal(n)
    {
    }

    define speak()
    {
        writeln this.name + ": woof!";
    }
}

Animal a = new Dog("Rex");
a.speak();             // Rex: woof!
delete a;
```

Methods are virtual: a variable of type `Animal` holding a `Dog` object calls the `Dog` method. When overriding a method, the return type cannot change. From inside a constructor, the parent constructor can also be invoked with the statement `super(arguments);`.

### Static methods

A static method belongs to the class itself and is called without an object:

```cpp
class MathUtil
{
public:
    static define twice(int x) -> int
    {
        return x * 2;
    }
}

writeln MathUtil.twice(21);    // 42
```

### Methods with the same name

Methods, like functions, can be overloaded by argument types. The argument types at the call site must match the declaration exactly.

### Null references

If you call a method or access a field on a variable whose value is `null`, the program prints the message `Null reference: ...` and exits with code 1.

> [!NOTE]
> An object cannot be printed whole with `writeln`. Comparing variables to `null` with `==` isn't supported yet either.

## Namespaces

A namespace groups functions, classes, and global variables under a common name so they don't get confused with other people's. It's declared at the top level with the `namespace` statement:

```cpp
namespace geo
{
    define area(int w, int h) -> int
    {
        return w * h;
    }

    class Point
    {
    public:
        int x;
        int y;

        Point(int px, int py)
        {
            this.x = px;
            this.y = py;
        }
    }
}

writeln geo::area(3, 4);                // 12
geo::Point p = new geo::Point(1, 2);
writeln p.x;                            // 1
delete p;
```

Rules:

- names from a namespace are accessed through `::`: `geo::area(...)`, `geo::Point`;
- inside the namespace itself, its names are written without the prefix, and the names of outer namespaces are visible from within nested ones;
- namespaces can be nested: `namespace app { namespace util { ... } }` or shorter `namespace app::util { ... }`;
- a namespace can be opened several times; the contents are merged;
- the statement `use namespace geo;` makes names from `geo` available without the prefix (the namespace must be declared above):

```cpp
use namespace geo;
writeln area(2, 5);        // 10
```

- the name `std` is reserved. The notation `std::name(...)` calls a standard library function, even if you have your own function with the same name.

## Runtime errors

### Checks the program makes

Some errors the program notices itself: it prints a message and exits with code 1.

| Message | When it appears |
|---|---|
| `Index out of range` | a list element index (`nums[i]`) is out of bounds |
| `Division by zero` | integer division or remainder (`/`, `%`) by zero |
| `Null reference: ...` | a method or field on a `null` object, a method or index on a `null` list or dictionary |

Dividing floating-point numbers by zero does not raise an error; it gives `inf` or `nan`.

If the compiler sees that an error will always happen (for example, `x / 0` or `nums[-1]`), it issues a `warning` at build time already.

### throw

The `throw` statement stops the program, printing a message to the error stream. The exit code will be 1.

```cpp
define divide(int a, int b) -> int
{
    if b == 0 { throw "can't divide by zero"; }
    return a / b;
}

writeln divide(10, 2);    // 5
writeln divide(1, 0);     // the program stops with a message
```

You can throw a string or any value that has a `to_str()` method (numbers, `date`, `time`...). Errors cannot be caught yet: there are no `try` / `catch` blocks in the language.

## Libraries and modules

### Including a library

The `use` statement includes a library. Include it **before** the first use of its functions and types.

```cpp
use math;

writeln sqrt(16.0);    // 4
writeln pow(2, 10);    // 1024
writeln floor(3.7);    // 3
writeln round(2.5);    // 3
writeln PI;            // 3.1415926535897931
```

Available libraries:

| Library | What's inside |
|---|---|
| `math` | math |
| `random` | random numbers |
| `io` | files and path handling |
| `system` | running programs, arguments, environment, folders |
| `date` | dates and time |
| `console` | colors, clearing the screen, reading lines |
| `bits` | bitwise operations |
| `strbuf` | string builder |
| `thread` | threads and mutexes |

You can include several: `use io; use system;`.

### math

Constants: `PI`, `TAU`, `E`, `PHI`, `SQRT2`, `LN2`, `LN10` (all `double`).

Functions are selected by argument types: `pow(2, 10)` computes an integer, and `pow(2.0, 10.0)` a floating-point one. This is the same overloading as in your own functions.

| Group | Functions | Arguments → result |
|---|---|---|
| Absolute value | `abs(x)` | `int`, `long`, `double` → same type |
| | `fabs(x)` | `float`, `double` → same type |
| Powers and roots | `pow(x, n)` | `int`, `long`, `float`, `double` → same type (an integer power with negative `n` gives `0`) |
| | `sqrt(x)` | `int` → `double`; `float` → `float`; `double` → `double` |
| | `cbrt(x)` | `float`, `double` → same type |
| | `hypot(a, b)` | `float`, `double` → same type |
| Rounding | `ceil(x)`, `floor(x)`, `round(x)`, `trunc(x)` | `float`, `double` → same type |
| Remainder | `fmod(a, b)` | `float`, `double` → same type (sign like `a`) |
| | `mod(a, b)` | `int` → `int` (sign like `b`) |
| Trigonometry | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh` | `float`, `double` → same type |
| | `atan2(y, x)` | `float`, `double` → same type |
| Exponent and logarithms | `exp`, `log`, `log2`, `log10` | `float`, `double` → same type |
| | `log(x, base)` | `double, double` → `double` |
| Min., max. | `min(a, b)`, `max(a, b)` | `int`, `long`, `float`, `double` → same type |
| | `clamp(x, lo, hi)` | `int`, `long`, `float`, `double` → same type |
| | `sign(x)` | `int`, `long`, `float`, `double` → `int` (`-1`, `0`, `1`) |
| Integers | `gcd(a, b)`, `lcm(a, b)` | `int`, `long` → same type |
| | `factorial(n)` | `int` → `long` (gives `-1` for `n < 0` or `n > 20`) |
| | `is_prime(n)` | `int`, `long` → `bool` |
| Checks | `is_nan(x)`, `is_inf(x)`, `is_finite(x)` | `float`, `double` → `bool` |
| | `inf()`, `nan()` | → `double` |
| Angles | `to_rad(x)`, `to_deg(x)` | `float`, `double` → same type |
| Interpolation | `lerp(a, b, t)` | `float`, `double` → same type |

> [!TIP]
> If there is no exact version of a function for the given argument types, the compiler picks the nearest one: `sin(1)` and `exp(2)` work even though the argument is an integer.

### random

A random number generator. By default it starts from the current time, so numbers differ on every run. To get a repeatable sequence, set a seed.

| Function | Result | What it does |
|---|---|---|
| `set_seed(n)` | — | sets the generator seed |
| `randi(a, b)` | `int` | random integer from `a` to `b` **inclusive** |
| `rands(a, b)` | `short` | the same for `short` |
| `randl(a, b)` | `long` | the same for `long` |
| `randf(a, b)` | `float` | random floating-point in the range from `a` to `b` |
| `randd(a, b)` | `double` | the same for `double` |

```cpp
use random;

set_seed(42);
writeln randi(1, 6);       // a dice roll
writeln randd(0.0, 1.0);
```

### io

Working with files. There are two ways: short functions that only need a path, and the `file` type for step-by-step reading and writing.

**Path functions**

| Function | Result | What it does |
|---|---|---|
| `file_exists(path)` | `bool` | does the file exist? |
| `file_size(path)` | `long` | size in bytes, `-1` on error |
| `file_read(path)` | `string` | the whole file content (empty string on error) |
| `file_write(path, text)` | `bool` | writes text, overwriting the file |
| `file_append(path, text)` | `bool` | appends text to the end |
| `file_lines(path)` | `list<string>` | the file's lines |
| `file_delete(path)` | `bool` | deletes the file |
| `file_rename(from, to)` | `bool` | renames |
| `io_error()` | `string` | text of the last error |

```cpp
use io;

file_write("notes.txt", "first\nsecond\n");
list<string> lines = file_lines("notes.txt");
writeln lines.length();      // 2
```

**The file type**

A file is opened with the function `file_open(path, mode)`. The modes are the same as in C: `"r"` read, `"w"` write with truncation, `"a"` append, `"rb"`, `"wb"`, and others. If the file couldn't be opened, the object turns out to be "closed": check that with `is_open()`, and find out the reason via `io_error()`.

```cpp
use io;

file f = file_open("log.txt", "w");
if (!f.is_open())
{
    writeln io_error();
}
else
{
    f.putln("entry 1");
    f.putln("entry 2");
    f.close();
}
delete f;
```

Ready-made standard streams: `file_stdin()`, `file_stdout()`, `file_stderr()`.

| Method | Result | What it does |
|---|---|---|
| `close()` | `bool` | closes the file |
| `is_open()` | `bool` | is the file open? |
| `eof()` | `bool` | nothing left to read? (true right after reading the last line) |
| `flush()` | `bool` | flushes the buffer to disk |
| `path()`, `mode()` | `string` | path and mode |
| `get_line()` | `string` | the next line without the line break |
| `get_char()` | `char` | the next character (`'\0'` at the end) |
| `read(n)` | `string` | up to `n` bytes |
| `read_all()` | `string` | everything up to the end of the file |
| `lines()` | `list<string>` | all remaining lines |
| `put(s)`, `putln(s)` | `bool` | writes a string (`putln` also adds a line break) |
| `put_char(c)` | `bool` | writes a character |
| `size()` | `long` | file size |
| `tell()` | `long` | current position |
| `seek(pos)` | `bool` | jump to a position from the beginning |
| `rewind()` | — | go back to the beginning |

Reading a file line by line is convenient like this:

```cpp
file f = file_open("notes.txt", "r");
while (!f.eof())
{
    writeln f.get_line();
}
delete f;
```

`delete f;` closes the file if it is still open and frees memory. For the standard streams, `delete` is safe and does nothing.

### system

| Function | Result | What it does |
|---|---|---|
| `system(cmd)` | `int` | runs a command, returns its exit code |
| `system_output(cmd)` | `string` | runs a command and returns everything it printed |
| `exit(code)` | — | exits the program with a code |
| `abort()` | — | aborts the program |
| `arg_count()` | `int` | how many command-line arguments (including the program name) |
| `arg(i)` | `string` | argument number `i` (`arg(0)` is the program name) |
| `args()` | `list<string>` | all arguments as a list |
| `get_env(name)` | `string` | environment variable (empty string if absent) |
| `has_env(name)` | `bool` | is the environment variable set? |
| `set_env(name, value)` | `bool` | sets an environment variable |
| `sleep(sec)`, `sleep_ms(ms)` | — | pause in seconds / milliseconds |
| `ticks_ms()` | `long` | monotonic timer in milliseconds (for measuring how long code runs) |
| `get_platform()` | `string` | `"windows"` or `"linux"` |
| `get_cpu_count()` | `int` | number of processors |
| `get_process_id()` | `int` | process ID |
| `get_hostname()`, `get_username()` | `string` | computer and user name |
| `cwd()` | `string` | current folder |
| `chdir(path)`, `mkdir(path)`, `rmdir(path)` | `bool` | change, create, delete a folder |
| `is_dir(path)` | `bool` | is this a folder? |

```cpp
use system;

long start = ticks_ms();
sleep_ms(250);
writeln ticks_ms() - start;     // about 250

for (int i = 1; i < arg_count(); i++)
    writeln arg(i);
```

### date

The library introduces two types: `date` (a calendar date) and `time` (time of day down to the second). Objects are created with functions and freed with `delete`.

**"Now"** (read like variables, no parentheses):

| Variable | What it is |
|---|---|
| `year_now`, `month_now`, `day_now` | current year, month, day |
| `hour_now`, `min_now`, `sec_now` | current hour, minute, second |
| `weekday_now` | day of the week: 1 — Monday, 7 — Sunday |
| `yday_now` | day number in the year |
| `unix_now` | seconds since January 1, 1970 (`long`) |

**Creation** (with invalid values you get an "empty" object whose methods are safe and return zeros and empty strings):

| Function | What it does |
|---|---|
| `date_new(y, m, d)` | a date from year, month, and day |
| `date_now()` | today's date |
| `date_parse("2026-11-01")` | a date from a `YYYY-MM-DD` string |
| `date_from_days(n)` | a date by the number of days since January 1, 1970 |
| `date_valid(y, m, d)` | `bool`: does such a date exist |
| `time_new(h, m, s)` | a time from hours, minutes, seconds |
| `time_now()` | the current time |
| `time_parse("23:30:15")` | a time from a `HH:MM:SS` or `HH:MM` string |
| `time_from_seconds(n)` | a time by the number of seconds since midnight |

**date methods**

| Method | Result | What it does |
|---|---|---|
| `year()`, `month()`, `day()` | `int` | parts of the date |
| `weekday()` | `int` | 1 — Monday ... 7 — Sunday |
| `day_of_year()`, `days_in_month()` | `int` | day number in the year, number of days in the month |
| `is_leap()` | `bool` | is the year a leap year? |
| `to_days()` | `long` | days since January 1, 1970 |
| `month_name()`, `weekday_name()` | `string` | English names, in lower case |
| `add_days(n)`, `add_months(n)`, `add_years(n)` | — | shifts the date **in place** |
| `copy()` | `date` | an independent copy |
| `diff_days(other)` | `long` | how many days between the dates (`this - other`) |
| `compare(other)` | `int` | `-1`, `0`, or `1` |
| `equals(other)`, `is_before(other)`, `is_after(other)` | `bool` | comparison |
| `to_str()` | `string` | `YYYY-MM-DD` |
| `format(f)` | `string` | formatting by a template |

The `format` template for a date: `%Y` year, `%y` two-digit year, `%m` month, `%d` day, `%j` day of the year, `%B` month name, `%A` day of the week.

**time methods**

| Method | Result | What it does |
|---|---|---|
| `hour()`, `minute()`, `second()` | `int` | parts of the time |
| `to_seconds()` | `int` | seconds since midnight |
| `add_seconds(n)`, `add_minutes(n)`, `add_hours(n)` | — | shifts the time **in place** (wraps around midnight) |
| `copy()` | `time` | an independent copy |
| `diff_seconds(other)` | `int` | difference in seconds |
| `compare(other)`, `equals(other)`, `is_before(other)`, `is_after(other)` | `int` / `bool` | comparison |
| `to_str()` | `string` | `HH:MM:SS` |
| `format(f)` | `string` | template: `%H` hours, `%M` minutes, `%S` seconds |

```cpp
use date;

date d = date_new(2026, 11, 1);
d.add_days(30);
writeln d.to_str();               // 2026-12-01
writeln d.format("%d.%m.%Y");     // 01.12.2026
writeln d.weekday_name();         // tuesday

time t = time_new(23, 30, 0);
t.add_minutes(45);
writeln t.to_str();               // 00:15:00

writeln $"It is now the year {year_now}";
delete d;
delete t;
```

> [!NOTE]
> `add_months` doesn't spill past the end of the month: `January 31 + 1 month` gives February `28` (or `29`).

### console

The `console` library consists of **static** functions: they are called as `console.name(...)`. Colors are set with ANSI codes; on Windows the compiler enables their support itself.

| Call | Result | What it does |
|---|---|---|
| `console.error(s)`, `console.errorln(s)` | — | prints to the error stream (without / with a line break) |
| `console.flush()` | — | flushes the output buffer |
| `console.read_line()` | `string` | reads a whole line (with spaces) |
| `console.read_char()` | `char` | reads one character |
| `console.clear()` | — | clears the screen |
| `console.color(c)` | — | sets the text color |
| `console.reset()` | — | resets the color |
| `console.beep()` | — | sound signal |
| `console.title(s)` | — | terminal window title |

Color constants: `color_grey`, `color_red`, `color_green`, `color_yellow`, `color_blue`, `color_purple`, `color_cyan`, `color_white` (numbers from 0 to 7).

```cpp
use console;

console.color(color_green);
writeln "Done!";
console.reset();

write "What is your name? ";
string name = console.read_line();
writeln $"Hello, {name}!";
```

### bits

Bitwise operations on `int` (32 bits) and `long` (64 bits). The version is chosen by the first argument's type: `popcount(255)` works with `int`, `popcount(255L)` with `long`. Constants: `INT_BITS` (32) and `LONG_BITS` (64).

| Group | Functions | What they do |
|---|---|---|
| Logic | `band(a, b)`, `bor(a, b)`, `bxor(a, b)`, `bnot(a)` | AND, OR, exclusive OR, NOT by bits |
| Shifts | `shl(x, n)` | shift left |
| | `shr(x, n)` | shift right preserving the sign |
| | `ushr(x, n)` | shift right without a sign (zeros on the left) |
| | `rotl(x, n)`, `rotr(x, n)` | circular shift left / right |
| Counting | `popcount(x)` | how many bits are set to 1 |
| | `clz(x)`, `ctz(x)` | leading / trailing zeros (for `0` it equals the type's width) |
| | `bit_length(x)` | how many bits are needed to write the number |
| | `parity(x)` | `bool`: is the number of ones odd |
| Individual bits | `bit_get(x, i)` | `bool`: what bit number `i` holds (0 — the lowest) |
| | `bit_set(x, i)`, `bit_clear(x, i)`, `bit_toggle(x, i)` | set / clear / flip a bit |
| Masks and fields | `bit_mask(n)` | a number whose lowest `n` bits are 1 |
| | `bit_extract(x, pos, len)` | cuts out `len` bits starting at position `pos` |
| Layout | `bswap(x)` | swaps byte order |
| | `bit_reverse(x)` | swaps bit order |
| Powers of two | `is_pow2(x)` | `bool`: is it a power of two? |
| | `next_pow2(x)` | the nearest power of two not less than `x` (`-1` if it doesn't fit) |
| Text | `bin_str(x, width = 0)`, `hex_str(x, width = 0)` | the number as a binary / hexadecimal string, zero-padded to `width` |

An out-of-range bit number or shift amount doesn't break the program: `shl` and `ushr` give `0`, `shr` fills the number with the sign bit, and `bit_set`, `bit_clear`, and `bit_toggle` return the number unchanged. Negative numbers in `bin_str` and `hex_str` are printed as they sit in memory, at the full width of the type.

```cpp
use bits;

writeln popcount(255);          // 8
writeln shl(1, 4);              // 16
writeln hex_str(255);           // ff
writeln bin_str(5, 8);          // 00000101
writeln bin_str(5, width = 8);  // the same with a named argument

int flags = 0;
flags = bit_set(flags, 3);      // 8
writeln bit_get(flags, 3);      // true
```

> [!NOTE]
> The function `bit_mask(n)` always returns a `long`. If you need an `int`, add `.to_int()`.

### strbuf

A string builder. Ordinary concatenation `s = s + x` creates a new string each time, while `strbuf` appends text to one growing buffer, which makes it handy for assembling large text. The `strbuf` type is freed with `delete`.

```cpp
use strbuf;

strbuf sb = sb_new();
sb.addend("Hello");
sb.addend(", ");
sb.addend(42);
sb.addend('!');
writeln sb.to_str();         // Hello, 42!
writeln $"length: {sb.length()}, buffer: {sb}";
delete sb;
```

**Creation**

| Function | What it does |
|---|---|
| `sb_new()` | an empty builder |
| `sb_from(s)` | a builder with initial text |
| `sb_with_capacity(n)` | an empty builder with room reserved for `n` characters |
| `sb.copy()` | an independent copy |

**Appending** (to the end)

| Method | What it does |
|---|---|
| `addend(x)` | appends a string, character, number (`short`, `int`, `long`, `float`, `double`), or `bool` |
| `addend_line(s)` | appends a string plus a line break |
| `addend_repeat(s, n)` | appends the string `s` `n` times |
| `addend_char_repeat(c, n)` | appends the character `c` `n` times |
| `addend_join(list, sep)` | appends the elements of a string list separated by `sep` |

**Editing and inspecting**

| Method | Result | What it does |
|---|---|---|
| `clear()` | — | empties the buffer |
| `insert(pos, s)` | — | inserts a string at position `pos` |
| `remove(pos, len)` | — | deletes `len` characters starting at `pos` |
| `truncate(n)` | — | keeps only the first `n` characters |
| `reverse()` | — | reverses |
| `set_at(i, c)` | — | replaces the character at an index |
| `replace(a, b)` | `int` | replaces all `a` with `b`, returns the number of replacements |
| `length()`, `capacity()` | `int` | length and capacity |
| `is_empty()` | `bool` | is the buffer empty? |
| `at(i)` | `char` | character by index (`'\0'` out of range) |
| `find(s)` | `int` | position of a substring, or `-1` |
| `contains(s)`, `starts_with(s)`, `ends_with(s)` | `bool` | checks |
| `substr(start, len)` | `string` | a piece of the text |
| `to_str()` | `string` | a copy of the text as a regular string |
| `print()`, `println()` | — | prints the buffer without copying |
| `write_to(file)` | `bool` | writes the buffer to a file (needs the `io` library) |

Methods with invalid indexes don't break anything — they simply do nothing. They are also safe for a `null` builder: `length()` gives `0`, `to_str()` an empty string.

### thread

Threads and mutexes. Threads are started with the `spawn` statement: the first argument is the function name, followed by its arguments.

```cpp
use thread;

define hello(string who, int times)
{
    for (int i = 0; i < times; i++)
        writeln $"{who}: {i}";
}

thread t = spawn(hello, "worker", 3);
t.join();            // wait for completion
delete t;
```

Rules of `spawn`:

- the function must be declared **above** and match by argument types (ordinary conversions like `int` into `long` are allowed);
- default values must be passed explicitly, the function's result is discarded;
- `spawn` immediately returns a value of type `thread`, and the thread runs in parallel.

| What | Result | What it does |
|---|---|---|
| `t.join()` | `bool` | waits for the thread to finish |
| `t.is_running()` | `bool` | is the thread still running? |
| `thread_id()` | `long` | number of the current thread |
| `mutex_new()` | `mutex` | creates a mutex |
| `m.lock()`, `m.unlock()` | — | acquire / release |
| `m.try_lock()` | `bool` | try to acquire without waiting |

A mutex protects shared data when several threads access it at once:

```cpp
use thread;

int total = 0;

define work(mutex m, int n)
{
    for (int i = 0; i < n; i++)
    {
        m.lock();
        total++;
        m.unlock();
    }
}

define main() -> int
{
    mutex m = mutex_new();
    thread a = spawn(work, m, 1000);
    thread b = spawn(work, m, 1000);
    a.join();
    b.join();
    writeln total;           // 2000
    delete a;
    delete b;
    delete m;
    return 0;
}
```

> [!WARNING]
> When `main` ends, all threads stop, so do your `join()` in advance. Output, lists, and dictionaries are not protected from simultaneous access: wrap their use in `lock()` / `unlock()`. `delete` on a running thread doesn't stop it, it merely releases it.

### Including your own file

A large program can be spread across files. To pull in another `.fab` file, give the path in quotes:

```cpp
use "libs/helpers.fab";
```

The compiler looks for the file next to your program first, then next to the compiler itself. Each file is included once, even if you write `use` several times.

Included files share a common namespace: their functions, classes, and global variables are visible throughout the program (to avoid confusion, use [`namespace`](#namespaces)). If you need a variable invisible outside its file, declare it with the word `static`:

```cpp
// counter.fab
static int hits = 0;        // available only in counter.fab

define count() -> int
{
    hits++;
    return hits;
}
```

## Low-level features

### size_of

`size_of(type)` or `size_of(value)` returns the size in bytes (the result is of type `long`):

```cpp
writeln size_of(int);        // 4
writeln size_of(long);       // 8
writeln size_of(3.5);        // 8
```

### type

`type(value)` returns the name of the value's type as a string:

```cpp
list<int> a = [1, 2];
writeln type(a);             // list<int>
writeln type(1.5f);          // float
```

### asm

Inline assembly (AT&T syntax). The first argument is the code, the second is LLVM constraints, then come the values:

```cpp
asm("nop");
```

If you need constraints and values, they are appended with commas: `asm("code", "constraints", value1, value2);`.

This feature is for experienced users: the compiler doesn't check what you wrote.

## Mini project: FizzBuzz

Time to put it all together. The classic task: go over the numbers from 1 to 15 and print `Fizz` for multiples of 3, `Buzz` for multiples of 5, and `FizzBuzz` for multiples of both.

```cpp
for (int i = 1; i <= 15; i++)
{
    if i % 15 == 0
    {
        writeln "FizzBuzz";
    }
    else if i % 3 == 0
    {
        writeln "Fizz";
    }
    else if i % 5 == 0
    {
        writeln "Buzz";
    }
    else
    {
        writeln i;
    }
}
```

Here one program uses a `for` loop, a chain of `else if`, the remainder of division `%`, and output.

**Try it yourself:**

- Change the loop bound and make FizzBuzz go up to 100.
- Move the check into a function `define label(int n) -> string` and return the right word from it.
- Print the sum of all numbers from 1 to 100 with a loop and an accumulator variable.
- Collect the FizzBuzz answers into a `list<string>` and print them on one line via `join(" ")`.
- Assemble the result in a `strbuf` and write it to a file with the `io` library.
- Wrap the logic in a `FizzBuzz` class with a static method `label`.
- Compute FizzBuzz for 1-100 in two threads, half the range each.

## Cheat sheet

| What you need | How to write it |
|---|---|
| Output with a line break | `writeln expression;` (or just `writeln;`) |
| Output without a line break | `write expression;` |
| Interpolated string | `$"x = {x}, {a + b}"` |
| Variable | `int x = 5;` |
| Type from value | `auto x = 5;` |
| Character | `char c = 'a';` |
| Constant | `const int N = 5;` |
| Assignment | `x = 10;`, `x += 1;`, `x++;` |
| Input | `input_in x;` |
| List | `list<int> a = [1, 2, 3];` |
| Dictionary | `dict<string, int> d = {"a": 1};` |
| List element / string character | `a[0]`, `s[0]`, `a[0] = 5;` |
| Dictionary value | `d["a"]`, `d.get("a")`, `d["a"] = 2;` |
| Method | `value.method(arguments)` |
| Number into a string | `n.to_str()` or `$"{n}"` |
| Condition | `if a > b { ... } else { ... }` |
| Chain of conditions | `else if ... { ... }` |
| while loop | `while (condition) { ... }` |
| do-while loop | `do { ... } while (condition);` |
| for loop | `for (int i = 0; i < n; i++) { ... }` |
| Loop over elements | `for (string s in names) { ... }` |
| Exit the loop | `break;` |
| Next loop step | `continue;` |
| Function | `define f(int a) -> int { return a; }` |
| Default argument | `define f(int a, int b = 1)` |
| Argument by name | `f(1, b = 2)` |
| Stop the program | `throw "message";` |
| Class | `class A { public: int x; A(int v) { this.x = v; } }` |
| Inheritance | `class B : A { ... }` |
| Create / delete an object | `A a = new A(1);` / `delete a;` |
| Static call | `Class.method(...)` |
| Namespace | `namespace n { ... }`, `n::f()`, `use namespace n;` |
| File-level global variable | `static int x = 0;` |
| Include a library | `use math;` |
| Include a file | `use "path/file.fab";` |
| Start a thread | `thread t = spawn(f, 1, 2);` |
| Comments | `// ...` and `/* ... */` |

## Common errors

The compiler reports code errors in English: for each one it gives the file, line, column, and the underlined spot. Build errors (for example, at linking) start with `Compile error:`. Here is what the most frequent ones mean:

| Message | What happened | How to fix |
|---|---|---|
| `You miss the ;` | a semicolon is missing | add `;` at the end of the statement |
| `Variable {x} not found!` | the variable isn't declared or went out of scope | declare it above or inside the needed braces |
| `Cannot assign to const variable 'x'!` | an attempt to change a constant | remove `const` or don't change the value |
| `Cannot modify const variable 'x'!` | `++` or `--` on a constant | the same |
| `Cannot assign to library const 'X'!` | an attempt to change a library constant (`PI`, `INT_MAX`...) | library constants cannot be changed |
| `Library variable 'x' is read-only!` | an attempt to assign to a "now" variable (`year_now`...) | they are read-only |
| `Cannot declare a variable of type 'void'!` | a variable of type `void` | choose a real type |
| `'auto' needs an initializer: auto x = ...;` | `auto` has no value (or it's `null`) | give it an initial value |
| `Global 'x' is already defined!` | a global variable is declared twice | use a different name |
| `'break' outside of a loop!` | `break` outside a loop | use it only inside loops |
| `'continue' outside of a loop!` | `continue` outside a loop | the same |
| `Unknown function or overload for codegen: name(...)` | no such function, or the argument types don't fit | check the name, `use`, and argument types (the passed types are shown in parentheses) |
| `Ambiguous call to 'name'` | several functions fit the call equally well | convert the arguments to the needed type explicitly (`to_double()` and so on) |
| `No overload of 'name' matches these arguments` / `... accepts these argument types` | with named arguments, no matching version was found | check the argument names and types |
| `Positional argument after named argument!` | a positional argument comes after a named one | put named arguments at the end |
| `Parameter without default value after parameter with default!` | a positional argument after one with a value | put arguments with values at the end |
| `Type 'X' has no method 'name'` | the type has no such method | check the tables in the "Type methods" section |
| `Type 'X' has no method 'name' with these arguments` | the method exists but with other arguments | check the number and types of arguments |
| `Class 'X' has no method 'name' with these arguments` | the class has no such method (or the arguments didn't match exactly) | check the name and types |
| `Method 'name' is private!` / `Field 'x' of class 'X' is private!` | accessing a private member from outside the class | add `public:` or access it from inside |
| `Class 'X' has no field 'name'!` | no such field | check the field name |
| `Unknown parent class 'X'!` | inheriting from a nonexistent class | check the name |
| `Class 'X' has no constructor!` | passing arguments to `new` on a class without a constructor | add a constructor or drop the arguments |
| `Wrong argument count for constructor of 'X'!` | the wrong number of arguments in `new` | check the constructor declaration |
| `Constructor of 'P' takes N argument(s): write ': P(...)' ...` | the parent has a constructor with arguments | call it after the colon |
| `'super' in a class without parent!` | `super(...)` in a class without a parent | remove the call |
| `Override of 'm' changes return type!` | the overridden method has a different return type | return the same type |
| `Destructor name must match class name!` | the name after `~` didn't match the class | write `~ClassName()` |
| `Cannot print an object of class 'X'` | an attempt to print an object | print its fields |
| `Cannot delete a value of type 'X'!` | `delete` on a value of a regular type | `delete` works with objects, lists, dictionaries, and library types |
| `'static' at top level must be followed by a variable declaration!` | `static` before something other than a variable | `static` may only precede a global variable declaration |
| `Library '...' not found!` | no library file | check the name in `use` and the `lib` folder next to the compiler |
| `Module '...' not found ...` | the file from `use "..."` wasn't found | check the path: it is searched next to the program and next to the compiler |
| `Unknown namespace 'x'!` | `use namespace` for an unknown namespace | declare the namespace above |
| `Namespace name 'std' is reserved!` | a namespace named `std` | choose another name |
| `Token ... does not match ...` | a different character stands where one is required | most often a missing parenthesis, for example in `while` |
| `Unknown expression!` | the compiler didn't understand the expression | check for typos and extra characters |
| `Unknown type for codegen: ...` | no such type | check the type name (for example, `byte` doesn't exist) |
| `null has no standalone value` | `null` used outside an assignment (for example, `x == null`) | comparisons with `null` aren't supported yet |
| `'+' needs a string on both sides: use $"...{x}..." or x.to_str()` | a number or character is added to a string | use `$"..."` or `to_str()` |
| `Empty expression in f-string!` | empty `{}` in a template | write an expression or remove the braces |
| `Single '}' in f-string: write '}}'!` | a lone `}` in a template | double it: `}}` |
| `Unterminated f-string!` | the template isn't closed with a quote or a brace | check `"` and `{ }` |
| `Cannot put a value of type 'X' into an f-string!` | a list, dictionary, or object was interpolated | interpolate `join()` or an object field |
| `foreach works only with list, dict and string, not 'X'!` | `for ... in` over an unsuitable value | only lists, dictionaries, and strings can be walked |
| `foreach: cannot convert element type 'A' to 'B'!` | the loop variable's type doesn't fit the elements | use `auto` or a suitable type |
| `Cannot throw null!` | `throw null;` | throw a string or a value |
| `Increment/Decrement requires a variable, list element or field!` | `++` on something that can't be changed | apply it to a variable, a list element, or a field |
| `spawn: function 'f' with these arguments not found ...` | `spawn` found no matching function | declare the function above and check the types |
| `Incorrect notation of a real number!` | two dots in a number, for example `1.2.3` | fix the number |
| `Missing close tag: '*/'!` | an unterminated block comment | add `*/` |
| `Link failed!` | the executable couldn't be built | check that the `lib` and `tools` folders sit next to the compiler |

While the program runs, the errors `Index out of range`, `Division by zero`, and `Null reference: ...` are possible (see [Runtime errors](#runtime-errors)).

## Not yet supported

Version 0.4 is not the end of the road. Not yet supported:

- the exponentiation operator `^` and bitwise operators (use `pow` from `math` and the `bits` library);
- error handling with `try` / `catch` (there is only `throw`, which stops the program);
- comparison with `null` (`x == null`);
- concatenating a string with a number or character via `+` (use `$"..."` or `to_str()`);
- nested lists and dictionaries (`list<list<int>>`);
- compound assignment for list elements and fields via dot (`a[i] += 1`, `obj.x += 1`; use `++` or the full form);
- the `byte` type;
- reading a whole line via `input_in` (there is `console.read_line()`);
- your own templates (generics);
- sets, queues, and other collections.

## Known issues

- Memory for strings, lists, and dictionaries is not freed automatically (strings are never freed at all). For short programs this is fine, but in long loops free unneeded lists and dictionaries with `delete`.
- Reading a string character by index `s[i]` doesn't check bounds, unlike `at(i)`.
- Floating-point numbers inside lists and dictionaries are printed with six digits after the decimal point.
- Output from several threads at once may get interleaved: protect it with a mutex.
- `bit_mask(n)` always returns a `long`.
- In quoted strings, a backslash before an ordinary character stays as is, while the combination `\\` gives two backslashes.

## Version history

| Version | What's new |
|---|---|
| 0.4 | **language**: `++` and `--` operators (for variables, list elements, and fields); template strings `$"...{x}..."`; string comparison via `==`, `!=`, `<`, `>`, `<=`, `>=`; the `for (x in ...)` loop over lists, dictionaries, and strings; namespaces `namespace`, `use namespace`, and `::`; the `throw` statement; named arguments `f(b = 2)`; default values for methods and constructors; initial class field values; assigning a field by its bare name inside a method; reading and writing `d[key]` in dictionaries with any keys; `writeln;` without a value; concatenation of adjacent string literals; the escape sequences `\0` and `\a`. **Reliability**: bounds checking for `a[i]`, division-by-zero checking, null-reference checking, compiler warnings; error messages with the file, line, and underlined code, several errors per run. **Libraries**: `bits` (bitwise operations), `strbuf` (string builder), `thread` (threads via `spawn` and mutexes). **Speed**: faster output via `write` / `writeln` and faster string handling |
| 0.3 | **classes**: fields, methods, constructors and destructors, `public` / `private`, inheritance and virtual methods, `super`, static methods, `new` and `delete`; global variables and `static` for file-level variables; `auto`; the `long double` type; number suffixes (`7s`, `10L`, `1.5f`, `2d`, `1.5L`), the `_` separator and the `1e6` exponent in numbers; default values for function arguments; writing `a[i] = x` and reading `d[key]` for numeric keys; an empty list `[]` and typeless lists; `bool` printed as `true` / `false`; new libraries `random`, `io` (the `file` type), `system`, `date` (the `date` and `time` types), and `console`; `size_of`, `type`, and `asm`; compiler options `-o`, `-O0`-`-O3`, `-Os`, `-Oz`, `-c`, `-S`, `--emit-llvm`, `--check`, `-r`, `--keep-obj`, `--cpu`, `-V`; `use "file.fab"` searches next to the program first |
| 0.2 | the `char`, `list<T>`, and `dict<K,V>` types with `[...]` and `{...}` literals; methods on all types; indexing lists and strings `a[i]`; `input_in` input for all simple types and strings; the `math` library extended (constants, trigonometry, logarithms, `min`/`max`/`clamp`, `gcd`/`lcm`, value checks, and more); limit constants `INT_MAX` and others |
| 0.1 | first version: data types, variables and constants, conditionals, loops, functions with overloading, input and output, the `math` library, including modules, compiling for Windows and Linux |
