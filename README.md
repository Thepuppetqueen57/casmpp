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
