# pisearch

PI SEARCH stores efficiently successions of digits, namely the decimals of PI. It also allows to search for these decimals in an efficient way.

Searches the decimals of for any succession of type long int
0 - 2^63-1 (not unsigned) that is from 0 to around 9223372036854,78.10^6 

So to say, digit strings up to 18 digits   

This program was optimized for searching digit strings from the ?.dat format file database, which stores 12 digits per 5 bytes.

==HOWTO==

Switch language by setting LG=1 or 2 in the source code

For starters :
1) Retrieve a database of decimals of pi in ascii format or compressed
   format (also called hexbin) containing 2 digits par byte
   maybe here : https://www.angio.net/pi/digits.html
2) Update FILEDATPATTERN constant to chose a destination folder
3) Execute the program in order to create a database in the .dat format
   > pi_search create/create2 <files>

   Using 'create' for an ascii input format and 'create2' for a compressed
   input format
4) Help yourself, have fun with your database, perform searches :
   > pi_search search 323846264

==COMMAND==

pi_search howto : quickstart guide

pi_search search <string>
  searches the first position of <string> inside pi. string is a number inferior or equal to 9223372036854775807

pi_search create <files>
  reads the given files in ascii format and fills piNNNNNN.dat N from 000000 to whatever required, 50M bytes per file.

pi_search create2 <files>
  same as create but reads the given files in Kanada-sensei's hexbin format*1 (files should be there : (pi/pi100m.hexbin.NNN)

pi_search read : shows the whole stored pi digits, 12 digits per line

pi_search read2 files : shows the whole stored pi digits, 12 digits per line, read from a list of hexbin*1 files

*1 hexbin format is a widely spread way for storing pi, with two digits per byte.

REM gdb autolaunch :
  if you still have the codefile pi_search.c, executing <<./pi_search.c gdb [search|create2|help] args...>> will launch the gdb debugger
  (head -n 18 pi_search.c for more details)

==HOWTO RUN IT==

Required : set constant FILEDATPATTERN to something apropriate (inside source code)

> gcc pi_search.c

> chmod 755 a.out

> ./a.out

For fun, you might also like to use the "autocompile" functionality included into pi_search.c

> chmod 755 pi_search.c

> ./pi_search.c


==HOW DOES IT WORK==

===Database structure===

The digit database stores digits as blobs of 12 digits coded on 5 bytes.

those 12 digits are represented as a succession of 4 3-digit numbers (between 0 and 999) each of them represented on 10 bits.

Hence, We have : 4 x 10bits = 40 bits = 5x8 = 5 bytes

Example.

We made the choice to start to start our pi-database with 141 592 653 589... neglecting the initial 3.

Then the first blob will be :

 > 141= .   . 128 . . .  8 4 . 1 = 0010001101
 
 > 592= 512 .  . 64 . 16 . . . . = 1001010000
 
 > 653= 512 . 128 . . .  8 4 . 1 = 1010001101
 
 > 589= 512 . .  64 . .  8 4 . 1 = 1001001101

 > 0010001101 1001010000 1010001101 1001001101
 
 > 0010 0011 0110 0101 0000 1010 0011 0110 0100 1101
 
 >    2 3       6 5       0 A       3 6       4 D


===Database search===

Considering the time of a search should be proportional to the number searched, we focussed on searching efficiently huge numbers.

For this purpose, we perform our comparisons at the level of 3-digit numbers without unpacking each decimal.

For instance, searching for 65358, we will

1) We define in advance 12 different check scenario corresponding of the 12 start positions of the comparison. Those scenarii will be applied to each blob.


2) Open the first 5 bytes blob : 0010001101 1001010000 1010001101 1001001101

This can be read 141 592 653 589 (but we hardly ever unpack these numbers int he algorithm)

Execute the scenario corresponding to each position :
 
 > 012 345 678 9ab  (position)
 > 141 592 653 589  (digit)

=== The algorithm step by step ===

For start position 0 :

Our searched string is cut into 265 and 35 

265 is compared with 141 (3-digit number at position 0) -> comparison fails


comparison failed for position 0, so we try position 1 :


Our searched string is cut into 26 and 535

26 having less than 3 digits, we start our comparison with 535 to be compared with position 3.

535 doesn't match 592 so it's a fail too.

comparison failed for position 1, so we try position 2 :


Our searched string is cut into 2, 653 and 5 

2 having less than 3 digits, we start our comparison with 653 to be compared with position 3.

653 doesn't match 592 so iot's a fail too.


Note that all this preparation of cutting the search string into 3-digit numbers is made (obviously) before the main loop of the search algorithm. That way the check in itself is performed very fast.

 > blob & patternmask == patternvalue


As you can see here, we process the 5 byte/12 digit blob as is, without unpacking it.


Let's see what happens with the right position (5)

once again, our searched string is cut into 2, 653 and 5

once again, 2 having less than 3 digits, we start our comparison with 653 to be compared with position 6.

635 (the check pattern) matches 635 (the 3-digit number at position 6)

so we move forward to the next pattern and compare 5 (the pattern) to 589 (the 3-digit number at position 9) -> once againt it's a match, as 589 begins with a 5.

so we move forward to the next pattern again... well we are at the end of the patterns, so that's wonderf... hey, wait a minute, having started our check with 635, we have skipped the first digit of the pattern (2). We must check that :

2 will be compared to the 3-digit number in position 3 (592).

592 ends with a 2 so it's a match.

So that's marvelous, we have found the first occurence of 26535 !


=== Conclusion ===

Consider how this system allows to gain time on huge patterns.

Say we are searching for the position of a 18-digit string inside pi.

The pattern will be split into 5 or 6 3-number digits (depending of the start position of the check). Only the left and right extremities of the pattenr might be represented as numbers inferior to 100.

Those partial checks of the extremities are not as efficient, so they are performed at the end of the chek, when all the complete 3-digit patterns have been matched.

that meens that 999 times out of 1000, only one check will be performed

999 times out of 1000000, only two checks will be performed

999 times out of 1000000000, only three checks will be performed

and so on.


To put it short, most of the time, the match is done applying one or two times this line of code : 

 > blob & patternmask == patternvalue






















gnifies that the case of the checks array is still unset.
const char* type2string[] = {"n","d","f",".","_"}; 
struct check {
  typetest type; // for a normal typetest, only one value is allowed (3 digits totally defined). for this reason, min_value and max_value are the same here. Hence we shall only test min_value. max_value est potentially undefined.
  long int mask;
  long int min_value;
  long int max_value;  
};

// emulation of object inheritance : we shall cast struct check into struct checkbeg to take advantage of the correct attribute name nbdigits (instead of max_value).
struct checkbeg {
  typetest type;  
  long int mask;
  long int min_value;
  long int nbdigits;  
};

// Every tile contains a list of checks according to the following structure
// [ partialcheck, complete,  complete,  complete, partialcheck ]
// partialcheck can be noop, beg or end
struct check* checks[12]; // the three ways of cutting the number (numbers are basically represented as integers from 0 to 999) times the 4 slots inside the 12-digit blob => makes 12 check scenarii.
int nbtripletsmax; // allocated size (in number of struct check) for each check array


