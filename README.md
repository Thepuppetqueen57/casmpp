# CASM++
CASM++ is a high-level assembly inspired interpreted language designed to make working with registers easier and more high-level.

CASM++ is a fork of my old project from years ago called [CASM](https://github.com/The-Puppet-Studios/casm).

CASM and CASM++ are both completely vibecoded.

This is because back when I made CASM I vibecoded everything for some reason

And now I just wanna see how far I can get with ai nowadays since its improved so much

If you want to check out something similar that is fully written by humans check out [RASM](https://github.com/Thepuppetqueen57/rasm)

# Compile Guide
Just install clang and gnu-make and then run the makefile

# Documentation
## Out
To print something you can use the out function. Heres an example:

```
out "Welcome to CASM!";
```

## Variables
There are 3 datatypes right now. str, int, and sml. sml can be either 0 or 1 and it stands for small.

Heres an example of making a variable:

```
int coolint = 5;
str coolstring = "5";
sml smallint = 1;
```

## If Else
Example of a simple if else:

```
int cooler = 5;

if cooler == 5
    out "Nice!";
else
    out "Nope.";
end
```

Supported operators: == != > < >= <=

You can add as many elif branches as you want between the if and the else. They get checked in order and only the first one that matches runs:

```
int cooler = 7;

if cooler == 5
    out "Nice!";
elif cooler == 7
    out "Lucky number!";
elif cooler > 10
    out "Thats a big number!";
else
    out "Nope.";
end
```

The colon at the end of a condition is optional, so `if cooler == 5:` and `else:` work too if you are used to python.

Both sides of a comparison can be a variable, a number, or a string in quotes:

```
int a = 5;
int b = 5;
str name = "bob";

if a == b
    out "Same!";
end

if name != "alice"
    out "Not alice!";
end

if 5 > 3
    out "Math works!";
end
```

One thing to watch out for: a word that isnt in quotes and isnt a number counts as a variable name, so `if name == bob` looks for a variable called bob. Write `if name == "bob"` to compare against the text bob.

## Run
The run keyword executes another file right where the run line sits, so both files share one set of variables. The path is looked for relative to the file the run is written in — not relative to the folder casmpp was started from — so a script can run the files sitting next to it:

```
run "otherfile.casmpp";
run "folder/otherotherfile.casm";

out "files ran";
```

In that example, if the code above sits in example/path.casmpp, then it runs example/otherfile.casmpp and example/folder/otherotherfile.casm. .casm files run exactly like .casmpp ones, just with the usual deprecation warning.

The quotes are optional for a path without spaces:

```
run otherfile.casmpp;
```

Files can run other files, and a file that ends up running itself is stopped with an error after 32 files instead of hanging.

## Redefining Variables
Declaring a variable again gives it a new value instead of making a second variable:

```
int score = 0;
out score;

int score = 10;
out score;
```

That prints 0 and then 10. A redefinition can even change the type, and then the old value is gone:

```
int thing = 5;
str thing = "now text";
```

For math there are += -= *= and /=. They work on int variables that already exist, and they can take a number or another variable on the right:

```
int score = 10;
int score += 5;
out score;

int bonus = 2;
int score += bonus;
```

A few things are errors on purpose, because before they would quietly turn into 0:

```
# error, write int score += 1 instead
int score = score + 1;

# error, points was never declared
int points += 1;

str name = "bob";
# error, math is for ints
int name += 1;
```

## While
A while loop runs its body again and again for as long as its condition is true. The condition is exactly the same as an if condition, so any operator works and either side can be a variable, a number or a string:

```
int var = 0;

while var < 500
    out "loop!";

    int var += 1;
end
```

A condition that is never true just skips the body:

```
while 0 == 1
    out "never";
end
```

There is no limit on how many times a loop runs, so `while 1 == 1` really does run forever. Ctrl-C stops it.

## For
A for loop counts a variable through a range. The end of the range is not included, so `for i = 0-5` runs 5 times with i being 0, 1, 2, 3 and 4:

```
for i = 0-5
    out "this will print 5 times!"
end
```

Both ends can be a number or a variable:

```
int last = 3;

for i = 0-last
    out i;
end
```

After the loop i is left on the end value (5 above), the same way a counting loop in C is. If the start is bigger than the end the body never runs.

## Break
break stops the loop it is written in and carries on with the line after the loop's end:

```
int var = 0;

while var < 500
    out var;

    if var == 3
        break
    end

    int var += 1;
end

out "the loop stopped early";
```

With loops inside loops break only stops the one it is written in. Using break where there is no loop is an error.

## Nesting
if, while and for blocks can go inside each other in any combination, as deep as 64 levels:

```
for i = 0-3
    out "table";

    for j = 0-3
        if j == 2
            break
        end

        out j;
    end
end
```

Every block still ends with its own end, and an elif or else belongs to the closest if above it.

## Functions
You make a function with fn, a name, and the parameters it takes, and you close it with end:

```
fn greet(str who, int times)
    for i = 0-times
        out who;
    end
end
```

Parameters have the usual three types — str, int and sml — and are separated by commas. A function can take no parameters at all:

```
fn banner()
    out "welcome";
end
```

Call a function by writing its name and its arguments on a line of their own:

```
greet("cool", 3);
banner();
```

An argument can be a value written right there, like "cool" or 5, or a variable: greet(word, 5) works too. If the number of arguments or one of their types does not match the definition, the call is reported and nothing runs.

A function can hold everything the rest of the language can: if, elif, else, while, for and break all work inside it, nested as usual. break only stops a loop of the function it is written in.

Parameters, and any variable declared inside a function, belong to that call. They are gone when the function ends, and a declaration inside a function does not change a variable of the same name outside it:

```
int total = 0;

fn add(int amount)
    int total += amount;
end

fn shadow()
    int total = 99;
    out total;
end

add(5);
out total;
shadow();
out total;
```

That prints 5, then 99, then 5. The math assignments += -= *= /= change the variable that already exists, so a function can change an outside one, but int total = ... always makes a local one.

Because every call keeps its own parameters, a function can call itself. Arguments are values or variables and not expressions yet, so count down with a variable:

```
fn countdown(int n)
    if n == 0
        out "liftoff";
    else
        out n;
        int next = 0;
        int next += n;
        int next -= 1;
        countdown(next);
    end
end

countdown(3);
```

A definition can be written above or below the lines that call it, but only at the top level of a file. Functions outlive the file that defines them, which is what makes a library file useful — running it hands you its functions for the rest of the program:

```
run "greetings.casmpp";
greet();
```

Running the same file again replaces the functions it defined the time before, and when two files define the same name the one loaded last wins. A function's run lines look for their files next to the file that defines the function.

Functions do not return a value — they print, loop and change variables. One file can define up to 64 functions, a function can take up to 8 parameters, and calls can nest up to 32 deep.

## Comments
To add a comment you need to put a hashtag at the beginning of the line. Like this:

```
# This wont be interpreted!
```

## Input
To make an input variable you need to add "in" to the beginning of a normal variable declaration.

Anyways with input variables if you define them as int for example the user has to put in an integar.

You make an input variable like this:

```
in str input "Input a string: ";
in int intinput "Input an integar: ";
in sml smlinput "Input a small integar (1 or 0): ";
```

# Credits
Voltaged: [Github](https://github.com/VoltagedDebunked)
