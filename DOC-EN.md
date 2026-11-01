# FAB#

*Programming language documentation · version 0.3*

> [!WARNING]
> FAB# is under active development. In version 0.3 the syntax and the set of features may still change, so check the documentation when you update the compiler.

## Contents

- [Introduction](#introduction)
- [Quick start](#quick-start)
- [Output](#output)
- [Variables and types](#variables-and-types)
- [Operators](#operators)
- [Conditions](#conditions)
- [Loops](#loops)
- [Functions](#functions)
- [Input](#input)
- [Type methods](#type-methods)
- [Lists](#lists)
- [Dictionaries](#dictionaries)
- [Classes](#classes)
- [Libraries and modules](#libraries-and-modules)
- [Low-level features](#low-level-features)
- [Mini project: FizzBuzz](#mini-project-fizzbuzz)
- [Cheat sheet](#cheat-sheet)
- [Common errors](#common-errors)
- [What is not there yet](#what-is-not-there-yet)
- [Known issues](#known-issues)
- [Version history](#version-history)

## Introduction

FAB# is a compiled programming language written in C++ using LLVM. You write a program in a `.fab` file, the compiler turns it into an ordinary executable, and from then on it runs on its own, with no interpreter and no extra dependencies.

The idea is simple: as little ceremony as possible. There are no mandatory headers or "magic" boilerplate: open a file, write a line, get a program. And when the program grows, the language has functions, classes with inheritance, modules and libraries.

| What | How |
|---|---|
| File extension | `.fab` |
| Language version | 0.3 |
| Platforms | Windows and Linux (x86-64) |
| Compilation result | a standalone executable |
| Entry point | the `main` function or code right in the file |
| End of a statement | `;` |
| Code blocks | `{ ... }` |
| Comments | `// line` and `/* block */` |

## Quick start

Three steps from an empty folder to a running program:

1. Create a file `hello.fab` with the code from the example below.
2. Compile it in a terminal: `fab hello.fab`
3. Run the result: `./hello` on Linux or `hello.exe` on Windows.

```cpp
writeln "Hello, World!";
```

The executable appears next to the source file. The compiler deletes the intermediate object file itself.

### Compiler options

```
fab [options] file.fab
```

| Option | What it does |
|---|---|
| `-h`, `--help` | shows help |
| `-v`, `--version` | shows the version |
| `-o <file>` | output file name |
| `-O0` `-O1` `-O2` `-O3` | optimization level (default `-O2`) |
| `-Os`, `-Oz` | optimize for size |
| `-c` | compile to an object file only, do not link |
| `-S` | save assembly (`.s`) |
| `--emit-llvm` | save LLVM IR (`.ll`) |
| `--check` | only check the syntax |
| `-r`, `--run` | run the program after the build |
| `--keep-obj` | keep the object file after linking |
| `--cpu=<name>` | target CPU (default `x86-64-v3`) |
| `-V`, `--verbose` | print the build stages |

For example, `fab -r hello.fab` builds and immediately runs the program, and `fab --check hello.fab` quickly checks the file for syntax errors.

> [!TIP]
> Want a "real" program with a `main` function? See the [Functions](#functions) section. But short scripts do not need it: you can write code right in the file and it runs from top to bottom.

## Output

There are two output commands. They are almost twins and differ in one thing: the line break.

| Command | Behavior |
|---|---|
| `writeln` | prints a value and moves to a new line |
| `write` | prints a value and stays on the same line |

```cpp
write "Hello, ";
write "World!\n";
writeln "Bye!";
```

Result:

```
Hello, World!
Bye!
```

### What can be printed

| Value | Example | What you see |
|---|---|---|
| integer | `writeln 42;` | `42` |
| fractional number | `writeln 0.5;` | `0.5` (up to 17 significant digits, trailing zeros are not printed) |
| boolean | `writeln true;` | `true` (and `false`) |
| character | `writeln 'A';` | `A` |
| string | `writeln "Hello";` | `Hello` |
| list | `writeln [1, 2];` | `[1, 2]` |
| dictionary | `writeln d;` | `{a: 1, b: 2}` |

Objects of your own classes and library types (`file`, `date`, `time`) cannot be printed directly: print their fields or the result of a method (`d.to_str()`).

> [!NOTE]
> Fractional numbers are printed with maximum precision: `double` with up to 17 significant digits, `float` with up to 9, `long double` with up to 21. So `writeln 4.0;` prints `4`, while `writeln 3.14;` may show `3.1400000000000001`. This is a property of binary representation, not an error.

### Escape sequences in strings and characters

| Notation | Meaning |
|---|---|
| `\n` | new line |
| `\t` | tab |
| `\r` | carriage return |
| `\"` | a quote inside a string |
| `\'` | an apostrophe inside a character |

### Comments

Anything the compiler does not need to know can be hidden in a comment:

```cpp
// A single-line comment

/*
   And this one can span
   as many lines as you like
*/
writeln "Comments do not get in the way";
```

## Variables and types

A variable is a box with a name that holds a value. The type of the box is set once, at creation.

| Type | Size | What it holds | Example |
|---|---|---|---|
| `int` | 32 bits | integers from −2,147,483,648 to 2,147,483,647 | `int x = 42;` |
| `long` | 64 bits | very large integers | `long big = 100;` |
| `short` | 16 bits | small integers | `short s = 7;` |
| `double` | 64 bits | fractional numbers (about 15–17 significant digits) | `double d = 3.14159;` |
| `float` | 32 bits | fractional numbers with lower precision | `float f = 1.5;` |
| `long double` | 80 bits | extended-precision fractional numbers (x87) | `long double e = 1.5L;` |
| `bool` | 1 bit | `true` or `false` | `bool ready = true;` |
| `char` | 8 bits | a single character | `char c = 'a';` |
| `string` | — | text | `string name = "FAB#";` |
| `list<T>` | — | a list of values of type `T` | `list<int> nums = [1, 2, 3];` |
| `dict<K,V>` | — | a "key → value" dictionary | `dict<string, int> ages = {"Anna": 20};` |
| `auto` | — | the type is taken from the value on the right | `auto n = 5;` |
| `void` | — | "nothing": only as a function result type | `define f() -> void` |

Besides these, there are types you declare yourself ([classes](#classes)) and library types: `file` from `io`, `date` and `time` from `date`.

### Creation and assignment

```cpp
int age = 20;
double pi = 3.14159;
string name = "FAB#";
bool ready = true;
char grade = 'A';
int color = #FF8800;    // a hexadecimal number starts with #
int counter;            // no value: it will be 0 inside

age = 21;               // a new value
age += 1;               // age is now 22
```

Shorthand assignment forms:

| Notation | Equivalent to |
|---|---|
| `x += 5;` | `x = x + 5;` |
| `x -= 5;` | `x = x - 5;` |
| `x *= 5;` | `x = x * 5;` |
| `x /= 5;` | `x = x / 5;` |
| `x %= 5;` | `x = x % 5;` |

> [!NOTE]
> If you create a numeric or boolean variable without a value, it gets the zero value (`0` or `false`), and a list or dictionary becomes empty. Better not to do this with strings: always give them an initial value, for example `string s = "";`.

### auto

If the type is obvious from the value, you can leave it out. The compiler works it out itself:

```cpp
auto n = 5;                     // int
auto pi = 3.14;                 // double
auto big = 10L;                 // long
auto name = "fab".to_upper();   // string
```

`auto` must have an initial value: `auto x;` and `auto x = null;` will not work.

### Constants

The `const` keyword makes a value unchangeable. A constant must be given a value right away. Only simple types (`int`, `double`, `bool`, `string`, etc.) can be constants, not lists or dictionaries.

```cpp
const int MAX_PLAYERS = 4;
writeln MAX_PLAYERS;      // 4

// MAX_PLAYERS = 10;      // compile error: a constant cannot be changed
```

### Numbers of different types get along

If you put a fractional number into an integer variable (or the other way round), the compiler converts it for you:

```cpp
double d = 5;      // becomes 5
int n = 3.99;      // becomes 3: the fractional part is dropped, no rounding
```

For explicit conversions every type has methods `to_int()`, `to_double()` and others, see [Type methods](#type-methods).

### Kinds of values in code

| What | What it looks like | Examples |
|---|---|---|
| integer | digits | `0`, `42`, `1000` |
| number with separators | `_` between digits | `1_000_000` |
| fractional number | digits with a dot | `3.14`, `0.5` |
| number with an exponent | `e` or `E` | `1e6`, `2.5e-3` |
| hexadecimal | `#` and digits/letters a–f | `#FF`, `#1a2b` |
| boolean | keywords | `true`, `false` |
| character | in single quotes | `'a'`, `'\n'` |
| string | in double quotes | `"Hello"` |
| list | values in square brackets | `[1, 2, 3]` |
| dictionary | `key: value` pairs in curly braces | `{"a": 1, "b": 2}` |
| empty value | keyword | `null` (turns into zero of the needed type, or an empty list/dictionary) |

### Number suffixes

Without a suffix, an integer is an `int` (or `long` if it does not fit) and a fractional number is a `double`. To set the type explicitly, add a letter:

| Suffix | Type | Example |
|---|---|---|
| `s` | `short` | `7s` |
| `L` | `long` (for integers) | `10000000000L` |
| `f` | `float` | `1.5f` |
| `d` | `double` | `2d` |
| `L` | `long double` (for fractional numbers) | `1.5L` |

```cpp
long double e = 2.718281828459045235L;
float f = 0.1f;
writeln size_of(7s);    // 2
```

> [!NOTE]
> A dictionary literal knows its type only when it is put into a variable with an explicit type: `dict<string, int> d = {"a": 1};`. A list literal with elements can also be used on its own (`[1, 2, 3].sum()`), while an empty list `[]` needs an explicit type: `list<int> a = [];`.

### Where a variable "lives"

A variable created inside `{ ... }` disappears after the closing brace.

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
    counter += 1;
}

hit();
hit();
writeln counter;     // 2
```

If you write `static` before the declaration, the variable is visible only in its own file. This is handy in modules so that internal data does not clash with other people's names (see [Including your own file](#including-your-own-file)):

```cpp
static int secret = 42;     // available only in this file
```

You cannot declare a global variable with the same name twice. If the file has a `main`, the values of global variables are computed at the very start of `main`.

## Operators

### Arithmetic

| Operator | What it does | Example | Result |
|---|---|---|---|
| `+` | addition | `7 + 2` | `9` |
| `-` | subtraction | `7 - 2` | `5` |
| `*` | multiplication | `7 * 2` | `14` |
| `/` | division | `7 / 2` | `3` (integer by integer gives an integer) |
| `/` | division | `7.0 / 2` | `3.5` |
| `%` | remainder | `7 % 2` | `1` |

When an expression contains both integers and fractional numbers, the result is fractional. Between two integer types the wider one is used.

Strings can be added: this concatenates them.

```cpp
string greeting = "Hello, " + "World!";
writeln greeting;        // Hello, World!
```

To attach a number to a string, convert it to a string first:

```cpp
int n = 5;
writeln "n = " + n.to_str();    // n = 5
```

Unary operators: `-x` flips the sign, `+x` does nothing, `!flag` inverts a boolean value.

### Comparison

| Operator | Meaning |
|---|---|
| `==` | equal |
| `!=` | not equal |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal |
| `>=` | greater than or equal |

Characters are compared like numbers: `c == 'a'`, `c >= '0'`. Strings cannot be compared with `==` and `<`: use `compare()` and `equals_ignore_case()`.

### Logic

| Operator | Meaning | Example |
|---|---|---|
| `&&` | AND: true if both parts are true | `x > 0 && x < 10` |
| `\|\|` | OR: true if at least one part is true | `x < 0 \|\| x > 10` |
| `!` | NOT: inverts the value | `!ready` |

Evaluation is "lazy": if the answer is already clear from the left part, the right part is not even evaluated.

### Operator precedence

From strongest to weakest:

| Level | Operators |
|---|---|
| 1 | unary `-`, `+`, `!` |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-` |
| 4 | `<`, `>`, `<=`, `>=` |
| 5 | `==`, `!=` |
| 6 | `&&` |
| 7 | `\|\|` |

Not sure about the order? Add parentheses: `(a + b) * c`.

> [!WARNING]
> There is no exponent operator `^` in version 0.3. For powers use the `pow` function from the `math` library. There are no `++` and `--` operators yet either, write `i += 1;`.

## Conditions

The classic "if... else":

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

- Parentheses around the condition are **not required**: both `if x > 5` and `if (x > 5)` work.
- Conditions can be combined: `if x > 0 && x < 10`.
- A number also works as a condition: `0` means "false", anything else is "true".
- If a branch contains a single statement, the curly braces can be omitted.

## Loops

| Loop | When to use |
|---|---|
| `while` | repeat while a condition is true |
| `do-while` | the same, but the body runs at least once |
| `for` | when you know how many times to repeat |

> [!IMPORTANT]
> In loops the parentheses around the condition are **required**, unlike in `if`.

### while

```cpp
int i = 0;
while (i < 3)
{
    writeln i;
    i += 1;
}
```

Result: `0`, `1`, `2`.

### do-while

Action first, check afterwards. So the body runs at least once, even if the condition is false from the start:

```cpp
int i = 10;
do
{
    writeln i;
    i += 1;
} while (i < 5);
```

Result: `10`. Note the `;` after the closing parenthesis of the condition.

### for

Three parts in parentheses: what to do before the start, when to continue, what to do after each step.

```cpp
for (int i = 1; i <= 5; i += 1)
{
    writeln i;
}
```

Result: `1`, `2`, `3`, `4`, `5`.

This is convenient for walking through lists:

```cpp
list<string> names = ["Anna", "Boris", "Vera"];
for (int i = 0; i < names.length(); i += 1)
{
    writeln names[i];
}
```

### break and continue

| Command | What it does |
|---|---|
| `break;` | leaves the loop immediately |
| `continue;` | skips the rest of the step and goes to the next one |

```cpp
int i = 0;
while (true)
{
    i += 1;
    if i % 2 == 0 { continue; }   // skip even numbers
    if i > 7 { break; }           // leave after 7
    writeln i;
}
```

Result: `1`, `3`, `5`, `7`.

> [!TIP]
> In a `for` loop `continue` will not get stuck forever: it goes to the `i += 1` step, not straight to the condition check.

## Functions

A function is a named piece of code that you can call as many times as you like.

```cpp
define name(type argument1, type argument2) -> resultType
{
    // body
}
```

- `define` declares a function.
- The result type is written after the arrow `->`. If there is no arrow, the function returns nothing (`void`).
- The result is returned with the `return` statement.
- Any types can be used for arguments and the result, including `list<int>`, `dict<string, int>`, your own classes and library types.

### A function without a result

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

If the body is a single statement, the curly braces are not needed:

```cpp
define twice(int x) -> int
    return x * 2;
```

### Default values

An argument can have a default value. Then it may be omitted in a call. Such arguments go at the end of the list:

```cpp
define greet(string name, string hello = "Hi")
{
    writeln hello + ", " + name + "!";
}

greet("Anna");              // Hi, Anna!
greet("Boris", "Hello");    // Hello, Boris!
```

A regular argument cannot follow an argument with a default value.

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
> When calling your own function, argument types must match the declaration exactly. If there is only `square(int x)`, the call `square(2.5)` will not work: `2.5` is a `double`. Declare a `square(double x)` version or pass an `int`. Library functions get a relaxation: if there is no exact match, the compiler picks the closest version with the fewest conversions (this is why `sin(1)` works). If several versions fit equally well, you get an `Ambiguous call` error.

### The main function

There are two ways to write a program:

| Way | What it looks like |
|---|---|
| Without `main` | statements sit right in the file and run from top to bottom |
| With `main` | the whole program lives inside `main`, like in bigger languages |

```cpp
define main() -> int
{
    writeln "Hello from main!";
    return 0;
}
```

You do not have to write `return 0;` at the end of `main`: if you forget, the compiler returns `0` itself.

> [!IMPORTANT]
> If the file has a `main`, only declarations may stay outside it: functions (`define`), classes and global variables. Ordinary statements like `writeln` must live inside functions. Functions must be declared **above** the place where you call them (classes can be below).

## Input

The `input_in` command reads a value from the keyboard straight into a variable. The variable must be created beforehand.

```cpp
int n;
write "Enter a number: ";
input_in n;
writeln n * 2;
```

What can be entered:

| Variable type | What is read |
|---|---|
| `int`, `short`, `long` | an integer |
| `float`, `double` | a fractional number (with a dot) |
| `bool` | a number: `0` is `false`, anything else is `true` |
| `char` | one character (spaces and line breaks before it are skipped) |
| `string` | one word up to the first space |

> [!NOTE]
> A whole line with spaces cannot be read with `input_in`. For that, include the `console` library and call `console.read_line()` (see [console](#console)). Lists and dictionaries cannot be entered through `input_in`.

## Type methods

Values have **methods**: small functions that are called with a dot: `value.method(arguments)`. Methods work on variables and on literals, and they can be chained:

```cpp
string s = "  fab#  ";
writeln s.trim().to_upper();      // FAB#
writeln 'x'.to_upper();           // X
writeln "42".to_int() + 1;        // 43
```

Methods of the types `string`, `char`, `bool`, `int`, `short`, `long`, `float`, `double`, `long double`, `list` and `dict` are always available, `use` is not needed for them. Below, `T` is the list element type, `K`/`V` are the dictionary key and value types.

### string

| Method | Result | What it does |
|---|---|---|
| `length()` | `int` | string length |
| `empty()` | `bool` | is the string empty? |
| `at(i)` | `char` | character by index (out of range gives `'\0'`) |
| `first()`, `last()` | `char` | first and last character |
| `find(x)` | `int` | position of the first occurrence of string or character `x`, `-1` if not found |
| `rfind(s)` | `int` | position of the last occurrence, `-1` if none |
| `contains(s)` | `bool` | contains the substring? |
| `count(s)` | `int` | how many times the substring occurs |
| `start_with(s)`, `ends_with(s)` | `bool` | starts / ends with `s` |
| `substr(start, length)` | `string` | a piece of the string |
| `replace(a, b)` | `string` | replaces all `a` with `b` |
| `to_upper()`, `to_lower()` | `string` | case |
| `capitalize()` | `string` | first letter uppercase |
| `trim()` | `string` | removes spaces at the edges |
| `reverse()` | `string` | reverses |
| `repeat(n)` | `string` | repeats `n` times |
| `pad_left(width, fill)`, `pad_right(width, fill)` | `string` | pads with the `fill` character up to `width` |
| `compare(s)` | `int` | like `strcmp`: less than zero, zero or greater than zero |
| `equals_ignore_case(s)` | `bool` | equality ignoring case |
| `is_blank()` | `bool` | only whitespace characters |
| `is_numeric()` | `bool` | only digits |
| `to_int()`, `to_long()`, `to_double()` | number | parses a number from the string |
| `to_bool()` | `bool` | `true` only if the string equals `"true"` |
| `split(sep)` | `list<string>` | splits the string by a separator |

A character of a string can also be taken with square brackets: `s[0]`.

### char

| Method | Result | What it does |
|---|---|---|
| `is_digit()`, `is_alpha()`, `is_alnum()` | `bool` | digit / letter / letter or digit |
| `is_upper()`, `is_lower()` | `bool` | uppercase / lowercase |
| `is_space()`, `is_punct()` | `bool` | whitespace / punctuation |
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
| `sign()` | `int` | `-1`, `0` or `1` |
| `to_bool()`, `to_char()` | `bool` / `char` | conversion |
| `to_short()`, `to_int()`, `to_long()`, `to_float()`, `to_double()`, `to_ldouble()` | number | conversion (not needed for the type itself) |

### float, double, long double

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | the number as a string |
| `is_positive()`, `is_negative()`, `is_zero()` | `bool` | sign |
| `is_nan()`, `is_inf()`, `is_finite()` | `bool` | special values |
| `abs()` | same type | absolute value |
| `sign()` | `int` | `-1`, `0` or `1` |
| `floor()`, `ceil()`, `trunc()`, `round()` | same type | rounding (`round` goes away from zero: `2.5` → `3`) |
| `to_bool()`, `to_char()`, `to_short()`, `to_int()`, `to_long()` | value | conversion; `NaN` gives `0`, numbers that are too large are clamped to the type's limit |
| `to_float()`, `to_double()`, `to_ldouble()` | fractional | conversion between fractional types |

### bool

| Method | Result | What it does |
|---|---|---|
| `to_str()` | `string` | `"true"` or `"false"` |
| `to_bin()` | `string` | `"1"` or `"0"` |
| `to_int()`, `to_short()`, `to_long()`, `to_float()`, `to_double()` | number | `0` or `1` |
| `negate()` | `bool` | NOT |
| `and(b)`, `or(b)`, `xor(b)` | `bool` | AND, OR, exclusive OR |
| `nand(b)`, `nor(b)`, `xnor(b)` | `bool` | negations of the previous ones |
| `implies(b)` | `bool` | "if a, then b" |
| `compare(b)` | `int` | `false < true`: `-1`, `0` or `1` |

> [!NOTE]
> Unlike `&&` and `||`, the `and` and `or` methods always evaluate both parts.

### Limit constants

These constants are available without `use`:

| Constant | Type | Value |
|---|---|---|
| `INT_MIN`, `INT_MAX` | `int` | `int` limits |
| `SHORT_MIN`, `SHORT_MAX` | `short` | `short` limits |
| `LONG_MIN`, `LONG_MAX` | `long` | `long` limits |
| `CHAR_MIN`, `CHAR_MAX` | `int` | `-128` and `127` |
| `FLOAT_MIN`, `FLOAT_MAX`, `FLOAT_EPSILON` | `float` | smallest and largest positive values, machine epsilon |
| `DOUBLE_MIN`, `DOUBLE_MAX`, `DOUBLE_EPSILON` | `double` | the same for `double` |
| `LDOUBLE_MIN`, `LDOUBLE_MAX`, `LDOUBLE_EPSILON` | `long double` | the same for `long double` |

## Lists

A list holds a sequence of values of the same type. Elements can be `int`, `long`, `short`, `char`, `float`, `double`, `string` and `bool`.

```cpp
list<int> nums = [5, 2, 9];
nums.addend(7);                // append to the end
nums.sort();

writeln nums;                  // [2, 5, 7, 9]
writeln nums.length();         // 4
writeln nums[0];               // 2
writeln nums.sum();            // 23

nums[0] = 100;                 // write an element
```

An empty list is created without a value, with `null`, or with `[]`:

```cpp
list<string> words;
words.addend("one");
words.addend("two");
writeln words.join(", ");      // one, two

list<int> empty = [];
```

> [!TIP]
> Do not write `>` and `=` together when closing a type: the lexer reads `list<int>= ...` as `>=`. Put a space or the variable name right away: `list<int> nums = ...`.

> [!IMPORTANT]
> List methods never fail because of a wrong index: `get(100)` on a short list returns the type's "zero" value (`0`, `false` or an empty string), and `set`, `insert` and `remove_at` with a wrong index simply do nothing. But reading and writing with square brackets (`nums[i]`, `nums[i] = x`) does **not** check bounds: a wrong index leads to reading or corrupting foreign memory. If the index may go out of bounds, use `get` and `set`.

### List methods

| Method | Result | What it does |
|---|---|---|
| `addend(x)` | — | adds `x` **to the end** |
| `add(x)` | — | adds `x` **to the beginning** |
| `insert(i, x)` | — | inserts `x` at position `i` |
| `extend(other)` | — | appends all elements of another list to the end |
| `length()` | `int` | number of elements |
| `empty()` | `bool` | is the list empty? |
| `capacity()`, `reserve(n)` | `int` / — | capacity and space reservation |
| `get(i)`, `set(i, x)` | `T` / — | read / write an element |
| `first()`, `last()` | `T` | first and last element |
| `remove_at(i)` | — | removes the element at position `i` |
| `remove(x)` | `bool` | removes the first `x`; `true` if found |
| `pop()`, `pop_first()` | `T` | takes out and removes the last / first |
| `clear()` | — | clears the list |
| `contains(x)` | `bool` | is `x` there |
| `index_of(x)` | `int` | position of the first `x` or `-1` |
| `count(x)` | `int` | how many times `x` occurs |
| `reverse()` | — | reverses in place |
| `swap(i, j)` | — | swaps two elements |
| `sort()`, `sort_desc()` | — | sort ascending / descending |
| `min()`, `max()` | `T` | smallest / largest |
| `sum()` | `T` | sum (concatenation for strings); not available for `char` and `bool` |
| `copy()` | `list<T>` | an independent copy |
| `slice(start, length)` | `list<T>` | a piece of the list |
| `join(sep)` | `string` | joins elements into a string with a separator |

## Dictionaries

A dictionary holds "key → value" pairs and remembers the order in which keys were added. Keys can be `int`, `long`, `short`, `char` and `string`; values can be `int`, `long`, `short`, `char`, `float`, `double`, `string` and `bool`.

```cpp
dict<string, int> ages = {"Anna": 20, "Boris": 31};
ages.set("Vera", 25);

writeln ages.get("Anna");          // 20
writeln ages.get_or("Dima", -1);   // -1
writeln ages.contains("Boris");    // true
writeln ages.length();             // 3
writeln ages;                      // {Anna: 20, Boris: 31, Vera: 25}
```

For dictionaries with numeric or character keys, a value can also be read with square brackets: `d[5]` does the same as `d.get(5)`. For string keys the brackets do not work yet, use `get`. You cannot write a value through brackets, use `set` for that.

The `{...}` literal knows its type only when creating a variable with an explicit type `dict<K, V>`. An empty dictionary is created without a value: `dict<int, string> d;`.

To walk over all pairs, use `key_at` and `value_at`:

```cpp
for (int i = 0; i < ages.length(); i += 1)
{
    writeln ages.key_at(i) + ": " + ages.value_at(i).to_str();
}
```

### Dictionary methods

| Method | Result | What it does |
|---|---|---|
| `set(k, v)` | — | adds a pair or replaces the value of an existing key |
| `merge(other)` | — | copies all pairs of another dictionary into this one |
| `get(k)` | `V` | value by key; if the key is missing, the type's "zero" value |
| `get_or(k, def)` | `V` | value by key or `def` if the key is missing |
| `key_at(i)`, `value_at(i)` | `K` / `V` | key / value by ordinal number |
| `contains(k)` | `bool` | is the key there |
| `contains_value(v)` | `bool` | is the value there |
| `length()`, `empty()`, `capacity()` | `int` / `bool` | size |
| `remove(k)` | `bool` | removes a pair; `true` if the key existed |
| `pop(k)` | `V` | removes a pair and returns its value |
| `clear()` | — | clears the dictionary |
| `keys()`, `values()` | `list<K>` / `list<V>` | lists of keys and values |
| `copy()` | `dict<K,V>` | an independent copy |

## Classes

A class bundles data (fields) with actions on it (methods). Objects are created from a class.

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
        this.value = this.value + 1;
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

### What a class consists of

| Part | How it is written | What it is |
|---|---|---|
| field | `int value;` | a variable inside an object |
| method | `define name(...) -> type { ... }` | a function of the object |
| constructor | `Name(arguments) { ... }` | runs at creation |
| destructor | `~Name() { ... }` | runs on `delete` |
| static method | `static define name(...) { ... }` | called on the class itself |
| visibility | `private:` / `public:` | who may use the following fields and methods |

Visibility applies until the next label. Before the first label everything is `private`. Private fields and methods can only be accessed from inside the class itself, from outside you get a compile error.

### Creating and deleting objects

```cpp
Counter a = new Counter(5);    // an object with a constructor
Counter b;                     // create an object; the no-argument constructor is called if there is one
Counter c = null;              // no object yet

delete a;                      // call the destructor and free the memory
```

After `delete` the variable becomes `null`. Memory of objects is **not** freed automatically: when an object is no longer needed, delete it yourself. Lists, dictionaries and library types stored in class fields are freed by the destructor automatically.

### Fields, methods and this

A field is read with a dot: `c.value` (if it is `public`). Inside a method, the fields of your own object can be read simply by name, but to change a field, and to call other methods of your own object, use `this`:

```cpp
define reset()
{
    this.value = 0;        // write only through this
    this.inc();            // call a method of your own object
}
```

String fields are `""` by default, list and dictionary fields are created empty, the rest are zero.

> [!WARNING]
> In version 0.3 an assignment like `value = 5;` or `value += 1;` by bare name inside a method does not change the field. Always write `this.value = ...`. Compound operators (`+=`, `-=`) for fields through a dot do not work yet either: write `this.value = this.value + 1;`.

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

Methods are virtual: a variable of type `Animal` holding a `Dog` object calls the `Dog` method. When overriding a method you cannot change the result type. From the body of a constructor the parent constructor can also be called with `super(arguments);`.

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

Methods, like functions, can be overloaded by argument types. When calling a method, argument types must match the declaration exactly.

### Null references

If you call a method or access a field on a variable whose value is `null`, the program prints a `Null reference: ...` message and exits with code 1.

> [!NOTE]
> An object cannot be printed whole with `writeln`. Comparing variables with `null` using `==` is not possible yet either.

## Libraries and modules

### Including a library

The `use` command includes a library. Include it **before** the first use of its functions and types.

```cpp
use math;

writeln sqrt(16.0);    // 4
writeln pow(2, 10);    // 1024
writeln floor(3.7);    // 3
writeln round(2.5);    // 3
writeln PI;            // 3.1415926535897931
```

Available libraries:

| Library | What is in it |
|---|---|
| `math` | mathematics |
| `random` | random numbers |
| `io` | files and working with paths |
| `system` | running programs, arguments, environment, folders |
| `date` | dates and time |
| `console` | colors, clearing the screen, reading lines |

You can include several: `use io; use system;`.

### math

Constants: `PI`, `TAU`, `E`, `PHI`, `SQRT2`, `LN2`, `LN10` (all `double`).

Functions are chosen by argument types: `pow(2, 10)` computes an integer, while `pow(2.0, 10.0)` a fractional number. This is the same overloading as for your own functions.

| Group | Functions | Arguments → result |
|---|---|---|
| Absolute value | `abs(x)` | `int`, `long`, `double` → same type |
| | `fabs(x)` | `float`, `double` → same type |
| Powers and roots | `pow(x, n)` | `int`, `long`, `float`, `double` → same type (an integer power with negative `n` gives `0`) |
| | `sqrt(x)` | `int` → `double`; `float` → `float`; `double` → `double` |
| | `cbrt(x)` | `float`, `double` → same type |
| | `hypot(a, b)` | `float`, `double` → same type |
| Rounding | `ceil(x)`, `floor(x)`, `round(x)`, `trunc(x)` | `float`, `double` → same type |
| Remainder | `fmod(a, b)` | `float`, `double` → same type (sign as `a`) |
| | `mod(a, b)` | `int` → `int` (sign as `b`) |
| Trigonometry | `sin`, `cos`, `tan`, `asin`, `acos`, `atan`, `sinh`, `cosh`, `tanh` | `float`, `double` → same type |
| | `atan2(y, x)` | `float`, `double` → same type |
| Exponent and logarithms | `exp`, `log`, `log2`, `log10` | `float`, `double` → same type |
| | `log(x, base)` | `double, double` → `double` |
| Min, max | `min(a, b)`, `max(a, b)` | `int`, `long`, `float`, `double` → same type |
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
> If there is no exact version of a function for the argument types, the compiler picks the closest one: `sin(1)` and `exp(2)` work even though the argument is an integer.

### random

A random number generator. By default it starts from the current time, so the numbers differ on every run. To get a repeatable sequence, set a seed.

| Function | Result | What it does |
|---|---|---|
| `set_seed(n)` | — | sets the generator seed |
| `randi(a, b)` | `int` | a random integer from `a` to `b` **inclusive** |
| `rands(a, b)` | `short` | the same for `short` |
| `randl(a, b)` | `long` | the same for `long` |
| `randf(a, b)` | `float` | a random fractional number in the range from `a` to `b` |
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
| `file_read(path)` | `string` | the whole file contents (an empty string on error) |
| `file_write(path, text)` | `bool` | writes text, overwriting the file |
| `file_append(path, text)` | `bool` | appends text to the end |
| `file_lines(path)` | `list<string>` | lines of the file |
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

A file is opened with `file_open(path, mode)`. The modes are the same as in C: `"r"` read, `"w"` write with truncation, `"a"` append, `"rb"`, `"wb"` and others. If the file could not be opened, the object ends up "closed": check this with the `is_open()` method and find the reason via `io_error()`.

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
| `eof()` | `bool` | is there nothing left to read? (true right after the last line is read) |
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
| `seek(pos)` | `bool` | go to a position from the start |
| `rewind()` | — | return to the start |

This is a handy way to read a file line by line:

```cpp
file f = file_open("notes.txt", "r");
while (!f.eof())
{
    writeln f.get_line();
}
delete f;
```

`delete f;` closes the file if it is still open and frees the memory. For standard streams `delete` is safe and does nothing.

### system

| Function | Result | What it does |
|---|---|---|
| `system(cmd)` | `int` | runs a command, returns the exit code |
| `system_output(cmd)` | `string` | runs a command and returns everything it printed |
| `exit(code)` | — | ends the program with a code |
| `abort()` | — | aborts the program abnormally |
| `arg_count()` | `int` | number of command-line arguments (including the program name) |
| `arg(i)` | `string` | argument number `i` (`arg(0)` is the program name) |
| `args()` | `list<string>` | all arguments as a list |
| `get_env(name)` | `string` | an environment variable (an empty string if none) |
| `has_env(name)` | `bool` | is the environment variable set? |
| `set_env(name, value)` | `bool` | sets an environment variable |
| `sleep(sec)`, `sleep_ms(ms)` | — | pause in seconds / milliseconds |
| `ticks_ms()` | `long` | a monotonic timer in milliseconds (for measuring how long code runs) |
| `get_platform()` | `string` | `"windows"` or `"linux"` |
| `get_cpu_count()` | `int` | number of processors |
| `get_process_id()` | `int` | process id |
| `get_hostname()`, `get_username()` | `string` | computer and user names |
| `cwd()` | `string` | current folder |
| `chdir(path)`, `mkdir(path)`, `rmdir(path)` | `bool` | change, create, remove a folder |
| `is_dir(path)` | `bool` | is it a folder? |

```cpp
use system;

long start = ticks_ms();
sleep_ms(250);
writeln ticks_ms() - start;     // about 250

for (int i = 1; i < arg_count(); i += 1)
    writeln arg(i);
```

### date

The library introduces two types: `date` (a calendar date) and `time` (time of day with one-second precision). Objects are created by functions and freed with `delete`.

**"Now"** (read like variables, without parentheses):

| Variable | What it is |
|---|---|
| `year_now`, `month_now`, `day_now` | current year, month, day |
| `hour_now`, `min_now`, `sec_now` | current hour, minute, second |
| `weekday_now` | day of the week: 1 is Monday, 7 is Sunday |
| `yday_now` | day number in the year |
| `unix_now` | seconds since January 1, 1970 (`long`) |

**Creation** (for invalid values an "empty" object is returned, whose methods are safe and give zeros and empty strings):

| Function | What it does |
|---|---|
| `date_new(y, m, d)` | a date from year, month and day |
| `date_now()` | today's date |
| `date_parse("2026-11-01")` | a date from a `YYYY-MM-DD` string |
| `date_from_days(n)` | a date from the number of days since January 1, 1970 |
| `date_valid(y, m, d)` | `bool`: does such a date exist |
| `time_new(h, m, s)` | a time from hours, minutes and seconds |
| `time_now()` | current time |
| `time_parse("23:30:15")` | a time from a `HH:MM:SS` or `HH:MM` string |
| `time_from_seconds(n)` | a time from the number of seconds since midnight |

**date methods**

| Method | Result | What it does |
|---|---|---|
| `year()`, `month()`, `day()` | `int` | parts of the date |
| `weekday()` | `int` | 1 is Monday ... 7 is Sunday |
| `day_of_year()`, `days_in_month()` | `int` | day number in the year, number of days in the month |
| `is_leap()` | `bool` | is it a leap year? |
| `to_days()` | `long` | days since January 1, 1970 |
| `month_name()`, `weekday_name()` | `string` | names in English, in lowercase |
| `add_days(n)`, `add_months(n)`, `add_years(n)` | — | shifts the date **in place** |
| `copy()` | `date` | an independent copy |
| `diff_days(other)` | `long` | how many days between dates (`this - other`) |
| `compare(other)` | `int` | `-1`, `0` or `1` |
| `equals(other)`, `is_before(other)`, `is_after(other)` | `bool` | comparison |
| `to_str()` | `string` | `YYYY-MM-DD` |
| `format(f)` | `string` | formatting by a template |

The `format` template for a date: `%Y` year, `%y` two-digit year, `%m` month, `%d` day, `%j` day of year, `%B` month name, `%A` weekday name.

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

writeln year_now;
delete d;
delete t;
```

> [!NOTE]
> `add_months` does not spill past the end of a month: `January 31 + 1 month` gives February `28` (or `29`).

### console

The `console` library consists of **static** functions: they are called as `console.name(...)`. Colors are set with ANSI codes; on Windows the compiler enables their support itself.

| Call | Result | What it does |
|---|---|---|
| `console.error(s)`, `console.errorln(s)` | — | prints to the error stream (without / with a line break) |
| `console.flush()` | — | flushes the output buffer |
| `console.read_line()` | `string` | reads a whole line (with spaces) |
| `console.read_char()` | `char` | reads one character |
| `console.clear()` | — | clears the screen |
| `console.color(c)` | — | turns on a text color |
| `console.reset()` | — | resets the color |
| `console.beep()` | — | a beep |
| `console.title(s)` | — | the terminal window title |

Color constants: `color_grey`, `color_red`, `color_green`, `color_yellow`, `color_blue`, `color_purple`, `color_cyan`, `color_white` (numbers from 0 to 7).

```cpp
use console;

console.color(color_green);
writeln "Done!";
console.reset();

write "What is your name? ";
string name = console.read_line();
writeln "Hello, " + name + "!";
```

### Including your own file

A big program can be split across files. To pull in another `.fab` file, give its path in quotes:

```cpp
use "libs/helpers.fab";
```

The compiler looks for the file first next to your program, and then next to the compiler itself. Each file is included once, even if you write `use` several times.

Included files share a common namespace: their functions, classes and global variables are visible in the whole program. If you need a variable that is invisible outside the file, declare it with `static`:

```cpp
// counter.fab
static int hits = 0;        // available only in counter.fab

define count() -> int
{
    hits += 1;
    return hits;
}
```

## Low-level features

### size_of

`size_of(type)` or `size_of(value)` returns the size in bytes (a result of type `long`):

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

If you need constraints and values, add them after commas: `asm("code", "constraints", value1, value2);`.

This feature is meant for experienced users: the compiler does not check what you wrote.

## Mini project: FizzBuzz

Time to put it all together. A classic task: go through the numbers from 1 to 15 and print `Fizz` for multiples of 3, `Buzz` for multiples of 5 and `FizzBuzz` for multiples of both.

```cpp
for (int i = 1; i <= 15; i += 1)
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

This one program uses the `for` loop, an `else if` chain, the remainder `%` and output.

**Try it yourself:**

- Change the loop bound and make FizzBuzz up to 100.
- Move the check into a function `define label(int n) -> string` and return the right word from it.
- Print the sum of all numbers from 1 to 100 using a loop and an accumulator variable.
- Put the FizzBuzz answers into a `list<string>` and print them in one line with `join(" ")`.
- Write the result to a file using the `io` library.
- Wrap the logic in a class `FizzBuzz` with a static method `label`.

## Cheat sheet

| What you need | How to write it |
|---|---|
| Output with a line break | `writeln expression;` |
| Output without a line break | `write expression;` |
| Variable | `int x = 5;` |
| Type from the value | `auto x = 5;` |
| Character | `char c = 'a';` |
| Constant | `const int N = 5;` |
| Assignment | `x = 10;`, `x += 1;` |
| Input | `input_in x;` |
| List | `list<int> a = [1, 2, 3];` |
| Dictionary | `dict<string, int> d = {"a": 1};` |
| List element / string character | `a[0]`, `s[0]`, `a[0] = 5;` |
| Dictionary value | `d.get("a")` |
| Method | `value.method(arguments)` |
| Number to string | `n.to_str()` |
| Condition | `if a > b { ... } else { ... }` |
| Condition chain | `else if ... { ... }` |
| while loop | `while (condition) { ... }` |
| do-while loop | `do { ... } while (condition);` |
| for loop | `for (int i = 0; i < n; i += 1) { ... }` |
| Leave a loop | `break;` |
| Next loop step | `continue;` |
| Function | `define f(int a) -> int { return a; }` |
| Default argument | `define f(int a, int b = 1)` |
| Class | `class A { public: int x; A(int v) { this.x = v; } }` |
| Inheritance | `class B : A { ... }` |
| Create / delete an object | `A a = new A(1);` / `delete a;` |
| Static call | `Class.method(...)` |
| File-level global variable | `static int x = 0;` |
| Include a library | `use math;` |
| Include a file | `use "path/file.fab";` |
| Comments | `// ...` and `/* ... */` |

## Common errors

The compiler reports errors in English (prefixed with `Compile error:`). Here is what they mean:

| Message | What happened | How to fix |
|---|---|---|
| `You miss the ;` | a semicolon is missing | add `;` at the end of the statement |
| `Variable {x} not found!` | the variable is not declared or is out of scope | declare it above or inside the right braces |
| `Cannot assign to const variable 'x'!` | an attempt to change a constant | remove `const` or do not change the value |
| `Cannot assign to library const 'X'!` | an attempt to change a library constant (`PI`, `INT_MAX`...) | library constants cannot be changed |
| `Library variable 'x' is read-only!` | an attempt to assign to a "now" variable (`year_now`...) | they are read-only |
| `Cannot declare a variable of type 'void'!` | a variable of type `void` | choose a real type |
| `'auto' needs an initializer: auto x = ...;` | `auto` has no value (or it is `null`) | give an initial value |
| `Global 'x' is already defined!` | a global variable is declared twice | use another name |
| `'break' outside of a loop!` | `break` outside a loop | use it only inside loops |
| `'continue' outside of a loop!` | `continue` outside a loop | the same |
| `Unknown function or overload for codegen: name(...)` | there is no such function, or the argument types do not fit | check the name, `use` and argument types (the passed types are shown in parentheses) |
| `Ambiguous call to 'name'` | several library functions fit the call equally well | cast the arguments to the needed type explicitly (`to_double()` etc.) |
| `Parameter without default value after parameter with default!` | a regular argument after an argument with a default | put arguments with defaults at the end |
| `Type 'X' has no method 'name'` | the type has no such method | check the tables in the "Type methods" section |
| `Type 'X' has no method 'name' with these arguments` | the method exists but with other arguments | check the number and types of arguments |
| `Class 'X' has no method 'name' with these arguments` | the class has no such method (or the arguments did not match exactly) | check the name and types |
| `Method 'name' is private!` / `Field 'x' of class 'X' is private!` | access to a private member from outside the class | add `public:` or access it from inside |
| `Class 'X' has no field 'name'!` | there is no such field | check the field name |
| `Unknown parent class 'X'!` | inheriting from a class that does not exist | check the name |
| `Class 'X' has no constructor!` | passing arguments to `new` of a class without a constructor | add a constructor or remove the arguments |
| `Wrong argument count for constructor of 'X'!` | wrong number of arguments in `new` | check the constructor declaration |
| `Constructor of 'P' takes N argument(s): write ': P(...)' ...` | the parent has a constructor with arguments | call it after the colon |
| `Override of 'm' changes return type!` | the overriding method has a different result type | return the same type |
| `Destructor name must match class name!` | the name after `~` does not match the class | write `~ClassName()` |
| `Cannot print an object of class 'X'` | an attempt to print an object | print its fields |
| `Cannot delete a value of type 'X'!` | `delete` for a value of an ordinary type | `delete` works with objects, lists, dictionaries and library types |
| `'static' at top level must be followed by a variable declaration!` | `static` before something other than a variable | `static` can only precede a global variable declaration |
| `Library '...' not found!` | the library file is missing | check the name in `use` and the `lib` folder next to the compiler |
| `Module '...' not found ...` | the file from `use "..."` was not found | check the path: it is searched next to the program and next to the compiler |
| `Token ... does not match ...` | a different symbol stands where a needed one should be | most often a parenthesis is missing, for example in `while` |
| `Unknown expression!` | the compiler did not understand the expression | check for typos and stray characters |
| `Unknown type for codegen: ...` | there is no such type | check the type name (for example, `byte` does not exist) |
| `null has no standalone value` | `null` used outside an assignment (for example, `x == null`) | comparisons with `null` are not supported yet |
| `Dict literal needs a type context: dict<K,V> x = {...};` | a `{...}` dictionary is written outside a variable declaration | declare `dict<K, V> x = {...};` first |
| `Empty list literal needs a type context: list<T> x = [];` | an empty `[]` without a type | declare `list<T> x = [];` |
| `List literal can only initialize a list<T>!` | a `[...]` list was put into a variable of another type | declare the variable as `list<T>` |
| `List literal has mixed element types: ...` | different types mixed in a list | bring the elements to one type |
| `Type '...' does not support indexing!` | square brackets on a type that does not support them | lists, strings and dictionaries (read only) can be indexed |
| `Cannot assign by index to type '...'!` | writing `d[k] = v` on a dictionary | use `d.set(k, v)` |
| `Only '+' is supported for strings!` | an operator other than `+` used with a string | strings can only be concatenated |
| `Unsupported type for input_in codegen!` | `input_in` for a list, a dictionary or another type | enter values one by one |
| `Link failed!` | the executable could not be built | check that the `lib` and `tools` folders are next to the compiler |

A `Null reference: ...` error is possible while the program runs: you accessed a field or method of an object that does not exist (`null`).

## What is not there yet

Version 0.3 is not the end of the road. The following is not supported yet:

- the exponent operator `^` (use `pow` from `math`);
- the `++` and `--` operators;
- comparing strings with `==` and `<` (use `compare()` and `equals_ignore_case()`);
- comparison with `null` (`x == null`);
- concatenating a string with a number or character (`"x = " + 5`, use `5.to_str()`);
- writing to a dictionary with `d[key] = x` (use `d.set(key, x)`) and reading `d["string"]` by a string key (use `d.get("string")`);
- nested lists and dictionaries (`list<list<int>>`);
- compound assignments for fields (`obj.x += 1`);
- default values for method arguments (ordinary functions have them);
- the `byte` type;
- entering a whole line with `input_in` (there is `console.read_line()`);
- your own templates (generics).

## Known issues

- Strings are printed as they are through the system `printf`, so a `%` character inside a printed string may give a strange result for now. Avoid `%` in text for `write` and `writeln`.
- `write` without a line break works correctly with strings, `int`, `double`, `float`, `bool` and lists. For `char`, `long` and dictionaries use `writeln`.
- Assigning to a field by bare name inside a method (`value = 5;`) changes nothing, write `this.value = 5;`.
- Reading and writing by index with square brackets does not check list bounds.
- `find(char)` returns a wrong result if the character is not in the string; for reliable searching use `find` with a string (`s.find("x")`).
- `substr` with invalid bounds returns a null pointer instead of a string.
- Memory of strings, lists and dictionaries is not freed automatically. For short programs this is fine, but in long loops free unneeded lists and dictionaries with `delete`.

## Version history

| Version | What is new |
|---|---|
| 0.3 | **classes**: fields, methods, constructors and destructors, `public` / `private`, inheritance and virtual methods, `super`, static methods, `new` and `delete`; global variables and `static` for file-level variables; `auto`; the `long double` type; number suffixes (`7s`, `10L`, `1.5f`, `2d`, `1.5L`), the `_` separator and exponent `1e6` in numbers; default values for function arguments; `a[i] = x` writing and `d[key]` reading for numeric keys; an empty list `[]` and lists without a stated type; `bool` prints as `true` / `false`; new libraries `random`, `io` (the `file` type), `system`, `date` (the `date` and `time` types) and `console`; `size_of`, `type` and `asm`; compiler options `-o`, `-O0`–`-O3`, `-Os`, `-Oz`, `-c`, `-S`, `--emit-llvm`, `--check`, `-r`, `--keep-obj`, `--cpu`, `-V`; `use "file.fab"` looks for the file next to the program first |
| 0.2 | `char`, `list<T>` and `dict<K,V>` types with `[...]` and `{...}` literals; methods on all types; list and string indexing `a[i]`; `input_in` for all simple types and strings; `math` library extended (constants, trigonometry, logarithms, `min`/`max`/`clamp`, `gcd`/`lcm`, value checks and more); limit constants `INT_MAX` etc. |
| 0.1 | first version: data types, variables and constants, conditions, loops, functions with overloading, input and output, the `math` library, module inclusion, compilation for Windows and Linux |