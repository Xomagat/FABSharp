# FAB#

*Programming Language Documentation · Version 0.1*

> [!WARNING]
> FAB# is under active development. In version 0.1, the syntax and feature set are still subject to change, so please check the documentation when updating the compiler.

## Sections

- [Introduction](#introduction)
- [Quick Start](#quick-start)
- [Output](#output)
- [Variables and Types](#variables-and-types)
- [Operators](#operators)
- [Conditionals](#conditionals)
- [Loops](#loops)
- [Functions](#functions)
- [Input](#input)
- [Libraries and Modules](#libraries-and-modules)
- [Mini-project: FizzBuzz](#mini-project-fizzbuzz)
- [Cheat Sheet](#cheat-sheet)
- [Common Errors](#common-errors)
- [What's Missing](#whats-missing)
- [Version History](#version-history)

## Introduction

FAB# is a compiled programming language written in C++. You write your program in a `.fab` file, the compiler turns it into a standard executable file, and it runs standalone—without an interpreter or unnecessary dependencies.

The idea is simple: minimal boilerplate. No mandatory headers, classes, or "magic" templates—just open a file, write a line, and you have a program.

| Feature | Details |
|---|---|
| File extension | `.fab` |
| Language version | 0.1 |
| Platforms | Windows and Linux (x86-64) | | Compilation result | Standalone executable file |
| Entry point | `main` function or code directly in the file |
| Statement terminator | `;` |
| Code blocks | `{ ... }` |
| Comments | `// line` and `/* block */` |

## Quick Start

Three steps from an empty folder to a working program:

1. Create a file named `hello.fab` with the code from the example below.
2. Compile it in the terminal: `fab hello.fab`
3. Run the result: `./hello` on Linux or `hello.exe` on Windows.

```cpp
writeln "Hello, World!";
```

The executable file will appear next to the source file. The compiler automatically deletes the intermediate object file.

Compiler options:

| Command | Action |
|---|---|
| `fab file.fab` | Compiles the program |
| `fab --help` or `fab -h` | Shows help |
| `fab --version` or `fab -v` | Shows version |

> [!TIP]
> Want a "proper" program with a `main` function? Check out the [Functions](#functions) section. However, it's not required for short scripts: you can write code directly in the file, and it will execute from top to bottom.

## Output

There are two commands for output. They are almost identical, differing only by one thing: the line break.

| Command | Behavior |
|---|---|
| `writeln` | Prints the value and moves to a new line |
| `write` | Prints the value and stays on the same line |

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

### What it can print

| Value | Example | Output |
|---|---|---|
| Integer | `writeln 42;` | `42` |
| Floating-point number | `writeln 3.14;` | `3.140000` (six decimal places) |
| Boolean | `writeln true;` | `1` (`false` prints as `0`) |
| String | `writeln "Привет";` | `Привет` |

### Special characters in strings

| Notation | Meaning |
|---|---|
| `\n` | Newline |
| `\t` | Tab |
| `\r` | Carriage return |
| `\"` | quotation mark inside a string |

### Comments

Anything the compiler doesn't need to know can be hidden in a comment:

```cpp
// Single-line comment

/*
And this one can span
any number of lines
*/
writeln "Comments don't interfere with the code";
```

## Variables and types

A variable is a named box containing a value. The box's type is defined once upon creation.

| Type | Size | What it stores | Example |
|---|---|---|---|
| `int` | 32 bits | integers from −2,147,483,648 to 2,147,483,647 | `int x = 42;` |
| `long` | 64 bits | very large integers | `long big = 100;` |
| `short` | 16 bits | small integers | `short s = 7;` |
| `byte` | 8 bits | very small integers | `byte b = 1;` |
| `double` | 64 bits | fractional numbers (about 15–17 significant digits) | `double d = 3.14159;` |
| `float` | 32 bits | fractional numbers with lower precision | `float f = 1.5;` |
| `bool` | 1 bit | `true` or `false` | `bool ready = true;` |
| `string` | — | text | `string name = "FAB#";` |
| `void` | — | "nothing": only used as a function return type | `define f() -> void` |

### Creation and assignment

```cpp
int age = 20;
double pi =
``` 3.14159;
string name = "FAB#";
bool ready = true;
int color = #FF8800; // hexadecimal number starts with #
int counter; // no value: defaults to 0

age = 21; // new value
age += 1; // age becomes 22
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
> If you create a numeric or boolean variable without a value, it will default to a zero value (`0` or `false`). It is best to avoid doing this with strings; always assign them an initial value, e.g., `string s = "";`.

### Constants

The keyword `const` makes a value immutable. A constant must be assigned a value immediately upon declaration.

```cpp
const int MAX_PLAYERS = 4;
writeln MAX_PLAYERS; // 4

// MAX_PLAYERS = 10; // compilation error: cannot change a constant
```

### Mixing number types

If you assign a floating-point number to an integer variable (or vice versa), the compiler handles the conversion automatically:

```cpp
double d = 5; // becomes 5.000000
int n = 3.99; // becomes 3: fractional part is discarded, no rounding
```

### Types of values ​​in code

| What | Appearance | Examples |
|---|---|---|
| integer | digits | `0`, `42`, `1000` |
| floating-point number | digits with a dot | `3.14`, `0.5` |
| hexadecimal | `#` and digits/letters a–f | `#FF`, `#1a2b` |
| boolean | keywords | `true`, `false` |
| string | in double quotes | `"Hello"` |
| null value | keyword | `null` (converts to zero of the appropriate type) |

### Where a variable "lives"

A variable created inside `{ ... }` ceases to exist after the closing brace.

```cpp
int x = 1;
if x == 1
{
int y = 2; // y is visible only inside these braces
writeln x + y; // 3
}
```

## Operators

### Arithmetic

| Operator | Function | Example | Result |
|---|---|---|---|
| `+` | addition | `7 + 2` | `9` |
| `-` | subtraction | `7 - 2` | `5` |
| `*` | multiplication | `7 * 2` | `14` |
| `/` | division | `7 / 2` | `3` (integer divided by integer yields an integer) |
| `/` | division | `7.0 / 2` | `3.5` |
| `%` | remainder | `7 % 2` | `1` |

When an expression contains both integers and floating-point numbers, the result is a floating-point number.

Strings can be added together: this concatenates them.

```cpp
string greeting = "Hello, " + "World!";
writeln greeting; // Hello, World!
```

Unary operators: `-x` changes the sign, `+x` does nothing, `!flag` inverts the boolean value.

### Comparison

| Operator | Meaning |
|---|---|
| `==` | equal to |
| `!=` | not equal to |
| `<` | less than |
| `>` | greater than |
| `<=` | less than or equal to |
| `>=` | greater than or equal to |

### Logic

| Operator | Meaning | Example |
|---|---|---|
| `&&` | AND: true if both parts are true | `x > 0 && x < 10` |
| `\|\|` | OR: true if at least one part is true | `x < 0 \|\| x > 10` |
| `!` | NOT: inverts the value | `!ready` |

Evaluation is "lazy": if the answer is already clear from the left part, the right part is not even evaluated.

### Operator Precedence

From highest to lowest:

| Level | Operators |
|---|---|
| 1 | unary `-`, `+`, `!` |
| 2 | `*`, `/`, `%` |
| 3 | `+`, `-` |
| 4 | `<`, `>`, `<=`, `>=` |
| 5 | `==`, `!=` |
| 6 | `&&` |
| 7 | `\|\|` |

Not sure about the order? Use parentheses: `(a + b) * c`.

> [!WARNING]
> There is no exponentiation operator `^` in version 0.1. For powers, use the `pow` function from the `math` library: [Libraries and Modules](#libraries-and-modules). The `++` and `--` operators are also missing for now; write `i += 1;` instead.

## Conditionals

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
writeln "needs improvement";
}
```

A few rules:

- Parentheses around the condition are **optional**: you can use `if x > 5` or `if (x > 5)`.
- Conditions can be combined: `if x > 0 && x < 10`.
- A number also works as a condition: `0` means "false," while anything else is "true."
- If a branch contains only one statement, curly braces can be omitted.

## Loops

| Loop | When to use |
|---|---|
| `while` | repeat while the condition is true |
| `do-while` | same, but the body executes at least once |
| `for` | when the number of repetitions is known |

> [!IMPORTANT]
> In loops, parentheses around the condition are **mandatory**, unlike in `if` statements.

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

Action first, then the check. Therefore, the body executes at least once, even if the condition is false from the start:

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

Three parts inside the parentheses: what to do before starting, when to continue, and what to do after each step.

```cpp
for (int i = 1; i <= 5; i += 1)
{
writeln i;
}
```

Result: `1`, `2`, `3`, `4`, `5`.

### break and continue

| Command | What it does |
|---|---|
| `break;` | immediately exits the loop |
| `continue;` | skips the rest of the current step and proceeds to the next one |

```cpp
int i = 0;
while (true)
{
i += 1; 
if i % 2 == 0 { continue; }   // skip even numbers
if i > 7 { break; }           // exit after 7
writeln i;
}
```

Result: `1`, `3`, `5`, `7`. > [!TIP]
> In a `for` loop, the `continue` command won't get stuck in an infinite loop: it proceeds to the `i += 1` step rather than jumping straight to the condition check.

## Functions

A function is a named block of code that can be called any number of times.

```cpp
define name(type argument1, type argument2) -> resultType
{
// body
}
```

- `define` declares the function.
- The result type is written after the arrow....arrow `->`. If there is no arrow, the function returns nothing (`void`).
- The result is returned using the `return` command.

### Function without a result

```cpp
define greet(string name)
{
writeln "Hello, " + name + "!";
}

greet("FAB#"); // Hello, FAB#!
```

### Function with a result

```cpp
define square(int x) -> int
{
return x * x;
}

writeln square(7); // 49
```

If the body consists of a single statement, curly braces are not required:

```cpp
define twice(int x) -> int
return x * 2;
```

### Multiple arguments

```cpp
define cool_print(string label, int value)
{
writeln label + " done"; 
writeln value;
}

cool_print("Task", 42);
```

### Recursion

A function can call itself:

```cpp
define factorial(int n) -> int
{
if n <= 1 { return 1; }
return n * factorial(n - 1);
}

writeln factorial(5); // 120
```

### Overloading

You can create multiple functions with the same name, provided they have different argument types. The compiler will automatically select the appropriate one:

```cpp
define show(int x)
{
writeln x;
}

define show(string s)
{
writeln "text: " + s;
}

show(5); // 5
show("hi"); // text: hi
```

> [!WARNING]
> Argument types during the call must match the declaration exactly. If only `square(int x)` exists, calling `square(2.5)` will not work: `2.5` is a `double`. Declare a `square(double x)` version or pass an `int`. ### The `main` function

There are two ways to write a program:

| Method | What it looks like |
|---|---|
| Without `main` | commands sit directly in the file and execute from top to bottom |
| With `main` | the entire program lives inside `main`, just like in major languages ​​|

```cpp
define main() -> int
{
writeln "Hello from main!"; 
return 0;
}
```

Writing `return 0;` at the end of `main` is optional: if you forget it, the compiler will return `0` automatically.

> [!IMPORTANT]
> If a file contains `main`, only function declarations (`define`) are allowed outside of it. Standard commands like `writeln` must reside inside functions. Functions must be declared **before** the point where they are called.

## Input

The `input_in` command reads a number from the keyboard directly into a variable. The variable must be created beforehand.

```cpp
int n;
write "Enter a number: ";
input_in n;
writeln n * 2;
```

> [!NOTE]
> In version 0.1, only integers (`int`) can be entered.

## Libraries and modules

### Importing a library

The `use` command imports a library. It must be imported **before** its functions are used for the first time.

```cpp
use math;

writeln sqrt(16.0); // 4.000000
writeln pow(2, 10); // 1024
writeln floor(3.7); // 3.000000
writeln round(2.5); // 3.000000
```

### The `math` library

| Function | Arguments | Result | What it does |
|---|---|---|---|
| `pow(x, n)` | `int, int` | `int` | raises to a power |
| `pow(x, n)` | `long, long` | `long` | raises to a power |
| `pow(x, n)` | `double, double` | `double` | raises to a power |
| `sqrt(x)` | `double` or `int` | `double` | square root |
| `abs(x)` | `int` or `long` | same | absolute value |
| `fabs(x)` | `float` or `double` | same | absolute value of a floating-point number |
| `ceil(x)` | `float` or `double` | same | rounds up |
| `floor(x)` | `float` or `double` | same | rounds down |
| `round(x)` | `float` or `double` | same | rounds to the nearest integer |

> [!TIP]
> The argument type determines which version of the function is used: `pow(2, 10)` calculates an integer result, while `pow(2.0, 10.0)` calculates a floating-point result. This is the same kind of overloading found in your own functions.

### Including a custom file

You can split a large program across multiple files. To include another `.fab` file, specify the path in quotes:

```cpp
use "libs/helpers.fab";
```

Each file is included only once, even if you write `use` multiple times.

> [!NOTE]
> In version 0.1, the path in `use "..."` is relative to the folder containing the compiler itself, not the folder containing the script.

## Mini-project: FizzBuzz

Time to put it all together. A classic problem: iterate through numbers from 1 to 15 and print `Fizz` for multiples of 3, `Buzz` for multiples of 5, and `FizzBuzz` for multiples of both.

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

This program demonstrates the use of a `for` loop, an `else if` chain, the modulo operator `%`, and output operations.

**Try it yourself:**

- Change the loop limit to run FizzBuzz up to 100.
- Move the check into a function `define label(int n) -> string` and return the appropriate word.
- Print the sum of all numbers from 1 to 100 using a loop and an accumulator variable.

## Cheat Sheet

| Requirement | Syntax |
|---|---|
| Output with newline | `writeln expression;` |
| Output without newline | `write expression;` |
| Variable | `int x = 5;` |
| Constant | `const int N = 5;` |
| Assignment | `x = 10;`, `x += 1;` |
| Integer input | `input_in x;` |
| Condition | `if a > b { ... } else { ... }` |
| Conditional chain | `else if ... { ... }` |
| While loop | `while (condition) { ... }` |
| Do-while loop |`do { ... } while (condition);` |
| For loop | `for (int i = 0; i < n; i += 1) { ... }` |
| Exit loop | `break;` |
| Next loop iteration | `continue;` |
| Function | `define f(int a) -> int { return a; }` |
| Import library | `use math;` |
| Import file | `use "path/file.fab";` |
| Comments | `// ...` and `/* ... */` |

## Common errors

The compiler reports errors in English. Here is what they mean:

| Message | What happened | How to fix |
|---|---|---|
| `You miss the ;` | missing semicolon | add `;` at the end of the statement |
| `Variable {x} not found!` | variable not declared or out of scope | declare it earlier or within the appropriate braces |
| `Cannot assign to const variable 'x'!` | attempt to modify a constant | remove `const` or do not change the value |
| `'break' outside of a loop!` | `break` used outside a loop | use it only inside loops |
| `'continue' outside of a loop!` | `continue` used outside a loop | same as above |
| `Unknown function or overload for codegen: name` | function does not exist or argument types do not match | check the name, `use` statement, and argument types |
| `Library '...' not found!` | library file missing | check the name in `use` and the `lib` folder next to the compiler |
| `Token ... does not match ...` | wrong symbol found where another was expected | usually a missing bracket, e.g., in a `while` loop |
| `Unknown expression!` | The compiler did not understand the expression | Check for typos and extra characters |

## What is not yet supported

Version 0.1 is just the beginning. The following are not yet supported:

- the exponentiation operator `^` (use `pow` from `math`);
- the `++` and `--` operators;
- string comparison using `==` and `<`;
- concatenating a string with a number (`"x = " + 5`);
- arrays, structures, and classes;
- input of numbers with fractional parts and strings;
- multiple files sharing state outside of `use "..."`.

## Version history

| Version | What's new |
|---|---|
| 0.1 | Initial version: data types, variables and constants, conditionals, loops, overloaded functions, input/output, `math` library, module importing, compilation for Windows and Linux |