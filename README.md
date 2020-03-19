# pisearch

Switch language by setting LG=1 or 2 in the source code

==HOWTO==

PI SEARCH stores efficiently successions of digits, namely the decimals of PI. It also allows to search for these decimals in an efficient way.

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
