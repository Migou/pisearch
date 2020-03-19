#ifdef MIGOU
x=`tempfile /tmp`
if test "$1" = "gdb"
then
  echo "gcc -g $0 -o $x"
  gcc -g $0 -o $x
  shift; 
  echo "gdb --args $0 $@"
  gdb --args $x $@
else 
  echo "gcc $0 -o $x"
  gcc $0 -o $x
  chmod 700 $x
  $x $@
fi
rm $x
exit
#endif

/***************************************************/
/*       PI SEARCH                                 */
/*                                                 */
/* Searches for any succession of type long int    */
/* 0 - 2^63-1 (not unsigned)                       */
/* that is from 0 to around 9223372036854,78.10^6  */
/*                                                 */
/* so to say, digit strings up to 18 digits        */
/*                                                 */
/* this program was optimized only for searching   */
/* digit strings from the ?.dat format file        */
/* database                                        */
/*                                                 */
/***************************************************/
/*             Author : Vicnent Barbier aka Migou  */ 
/***************************************************/

/***************************************************/
/*        V1 : finalized on the 18/03/2020        */
/*************************************************/
  
#include <stdio.h>
#include <stdlib.h> //exit
#include <string.h> // for read_opts

//#define DEBUG
#define OPTIMIZE //nothing very thrilling, actually removes... one line that is not really useful.

#define LG 2 // 1=fr 2=en

#define FILEDATPATTERN "/media/vincent/sdb1/pisearch/dat/pi%06ld.dat"
                         
//#define NBBLOBS 10000
#define NBBLOBS 10000000 // for controlling the .dat files size
                        // 5*10 000 000 B =  50 MB / file
                        // 12*10 000 000 decimals = 120M decimals
  
#define BLOBOCTETSIZE 5 
#define BLOBDIGITSIZE 12 
#define MAXSTRING 0x7fffffffffffffff 
#define DEFAULTSEARCH 159265358979 // for uninspired users
#define L(fr,en) LG==1 ? fr : en 

  
FILE* fh_log;

  // formats are refered to by the nomber of digits of the minimal element
  // format3 = 12-digit blob (5 bytes containing 4 numbers from 0 to 999, each of them are coded on 10bits)
  // format2 = hexbin (2 numbers per byte), a very widely used format for storing decimals of pi. (for instance used by Kanada-sensei)
  // format1 = ascii (human readable, one digit per byte...)
  // format12 = 12-digit number (5 octets) -- not implemented
  typedef enum _format {unkn_format=0, format12=1, format2=2, format1=3, format3=4 } type_format;
  


/***************************************************/
/*       MODULE UTILS                              */
/***************************************************/
void die1(const char* mess, int info){
  fprintf(stderr,"%s : %d",mess,info);
  exit(1);
}
void warn_l(const char* mess, long int info){
  fprintf(stderr,"WARNING %s : %ld\n",mess,info);
}

void die_str(const char* mess, const char* info){
  fprintf(stderr,"%s : %s\n",mess,info);
  exit(1);
}

void die(const char* mess, int code){
  fprintf(stderr,"%s",mess);
  exit(code);
}
int c2i(char c)
{
  if( '0'<=c && c<='9')
  {
    return (int)(c - '0');
  }  
  else
    {
      fprintf(stderr,"fatal --%c--\n",c);
      exit(2);
    }
}

// the number of significant digits the number "string" has
int get_nb_digits(long int string)
{
  int res = 1;
  while( string >= 10   )
  {
      res++;
      string = string / 10;
  }
  return res;
}


// a fast MODULO 4
// the % operator doesn't behave as expected for negative integers.
#define MOD4(i) i & 0x3



/***************************************************/
/*       MODULE BLOB - LONG INT 12 digits          */
/*                                                 */
/* handles ins an outs to the                      */ 
/* blob (long int 12 digits) format                */
/***************************************************/


// stores value into the 12-digit/5 bytes long int
// with :
//  - size : the length of a unit (in bits)
//  - offset : the offset of the storage slot (index within the units)
//
// eg for storing a 10bit 3-digit number at offset 2 (bits 20-29) :
// store_size_offset_value(&longint12digit,10,2,123);

// eg for storing a byte number at offset 3 :
// store_size_offset_value(&longint12digit,8,3,'a');
 
// long int = [00000000... offset4 offset3 offset2 offset1 offset0 ] <- least significant bit (LSB)
void store_size_offset_value(long int*res,int size,int offset,int value)
{
  long int sav = *res;
  long int ones = (1 << size) - 1 ; // 1 << size = 2 ^ size, 2 ^size - 1 is a succession of size '1's

  long int mask = -1 -  (ones << (offset*size)) ; // -1 = 0xffffffffffffffff
  // mask will look like 11111111OOOOOO11111111

  *res = (*res & mask) | ((value & ones) << (offset*size));
  //printf("%lx -> %lx\n",sav,*res);
}

// reads a sequence of size bits at the given offset and returns them as an integer.
//
// eg. for retrieving a 3-digit number coded on 10 bits at offset 2 (bits 20-29) :
// int nombre = read_size_offset(longint12digit,10,2);
int read_size_offset(long int res,int size,int offset)
{
  long int ones = (1 << size) - 1 ; // 1 << size = 2 ^ size 2 ^size - 1 est la suite de size '1's

  int num = (int)( (res >> (size*offset)) & ones);

  return num;
}

void print_blob(long int blob)
{
  // reads the 4 10-bit numbers, beginning by the most significant
  for(int i = 3; i>=0; i--)
  {
    int triplet = read_size_offset(blob,10,i); // selecting 10 bits
    printf("%03d",triplet);
  }
}


// buffer : a set of 5 bytes in the 12-digit blob format
//
// the first item of the buffer array is the most significant bit (MSB)
//
// returns the integer between 0 et 999 999 999 999 corresponding to that buffer
long int charblob2longblob(char* buffer)
{
  long int num = 0;
  for(int i = 0; i<5; i++)
  {
    store_size_offset_value(&num,8,i,buffer[i]); //i=0 = LSB
  }
  return num;
}


/***************************************************/
/*       MODULE FILE ITERATOR                      */
/***************************************************/
typedef struct _filelist { int nbfile; char** files; int ifile; FILE* cur; type_format format; } new_fileiterator;
// I gave a flavour of OOP (not poo) to C by using the following syntax :
// new_fileiterator fi = make_fileiterator(...);
// and then
// class_fileiterator = &fi;
// which allows to use a pointer to the object everywhere in a natural way.
typedef new_fileiterator* class_fileiterator;

//fileiterator files_in;

// initialises the iterator : warning, no copy is made of the list of files.
new_fileiterator make_fileiterator(int nbfile, char** file_list, type_format format)
{
  #ifdef DEBUG
  printf("Initialising the list of files\n");
  for(int i=0;i<nbfile;i++){
    printf("%s\n",file_list[i]);
  }
  #endif
  new_fileiterator self;
  self.files = file_list ;
  self.nbfile = nbfile ;
  self.ifile = -1 ;

  //self.cur = NULL ;
  if( self.nbfile > 0 ){
    self.cur = fopen(self.files[0],"r");
    self.ifile = 0 ;
    if(! self.cur ) die_str("can't open",self.files[0]);
  }
  else
  {
    die("file list is empty",790);
  }
  
  self.format=format;
  return self;
}

// returns the next file or null if all the files have been iterated
FILE* next_file(class_fileiterator self)
{
  self->ifile++;
  if(self->ifile < 0) die("fatal error in FILE LIST : index less than 0\n",681);
  if(self->ifile >= self->nbfile ) return NULL;
  // if reached there, whe've got a file to open.
  
  fclose(self->cur);
  char* filename = self->files[self->ifile];
  FILE* f = fopen(filename,"r");
  if(!f) die_str("can't open ",filename);
  
  self->cur = f;
  return f;
}

// reads nbchars bytes from the file(s) and fills the c array (/!\ must have been allocated)
// when the file is over, reads the next one (next_file command).
// hence the group of the nbchar read bytes can overlap two files.
// c[11] = MSB and c[0] = LSB
int read_bytes(class_fileiterator self, char* c, int nbchars)
{
  int i = nbchars-1;
  int lu; // parce que pour reconnaître un EOF, il faut conserver le format int
  FILE* fh_in = self->cur;
  do{
    lu=fgetc(fh_in);
    c[i]=lu;
  }
  while(
	( (lu != EOF) && --i >= 0 ) || ( (lu == EOF) && (fh_in = next_file(self)) )
	);

  return nbchars - (i+1) ;
}


// converts a 12 character array into a blob
//
// p_res = the long int to be filled
// c : the 12 byte array
//
// /!\ c[0] = LSB
//     c[11] = MSB
//
// isinteger : the format of the char : either the characters '0' to '9' (case isinteger = 0)
// or the int values 0 to 9 (case isinteger = 1)
void chars2blob(long int* p_res,char* c,int isinteger)
{
  *p_res = 0;
  for(int i_triplet=3; i_triplet >=0; i_triplet--)     
  {
      // the index of the triple inside the blob. alas it is the reverse of buffer c.
      // long int = [00000000... offset3 offset2 offset1 offset0 ] <- LSB
    int val_triplet = 0;
    for(int u = 2; u>=0; u--)      
    {
	// u : hundreds=2, tens=1, units=0
	int i_read = (i_triplet*3)+u;
	if(isinteger)
	{
	  if(0 <= c[i_read] && c[i_read] <= 9 )
	  {
	    val_triplet *= 10;    
	    val_triplet += c[i_read];	      
	  }
	  else
	  {
	    die1("found unexpected number",(int)c[i_read]);
	  }
	  
	}
	else
	{
	  if('0' <= c[i_read] && c[i_read] <= '9')
	  {	
	    val_triplet *= 10;    
	    val_triplet += (c[i_read]-'0');
	  }
	  else
	  {
	    die1("found unexpected character\n",c[i_read]);
	  }
	}
    }
    store_size_offset_value(p_res,10,i_triplet,val_triplet);
  }
}



// reads 5 bytes and returns the correspondaing blob (12 digits in 4x10bits).
// returns EOF (-1) if the file is over.
//
// NB : the format of the file is already in blob format. Hence this function only performs a copy.
long int _read3_blob(class_fileiterator fi)
{
  char c[BLOBOCTETSIZE];
  int car;

  int nbread = read_bytes(fi,c,BLOBOCTETSIZE);
  
  if(nbread < BLOBOCTETSIZE)
  {
    // implies EOF
    return -1; // 0xffffffffffffffff; // EOF for a long_int
  }
  else
  {
    return charblob2longblob(c);
  }
}


// reads 5 bytes and returns the corresponding blob
// returns EOF (-1) if the file is over.
long int _read12_blob(class_fileiterator fi)
{
  die("fonction not implemented",791);
}


// reads the hexbin format
// returns a 12-digit blob (4 numbers of 10bits in 5 bytes) 
// returns EOF if the last file is over
long int _read2_blob(class_fileiterator fi)
{
  char c[6]; // the 12 digits
  long int res=0;
  
  int car;
  int i = 5;
  
  int nbread = read_bytes(fi,c,6); // c[0] = 2 least significant digits

  if(nbread < 6)
  {
    return -1; // implies EOF
  }
  else
  {

    // conversion to the blob format in two steps :
    char c_exp[12]; // c[0] = least significat digit

    // - buffer of 12 integers of type char
    for(int i = 0; i<6; i++)
    {     
      int poids_fort = (c[i] & 0x0f0) >> 4;
      c_exp[2*i+1] = poids_fort;
      int poids_faible = c[i] & 0x0f;
      c_exp[2*i] = poids_faible;
    }

    // - namely the conversion
    chars2blob(&res,c_exp,1);
    
    return res;
  }
}

// reads the ascii format
// returns EOF if le last file is over
long int _read1_blob(class_fileiterator fi)
{
  char c[12]; // the 12 digits, LSB = c[0] 
  long int res=0;
  int car;

  int nbread = read_bytes(fi,c,12);
  
  if(nbread < 12) return -1; // implies EOF
  
  chars2blob(&res,c,0);
  
  return res; 
}


// converts 12-digit blob (4x10bits) to 12-digit integer
// greater index = MSB
long int blob2num(long int blob)
{
  long int res=0;
  for(int i = 3; i>=0; i--)
  {
    int triplet = read_size_offset(blob,10,i); // selecting 10 bits
    res*=1000;
    res+=triplet;
  }
  return res;
  
}

// returns 12-digit blobs (4x10bits)
long int next_blob(class_fileiterator self)
{
  switch( self->format ) // input format
    {
    case format3:
      return _read3_blob(self); 
    case format2:
	return _read2_blob(self);
    case format12:
        return _read12_blob(self); 
    case format1:
	return _read1_blob(self);
    default:
      die1("code format imprévu",self->format);
    }
}


/***************************************************/
/*       MODULE RING VECTOR                        */
/***************************************************/

class_fileiterator files_in;
int maxsize; // the maximal number of bases 
int ideb; // the first used tile
long int i12digit_beg; // index in pi of th first item (0 = 141592653589, 1=7932... )
int ringsize; 
typedef long int* typering;
typering ring;

void make_ring(int ringsizemax, class_fileiterator fi)
{
  files_in = fi;
  ideb=0;
  ringsize=0;
  i12digit_beg=0;
  maxsize=ringsizemax;
  ring = (typering)malloc( ringsizemax * sizeof(long int));
} 

// adds an item to the queue
// returns 0 if failure
// returns 1 if ok
int push(long int item)
{
  if(ringsize==maxsize) return 0;

  ring[ (ideb+ringsize++) % maxsize ] = item;  
  return 1;
}

// frees the head element 
// returns 0 if failure (array already empty)
// returns 1 if ok
int freering()
{
  if(ringsize==0) return 0;
  ideb = (ideb+1) % maxsize;
  i12digit_beg++;
  ringsize = (ringsize-1) % maxsize;
  return 1;
}

int freering2(long int i12digit_first_kept)
{
  while( i12digit_beg < i12digit_first_kept )
    {
      int ok = freering();
      if(!ok){
	fprintf(stderr,"WARNING : fonction freering called with empty array. Fatal error?\n");
      }
    }
}



// returns : the blob located at position i12digit_reel (if the index increases by 1 => the number of the decimal increases by 12.)
// the get fonction reads .dat files in a linear way keeping just the previous blob available for reading.
//
// Caution, this function fills the ring structure without freeing it.
// So you can reach saturation of the ring's memory.
// Use this function only if you know what you are doing.
// In order to free space you can either use get_freering() or freering2().
long int get(long int i12digit_reel,class_fileiterator fi)
{
  if(i12digit_reel < i12digit_beg ){
    fprintf(stderr,L("impossible d'accéder à l'élément %ld, situé avant le début du tableau %ld\n","impossible to access item %ld, located before the beginning of the array %ld\n"),i12digit_reel,i12digit_beg);
    exit(671);
  }
  while(i12digit_reel >= i12digit_beg + ringsize )
  {
      if(ringsize==maxsize) die("array full, free some place before you continue",670);
      
      long int suivant = next_blob(fi); // reads just the next blob from the file
      if(suivant == -1) die(L("fin de fichier atteinte","end of file reached\n"),0);
      int ok = push( suivant ); 
  }
  // assert :  i12digit_beg <= i12digit_reel < i12digitdeb + maxsize
  
  int i_anneau = (ideb + (i12digit_reel - i12digit_beg)) % maxsize;
  return ring[i_anneau];
}

// returns : the blob located at position i12digit_reel (if the index is increased by 1 => the number otf the decimal increases by 12.)
// the get_freering function reads the .dat files in a linear way, keeping available for reading only one previous blob.
long int get_freering(long int i12digit_reel,class_fileiterator fi)
{
  long int res = get(i12digit_reel,fi);
  freering2(i12digit_reel-1); // libère tout l'historique en conservant le long précédent. 
  return res;
}



/***************************************************/
/*       MODULE PRINT TO DB                        */
/*                                                 */
/* reads the pi.txt file and generates the         */ 
/* pi.dat compressed file                          */
/***************************************************/

// Blob version, takes a blob as input and prints it to file fh_out
void printf_blob(FILE* fh_out, long int num)
{
  for(int i = 4; i>=0; i--)
  {
    long int car = (num >> (i*8)) & 0x0ff; // on sélectionne 8bits
    // fprintf(fh_out,"%c",(int)car);
    fputc(car, fh_out);
  }
}

int microreadbuf = -1; // a one digit buffer containing the second digit that has been read, or a number <0 if nothing has been red yet.

// returns a character
// returns EOF if the file is over
int hexbin_getc(FILE* f)
{
  if( microreadbuf >= 0 ){
    char res = (char)microreadbuf;
    microreadbuf = -1;
    return res;
  }
  else
  {
    int car=fgetc(f);
    if(car==EOF) return EOF;

    microreadbuf =  car &  0x0f;  // least significant digit
    return (car & 0x0f0 ) >> 4;    // most significant digit
  }  
}
int ascii_getc(FILE* f)
{
  int car=fgetc(f);
  if(car==EOF) return EOF;
  
  if( '0' <= car && car <= '9')
  {
      return (int)car - '0';
  }
  else{
    printf("found unexpected character --%c--\n",car);
    exit(665);
  }    

}

// read files from fileiterator fi (either ascii or hexbin format) and creates the database in the 12digit-blob format (5 byte 4x10bits numbers)
void a2db(class_fileiterator fi)
{
  long int i12digit=0;
  if(fi->nbfile < 1) die("Please, precise the files to use on the command line\n",677);

  // creating a read buffer, this is mandatory even if you don't need to
  // perform any backward reading.
  int ringsizemax = 3; 
  make_ring(ringsizemax, fi);

  char file_out[100]; 

  long int nbdigitswritten=0; // number of digits written into file file_out
  long int nbdigitswrittentot=0; // number of digits written overall
  long int i_filein = 0;
  long int i_fileout = 0; // will be filled with non significant zeroes ... (so what?)

  sprintf(file_out,FILEDATPATTERN,i_fileout);  
  FILE* fh_out = fopen(file_out, "w");
  if(fh_out){ fprintf(fh_log,L("création de %s\n","creating %s\n"),file_out); }
  else {
    die_str(L("erreur fatale, impossible de créer","fatal error, impossible to create"),file_out);
  }

  long int blob = get(i12digit++,fi);
  while(blob != -1)
  {
    #ifdef DEBUG
    printf(L("LU : ","READ : "));
    print_blob(blob);
    printf("\n");
    #endif

    if(! (nbdigitswrittentot % 48000000) ) warn_l(L("Chiffres traités :","# digits treated"), nbdigitswrittentot);
	
    // creates a new file if the former one is full
    if(nbdigitswritten == BLOBDIGITSIZE*NBBLOBS)
    {
      fprintf(fh_log,L("fermeture. %ld chiffres écrits (total=%ld)\n","closing. %ld digits written (total=%ld)\n"),nbdigitswritten,nbdigitswrittentot);
      
      // we create a new file of 50 MB (5*NBBLOBS bytes)
      fclose(fh_out);
      i_fileout++;

      sprintf(file_out,FILEDATPATTERN,i_fileout);
      fh_out = fopen(file_out, "w");
      if(fh_out){ fprintf(fh_log,L("création %s\n","creating %s\n"),file_out); }
      else {
	die_str(L("erreur fatale, impossible de créer","fatal error, impossible to create"),file_out);
      }

      printf(L("ouverture de %s\n","opening %s\n"),file_out);
      nbdigitswritten=0;
    }
    else if(nbdigitswritten > BLOBDIGITSIZE*NBBLOBS)
    {
      die("fatal error",673);
    }

    printf_blob(fh_out,blob);
    nbdigitswritten+=BLOBDIGITSIZE;
    nbdigitswrittentot+=BLOBDIGITSIZE;

    blob = get_freering(i12digit++,fi);
  }  
  
  fclose(fh_out);
}





/***************************************************/
/*       MODULE READ DB                            */
/*                                                 */
/* Reads the pi.dat file and displays the decimals */ 
/* to the screen, 12 digits per line.              */
/*                                                 */
/***************************************************/
void read_db(class_fileiterator fi)
{
  long int i12digit=0;

  int ringsizemax = 4; // with a little margin : 3 is enough.  
  make_ring(ringsizemax, fi);

  long int blob = get(i12digit++,fi);
  while(blob != -1)
  {
    printf("%012ld\n", blob2num(blob));
    blob = get_freering(i12digit++,fi);
  }  
}

void find_position(class_fileiterator fi,unsigned long int position)
{
  long int i12digit=0; // i12digit=0 contains decimals 1 to 12 (1415...)
  long int i12position = position / 12 ;

  int ringsizemax = 4; // with a little security margin, 3 is enough.  
  make_ring(ringsizemax, fi);

  long int blob = get(i12digit++,fi);
  while(blob != -1)
  {
    if( i12digit > (i12position + 1) )
      {
	break;
      }
    if( (i12position - 1) <= i12digit && i12digit <= (i12position + 1) )
      {
	printf(L("décimales de %li à %li : ","decimals %li to %li : "), (i12digit-1) * 12 + 1, (i12digit-1) * 12 + 12);
	print_blob(blob);
	printf("\n");
      }

    blob  =get_freering(i12digit++,fi);
  }  
}



/***************************************************/
/*       MODULE VERIFICATION + SEARCH              */
/*                                                 */
/* Defines the research model to be applioed       */
/* on the blob format 4x(10bit 0 to 999 integers)  */ 
/*                                                 */
/*                                                 */
/***************************************************/


typedef enum _typetest {normal, beg, end, noop, empty} typetest; // empty signifies that the case of the checks array is still unset.
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

// in order to check what's going on.
void print_checks()
{
  for( int i=0;i<12;i++)
  {
    printf("%02d : ",i);
    for( int j=0; j<nbtripletsmax; j++)
    {
      struct check check = checks[i][j];
      if(check.type==beg)
	{
	  printf("[%s,%010lx,%010lx,nb=%d]-",type2string[check.type],check.mask,check.min_value,(int)check.max_value);
	  
	}
      else
	{
      printf("[%s,%010lx,%010lx,%010lx]-",type2string[check.type],check.mask,check.min_value,check.max_value);
	}
    }
    printf("\n");
  }
}

// instanciates a struct check
// offset_db allows to generate the mask corresponding to one of the 4 slots
// long int => [0000 offset=3,offset=2,offset=1,offset=0]
// following long int => [000 offset=-1,offset=-2, etc.   ]
// previous long int => [00000 ...    offset=5,offset=4]
struct check make_check(int i1, int i2, int i3, int offset_db_relatif)
{
  #ifdef DEBUG
  printf("make_check(%d,%d,%d)\n",i1,i2,i3);
  #endif
  int offset_db = MOD4(offset_db_relatif);
  struct check res;
  if(i1==-1 && i2==-1 && i3==-1)
  {
    res.type = noop;
  }
  else if(i1==-1 && i2==-1 && i3>=0)
  {
    res.type = beg;
    struct checkbeg* p_res = (struct checkbeg*)&res; // ugly trick, inherintance emulation
    p_res->nbdigits = 1;
    p_res->min_value = i3;
  }
  else if(i1==-1 && i2>=0 && i3>=0)
  {
    res.type = beg;
    struct checkbeg* p_res = (struct checkbeg*)&res; // ugly trick inheritance emulation
    p_res->nbdigits = 2;
    p_res->min_value = i2*10 + i3;
  }
  else if(i1>=0 && i2>=0 && i3>=0)
  {
    res.type = normal;
    res.min_value = ((long int)(i1*100+i2*10+i3)) << (offset_db*10);
    #ifndef OPTIMIZE
    res.max_value = res.min_value; // facultative, but who knows if something fails, might be good to compile this line
    #endif
  }
  else if(i1 >= 0 && i2 >= 0 && i3==-1)
  {
    res.type = end;
    res.min_value = ((long int)(i1*100+i2*10)) << (offset_db*10);
    res.max_value = ((long int)(i1*100+i2*10+9)) << (offset_db*10);
  }
  else if(i1 >= 0 && i2 == -1 && i3==-1 )
  {
    res.type = end;
    res.min_value = ((long int)(i1*100)) << (offset_db*10);
    res.max_value = ((long int)(i1*100+99)) << (offset_db*10);
  }
  
  res.mask = ((long int)0x03ff) << (offset_db*10);
  return res;
}

// retrieves a number between 0 and 999 inluded, located inside th long int at the offset given by the mask
int check_get_triplet(struct check cur_check,long int num)
{
  // ugly trick to find the offset back. maybe not optimized, but it's not used by the 'normal' check case
  int offset;
  if(cur_check.mask == 0x3ff){ offset=0; }
  else if(cur_check.mask  == 0x0ffc00){ offset=1; }
  else if(cur_check.mask  == 0x03ff00000){ offset=2; }
  else if(cur_check.mask  == 0x0ffc0000000){ offset=3; }
  else{
    die("fatal error due to a rotten stub code\n",669);
  }

  return (int)(  (num & cur_check.mask) >> (10*offset)  );
}

int check_match(struct check cur_check,long int num)
{
  struct checkbeg* pcheck; // required for casting the check...
  long int val; // for the 'end' case
  switch(cur_check.type)
  {
    case normal :
      return (num & cur_check.mask) == cur_check.min_value; // this is the case first tested (for any digit string longer than 4) and rejected 999 times on 1000 => This is the key part of this program, it MUST be as optimized  as possible.
      
    case beg :
      // retrieve units / tens and compare
      pcheck = (struct checkbeg*)&cur_check;

      int triplet = check_get_triplet(cur_check,num);
      
      if( pcheck->nbdigits == 1)
	{	  
	  return (triplet % 10) == pcheck->min_value;
	}
      else if( pcheck->nbdigits == 2)
	{
	  return (triplet % 100) == pcheck->min_value;
	}
      else
	{
	  die1("incorrect attribute nbdigits",pcheck->nbdigits);
	}
      
    case end:
      val=num & cur_check.mask;      
      return cur_check.min_value <= val && val <= cur_check.max_value;
      
    case noop:
      return 1;
      
    default:
      die("fatal error",668);
    }
}

int check_nextnumberneeded(struct check cur_check)
{
  return cur_check.type==normal && (cur_check.mask & 0x01); // if the mask's 1 bits reach the last bit, we are at the end of the blob and we must get the next blob.
}

int check_previousnumberneeded(struct check cur_check)
{
  return cur_check.mask & 0x01; // if the mask (of the previous struct check) reaches the last bit, 
  // this means the previous check has to be tested on the previous blob...
}


// e.g. if we search for 123456789 :
// fragments[0] will contain [noop, 123, 456, 789, noop]  
// fragments[1] will contain [1(type=beg), 234, 567, 89] <= 


/// compares buffer with the list of 12 possible checks/positions
// /!\REQUIRED : the comparison with anterior blobs has already been performed, so that we can forget all blobs at previous indexes (except the last one);

// i12digits : index of the blob on which the first step (check[i_check][1] ) of each check (check[i_check]) has to be made. REM, it is possible that check[i_check][0] requires to perform a check on the previous blob (at index i12digits-1).

// returns true if success.
// stops after the first found occurence
// TODO : allow to continue for further occurences
int compare2(long int i12digits,class_fileiterator fi)
{
  long int blob = get_freering(i12digits,fi); // only at the beginning of the comparison, do we free the previous indexes that have become useless

  #ifdef DEBUG
  printf(L("reçu ","recieved "));
  print_blob(blob);
  printf("\n");
  #endif
  int match = 0;
  for( int i_check = 0; i_check < 12; i_check++)
  {
    int report_fail=0; // debug
    int step_check=1; // NB : the algorithm is defined so that step_check=0 is an empty or uncomplete check (less than 3 digits)   
    struct check cur_check;// = checks[i_check][step_check];

    int check_failed=0;
    long int i12digits_cour = i12digits;
    do
    {
      cur_check = checks[i_check][step_check];

      // current verification
      if( ! check_match(cur_check,blob) )
      {
	check_failed=1;
	#ifdef DEBUG
	if(report_fail){ printf("...search %d failed at step %d\n",i_check,step_check);}
	#endif
	break;
      }
      else
      {
	#ifdef DEBUG
	printf("...found correct triplet during check %02d\n",i_check);
	#endif
	report_fail = 1;
      }

      // preparing the next verification
      if( check_nextnumberneeded(cur_check) ) 
      {
	blob = get(++i12digits_cour,fi);
	#ifdef DEBUG
	printf("...loading : ");
	print_blob(blob);
	printf("\n");
	#endif

      }

      if( cur_check.type != normal ) break ; // end of verification
      
      step_check++;
      cur_check = checks[i_check][step_check];	
    }
    while( (!check_failed) && step_check < nbtripletsmax );
    
    // fatal error, capacity overflow
    if(step_check >= nbtripletsmax)
    {
      fprintf(stderr,"fatal error, the struct check list is too long, check=%d,step=%d",i_check,step_check);
      exit(667);
    }

    if(!check_failed)
    {
	  
      struct check check = checks[i_check][0]; // All has been tested but the first step

      int found=0;
      if(check.type == noop) // bingo, nothing needs to be checked
      {
	found=1;
      }
      else
      {
	  // if verification step 0 targets the units of a long int, this means there is a change of blob between step 0 and step 1.   
	  long int i12digit_beg = check_previousnumberneeded(check) ? i12digits-1 : i12digits ;

	  if(i12digit_beg < 0)
	    {
	      found=0;
	    }
	  else
	    {
	  
	      blob = get(i12digit_beg,fi);
	  
	      found = check_match(check,blob);
	    }
      }

      if(found)
      {

	printf(L("TROUVE :\n","FOUND :\n"));
	for(long int i12tmp = i12digit_beg; i12tmp<=i12digits_cour+1; i12tmp++)
	{
	  // min_decimal = 12 * i12digit + 1;
	  // max_decimal = 12 * i12digit + 12;
	  printf(L("décimales de %li à %li : ","decimals %li to %li : "),12 * i12tmp + 1,12 * i12tmp + 12);
	  print_blob( get_freering(i12tmp,fi) );
	  printf("\n");
	}
	printf("\n");
	return 1;
      }
    }
  }

  #ifdef DEBUG
  printf(L("absent ","missing "));
  print_blob(blob);
  printf("\n");
  #endif
  
  return 0;
}


// gets the "pos" digit inside integer "string"
//
// from 0 for units to nb_digits - 1 for the most significant digit
// it is not efficient to extract every time, but that part is executed
// only once at the beginning of the search so it does(nt impact the algorithm's complexity
//
// returns -1 if index is out of bound
int ch(int pos,long int string)
{
  if(pos<0){ return -1; }
  
  while(pos >= 0)
  {
    if(string==0){ return -1; }
    if(pos==0){ return string % 10; }
    string = string / 10;
    pos--;
  }
}

// creates all the verification scenarii
void prepare_checks(long int string)
{
  int nbdigits = get_nb_digits(string);
  nbtripletsmax = nbdigits/3 + 2;
  #ifdef DEBUG
  printf("nbtripletsmax=%d\n",nbtripletsmax);
  #endif
  // arrays allocation
  for(int i=0; i<12; i++)
  {
    checks[i] = (struct check*)malloc( nbtripletsmax * sizeof(struct check) );
    for(int j = 0; j<nbtripletsmax; j ++) checks[i][j].type = empty;
      
  }
  
  int cas=0;
 //index_beg : the index to be searched within the string(0 = units, 1=tens, etc.). It will correspond to the first element of the first triplet to be matched.
 // NB : the algorithm is based on the fact that the first triplet is an incomplete triplet
  // as a consequence the index_beg is chosen beyond the most significant digit of the string => this will be either N, N+1 ou N+2 with N le number of digits of the string.
  // same explanation, short version : index_beg represents the most "elevated" element. As the first element is an incomplete element (might be event a noop),  index_beg refers to a number before the most significant digit. 
  for(int index_beg = nbdigits; index_beg<nbdigits+3; index_beg++)
  {
    
    // considers the 4 cases : the check pattern has to be applied to any of the 4 slots of the 12 digit blob (5 bytes)
    // each slot contains a 3-digit number. The slots are :
    // [0000 db_offset=3,db_offset=2,db_offset=1,db_offset=0]
    for(int db_offset=0; db_offset < 4; db_offset++)
    {
      int step=0;

      // We walk the string downwards 3 by 3 (towards index_beg=0 the unit digit) 
      // the start position is N, N+1 or n+2
      // the end position is 1 0 or -1
      // 1 -> triplet (1, 0, -1) => incomplete check of type end
      // 0 -> tiplet( 0, -1, -2) => incomplete check of type end
      //-1 -> triplet(-1 -2 -3) => empty check (type noop) : doesn't verify any digit
      // NB : 0=units of the string, 1=tens of the string
      for(int cur_index = index_beg; cur_index >= -1; cur_index-=3)
      {	   
	checks[cas][step] = make_check ( ch(cur_index,string)  , ch(cur_index-1,string), ch(cur_index-2,string), db_offset-step );
	step++;
      }
      
      cas++;
    }
  }
   
}


void search_db(long int string,class_fileiterator fi)
{
  prepare_checks(string);
  #ifdef DEBUG
  print_checks();
  #endif
  int ringsizemax = (get_nb_digits(string) / 12) + 3; // petite marge de sécurité normalement +2 suffit.  
  make_ring(ringsizemax,fi);


  long int i5octets = 0; // the number of the 12-digit blob being analysed.
  // the number of the decimal is given by :
  // decimal_min = 12 * i12digit + 1;
  // decimal_max = 12 * i12digit + 12;
  //
  //                0123456789ab
  // i12digit=0 : [ 141592653589 ]  <-- from decimal 1 to 12
  // i12digit=1 : [ 793238462643 ]  <-- from  decimal 13 to 24
  //

  long int blob12;

  int found=0;
  while( !found )
  {
    // the get_freering fuction frees the ring buffer until the i5octets-1 position

    // launches (unleashes :-D) the verification scenarii on the blob. The compare2 function will callthe next blobs if required, but will not free ring space. (the ring size is chosen so that there is enough space) 
    found = compare2(i5octets++,fi);
  }
}



/***************************************************/
/*       MODULE MAIN                               */
/*                                                 */
/*                                                 */ 
/*                                                 */
/*                                                 */
/***************************************************/

void test_modulo()
{
  printf("-2 %% 4 = %d\n", (-2) % 4);
  printf("-1 %% 4 = %d\n", (-1) % 4);
  printf("0 %% 4 = %d\n", 0 % 4);
  printf("1 %% 4 = %d\n", 1 % 4);
  printf("2 %% 4 = %d\n", 2 % 4);
  printf("3 %% 4 = %d\n", 3 % 4);
  printf("4 %% 4 = %d\n", 4 % 4);

  printf("\nMOD4(-2) = %d\n", MOD4(-2) );
  printf("MOD4(-1) = %d\n", MOD4(-1) );
  printf("MOD4(0) = %d\n", MOD4(0) );
  printf("MOD4(1) = %d\n", MOD4(1) );
  printf("MOD4(2) = %d\n", MOD4(2) );
  printf("MOD4(3) = %d\n", MOD4(3) );
  printf("MOD4(4) = %d\n", MOD4(4) );

}

// 20200112:OK
int test_type2string()
{
  typetest t = normal;
  const char* val = type2string[t];
  int o=2; // offset
  printf(L("%d -> %s ('normal' attendu : 0->n)\n","%d -> %s ('normal' expected : 0->n)\n"),t,val);

  t = beg; // 1
  val = type2string[t];
  printf("%d -> %s ('beg' attendu 1->d)\n",t,val);

  t = end; // 2
  val = type2string[t];
  printf("%d -> %s ('end' attendu 2->f)\n",t,val);

  t = noop; // 3
  val = type2string[t];
  printf("%d -> %s ('noop' attendu 3->. )\n",t,val);

  struct check c1 = make_check(-1,-1,6,o); // début
  val = type2string[c1.type];
  printf("%d -> %s ('beg' attendu 1->d)\n",c1.type,val);

  c1 = make_check(6,7,8,o); // normal
  val = type2string[c1.type];
  printf("%d -> %s ('normal' attendu 0->n)\n",c1.type,val);

  c1 = make_check(9,-1,-1,o); // end
  val = type2string[c1.type];
  printf("%d -> %s ('end' attendu 2->f)\n",c1.type,val);

  c1 = make_check(-1,-1,-1,o); // noop
  val = type2string[c1.type];
  printf("%d -> %s ('noop' attendu 3->.)\n",c1.type,val);
}

/*************************************/
/* A : test unpack_hexbin            */
/*************************************/

// the unpack_hexbin function converts in a very simple way the
//  hexbin format(2 digits per byte) into textual format (one digit per byte)
// The goal of this function is to provide a robust, reliable ascii output to compare to those of other, more complex, functions

// the source files (hexbin) can be downloaded at Kanada-sensei's website)

// TEST A.1
// please launch function unpack_hexbin(1000,100) // 1000 decimals, 100 per file
// you'll do that by modifying the source code at the "test:" section of the switch (around line 1664)

// EXPECTED : 
// creation of /tmp/pi00000.txt to /tmp/pi00009.txt
// TEST OK the 29/02/2020

// TEST A.2 :
// launch unpack_hexbin(100 000 000,100 000 000)
// rename file pi100m.text.000
// compare 'less pi100m.text.000' to 'hexedit pi100m.hexbin.000'
// TEST OK the 29/02/2020

// TEST A.3 :
// launch unpack_hexbin(500 000 000,500 000 000)
// name the file pi500m.txt
// 1) compare the beginning of pi500m.text.000 to 'hexedit pi100m.hexbin.000'
// 2) cat pi500m.txt | tail -n +8333334 | head -n 8333334 > /tmp/500m-extract
//    extracts the slide of file containing decimals 100 000 001 to 200 000 000.
// compare 'hexedit pi100m.hexbin.001' to the beginning of /tmp/500m-extract
//  knowing that you must not take into account the  first 4 numbers of 500m-extract (8333333*12 = 99999996 ). This gives :
//  [1592]21505880
//  957832796348
//  730951352849
// TEST OK on the 29/02/2020


/*************************************/
/* B : TESTING  read2                */
/*************************************/

// REQUIRED
// Let us compare in the following way :
// 1) You will need the ascii version formerly uncompressed by unpack_hexbin (CF ci-dessus)
// pi100m.text.000 (100 M décimales)
// pi500m.text.000 (500 M décimales)

// Generate :
//   ./pi_search-v4.c read2 pi/pi100m.hexbin.00[0-4] > /tmp/x
//
//  NB : You may need to clean a bit the output's first lines which sometimes have some debug info. (e.g. : tail -n +9 /tmp/x  > /tmp/y)

// TEST B.1)
// wc -c pi500m.text.000 /tmp/y
// NOTICE : The sizes will be nearly the same :
//   size of pi500m.text.000 = 500 000 000 /12*13 = 541666666
//   size of y, read2 stops at the lesser multiple of 12, which is 41666666*13 = 541666658
// EXPECTED :
// > 541666666 txt500m/pi500m.text.000
// > 541666658 y
// OK le 01/03/2020

// TEST B.2)
// NOTICE : diff will not work for reasons of memory saturation, so please use  
// the provided mydiff utility program
// ./mydiff pi500m.text.000 y
// EXPECTED :
// > 541666658 caractères identiques <- le nombre de caractères correspond
//                                      à y
// > fin prématurée de y
// > files don't match <- normal
// OK le 01/03/2020


/*************************************/
/* C : testing create and create2     */
/*************************************/

  // Checking that the 12-digit (5 bytes) blob format is correctly generated

  /////////////////////////////////////
  // EXAMPLE BLOBS                   //
  /////////////////////////////////////

  // If we chose to start the first blob with 31415... this wil give ...
  // (NB : We chose as a convention that the unit's digit (3) will not be
  // stored. so, this blob should not be useful for tests)
  //
  // 314 159 265 358             ... 979 323 846 264
  // 4 numbers between 0 and 999 coded on 10 bits each
  // 314 = . 256 . . 32 16 8 . 2 .     = 0100111010
  // 159 = . . 128 . .  16 8 4 2 1     = 0010011111
  // 265 = . 256 . . .  .  8 . . 1     = 0100001001
  // 358 = . 256 . 64 32 . . 4 2 .     = 0101100110
  //
  // so on 5 bytes it makes : 0100111010 0010011111 0100001001 0101100110
  // 0100 1110 1000 1001 1111 0100 0010 0101 0110 0110
  //    4 E       8 9       F 4       2 5       6 6

  // On the other hand, if we chose (that's our choice) to start with
  // 141592.. the first blob will be :
  //
  // 141 592 653 589
  // 141= .   . 128 . . .  8 4 . 1 = 0010001101
  // 592= 512 .  . 64 . 16 . . . . = 1001010000
  // 653= 512 . 128 . . .  8 4 . 1 = 1010001101
  // 589= 512 . .  64 . .  8 4 . 1 = 1001001101

  // 0010001101 1001010000 1010001101 1001001101
  // 0010 0011 0110 0101 0000 1010 0011 0110 0100 1101
  //    2 3       6 5       0 A       3 6       4 D

  // TEST : ./pi_search create2 ~/pi/pi100m.hexbin.search
  // open file 000000.dat with hexedit and check the first lines
  // EXPECTED : 2365 0A36 4D
  // tested OK on the 9th of february 2020


/*************************************************************/
/** ADVANCED TESTS                                          **/
/*************************************************************/

// TESTING 'search'
// ./pi_search.c create2 ~/pi/pi100m.hexbin.000 (100M decimals) 10 seconds

// limit test : the first 4 digits
// A) ./pi_search-v4.c search 1415    -> FOUND 141592653589 (the 3 is not included in Kanada's hexbin...)
// OK on the 15/02/2020

// LIMIT TEST :
// B) ./pi_search-v4.c search 31415 -> must avoid the trap of searching a digit at position -1 (hence for example segfaulting :-) ).
// => FOUND 933304962653 141514138612
// OK on the 15/02/2020


// C) ./pi_search-v4.c search 4338327 -> FOUND 793238462643 383279502884 (the second and third 12uple of pi)
// OK the 15/02/2020

// the following results have been verified thanks to https://www.angio.net/pi/

// D) ./pi_search-v4.c search 12345 ->
// EXPECTED at position 49702. = 31177648973523092666**12345**888731028
// -> FOUND 523092666123 458887310288
// OK le 15/02/2020
 

// E) ./pi_search-v4.c search 123456
// EXPECTED at position 2 458 885. = 20290596335933756562**123456**34918731
// -> FOUND 123456349187
// OK le 15/02/2020


///////////////////////////////////////////////////////////////////
// Testing the coordination of a .dat database having many files //
///////////////////////////////////////////////////////////////////

// set NBBLOBS=10000 in the source code
//
// ./pi_search.c create2 ~/pi/pi100m.hexbin.000 (100M décimales)  => 
// EXPECTED : 50 000bit files  containing 120 000 decimals each
// 100M decimals / 120 000 = 833 full files (99960000 decimals)
// from pi000000.dat to pi000832.dat
// and one last not complete file of 40000 decimals, namely 16666,66 octets
// Given that we stock 5 bytes by 5 bytes, the las file will be truncated at
// 16665 bytes = 39996 decimals.
//
// TEST OK the 15/03/2020


// please replay tests A to E
// A) OK the 28/02
// B) OK the 28/02
// C) OK the 28/02
// D) OK the 28/02
// E) OK the 28/02


///////////////////////////////////////////////////////////////////
// Testing with a REAL SIZE DATABASE                             //
///////////////////////////////////////////////////////////////////

// set NBBLOBS=10 000 000 in the source code
// ./pi_search-v1.c create2 ~/pi/pi100m.hexbin.00[0-4] (5*50 Mo = 250Mo = 500M decimals) 

// F)
// NOTICE : created files should have NBBLOBS*12 decimals = 120 M decimals
//                                and NBBLOBS*5  bytes    =  50 M bytes
// EXPECTED :
//  - pi000000.dat to pi000003.dat : 50 000 000 B 
//  - pi000004.dat 8 333 330 B 
// TEST OK the 17/03/2020

// G)
// ./pi_search-v1.c search 123456
// EXPECTED : position 2 458 885. = 20290596335933756562**123456**34918731
// TEST OK the 17/03/2020 (position and surrounding digits OK) 

// H)
// ./pi_search-v1.c search 12345678
// EXPECTED : position 186 557 266 = 72269200957811705289**12345678***05029135885957638876
// TEST OK the 17/03 (position and surrounding OK)

///////////////////////////////////////////////////////////////////
// Testing with a even-more-real SIZE DATABASE                   //
///////////////////////////////////////////////////////////////////

// ./pi_search-v1.c create2 ~/pi/pi100m.hexbin.0[0-2]* (30 input files of 100M decimals each = 3000M decimals)
// with NBBLOBS 10000000 (120M decimals & 50Mo per file)

// I) checking the generated files
// 3000M decimals in 120M decimal-files = 3000M/120M = 25 files exactly
// EXPECTED : 25 full files of 50 MB
// TEST OK the 17/03/2020

// J)
// open ~/pi/pi100m.hexbin.029 for example with hexedit. Pi's decimals will be
// displayed that way, for instance :
//
//00000000   14 15 92 65  35 89 79 32  38 46 26 43  38 32 79 50  ...e5.y28F&C82yP
//00000010   28 84 19 71  69 39 93 75  10 58 20 97  49 44 59 23  (..qi9.u.X .IDY#
//
// chose a digit string and launch the research on that string.
// ./pi_search-v1.c search <the chosen string>
// verify that the string is found and that the context is correct.
//
// example string : 678823239198121671
// NB : in the current v1 version, we are limited to 18 digit searches
// 98 77 33 19  84 84 69 70 07 ***678823239198121671*** 51 00  26 82 62 64 57 40 05 24
// TEST OK the 17/03/2020

// Well done, the program is validated !

// only to be used in the unpack_hexbin function.
#define FILEHEXBINPATTERN "/home/vincent/pi/pi100m.hexbin.%03ld"
#define NBDIGITPERLINE 12

// Oracle function
// The unpack_hexbin function converts in a very simple way the
// hexbin format(2 digits per byte) into textual format (one digit per byte)
// The goal of this function is to provide a robust, reliable ascii output to compare to those of other, more complex, functions
//
// creates a testfile with NBDIGITPERLINE characters per line
//
// REQ : output_digitsperfile is even
void unpack_hexbin(long nbdigits, long output_digitsperfile)
{
  char file_out[100];
  char file_in[100];
  long int i_file_in=0;
  long int i_file_out=0;
  int lu;
  FILE* fh_in;
  FILE* fh_out;
  
  long int nb_digits_written_tot=0;
  long int nb_digits_written_file=0;
  int nb_digits_written_ligne=0;

  if( output_digitsperfile & 1 ) die("argument output_digitsperfile must be even",781);
  
  while( nb_digits_written_tot < nbdigits)
  {
    // updating the input filehandle
    if(! fh_in ){
      sprintf(file_in,FILEHEXBINPATTERN,i_file_in++);
      fh_in = fopen(file_in, "r");      
    }
    if(! fh_in ){
      die_str("can't open",file_in);
    }

    // updating the output filehandle
    if(!fh_out)
    {
      sprintf(file_out,"/tmp/pi%06ld.txt",i_file_out++);
      fh_out=fopen(file_out,"w");
    }
    if(!fh_out)
    {
      die_str("can't write",file_out);
    }

    // conversion
    lu = fgetc(fh_in);

    if(lu == EOF)
    {
      fh_in=(FILE*)0;
    }
    else
    {
      // if you got there, you can write

      int tens = ( lu & 0x0f0 ) >> 4;
      int unites = lu & 0x00f;
      
      fputc('0'+tens,fh_out);
      nb_digits_written_tot++;
      nb_digits_written_file++;
      nb_digits_written_ligne++;
      if( nb_digits_written_ligne == NBDIGITPERLINE ){ fputc('\n',fh_out); nb_digits_written_ligne=0; }
      
      fputc('0'+unites,fh_out);
      nb_digits_written_tot++;
      nb_digits_written_file++;
      nb_digits_written_ligne++;
      if( nb_digits_written_ligne == NBDIGITPERLINE ){ fputc('\n',fh_out);  nb_digits_written_ligne=0; }
    }


    if(nb_digits_written_file > output_digitsperfile )
    {
      die("fatal error - parity problem",780);
    }    
    if(nb_digits_written_file == output_digitsperfile )
    {
      fclose(fh_out);
      fh_out=0;
      nb_digits_written_file=0;
      nb_digits_written_ligne=0;
    }

  }
  
}

// TEST OK the 12/01/2020
int test_ch()
{
  // position 5-> 314159 <-position 0
  long int pi=314159;
  printf("%d %d,%d%d%d%d%d %d (expected -1 3,14159 -1)\n",ch(6,pi),ch(5,pi),ch(4,pi),ch(3,pi),ch(2,pi),ch(1,pi),ch(0,pi),ch(-1,pi));
}

typedef enum {create, create2, read, read2, showposition, search, test, help, howto} typecommand;

// argc the number of arguments as provided by the command line (remember that argument 0 is the program's name)
// REM : char*** p_filename_list is a reference to the list of files to read
void read_opts(int argc,char** argv,long int* pstring, typecommand* pordre, int* p_nbfile, char*** p_filename_list){
  if(argc < 1 ){ printf("usage %s command [parameters]\ntry %s help for more information\n",argv[0],argv[0]); exit(0); }
  for(int i=1;i<argc;i++)
  {
    #ifdef DEBUG
    printf("argument %s\n",argv[i]);
    #endif

    if( strcmp( argv[i], "search" )==0 ){
      // string option
      *pordre = search;

      if(argc>++i)
      {
	char* str_string = argv[i];
	if(strlen(str_string) > 19 ) die_str("number too long, 18 digits max : ",str_string);      
	if(strlen(str_string) > 18 ) warn_l("number should be not more than ",MAXSTRING);
	sscanf(str_string,"%ld",pstring);
      }
      else
      {
	*pstring=DEFAULTSEARCH;
      }
      
    }
    else if( strcmp( argv[i], "test" )==0 ) *pordre=test;
    else if( strcmp( argv[i], "create" )==0  ){
      *pordre=create;
      *p_filename_list =  &(argv[i+1]) ;
      *p_nbfile = argc - (i+1); // number of args at the end of the list
      return; 
    }
    else if( strcmp( argv[i], "create2" )==0 ){
      *pordre=create2;
      // the following elements are the names of the input files
      *p_filename_list =  &(argv[i+1]) ;
      *p_nbfile = argc - (i+1); // number of args at the end of the list
      return; 
    }
    else if( strcmp( argv[i], "read" )==0 ) *pordre=read;
    else if( strcmp( argv[i], "position" )==0 ){
      *pordre=showposition;

      if(argc>++i)
      {
	char* str_pos = argv[i];
	if(strlen(str_pos) > 19 ) die_str("number too long, 18 digits max : ",str_pos);      
	if(strlen(str_pos) > 18 ) warn_l("number should be not more than ",MAXSTRING);
	sscanf(str_pos,"%ld",pstring);
      }
      else
      {
	die1("Argument missing : position of the decimal to display\n",999);
      }    
    }
    else if( strcmp( argv[i], "read2" )==0 ){
      *pordre=read2;
      *p_filename_list =  &(argv[i+1]) ;
      *p_nbfile = argc - (i+1); 
      return; 

    }
    else if( strcmp( argv[i], "help" )==0
	     ||strcmp( argv[i], "-h" )==0 ){
      *pordre=help;
      return;

    }
    else if( strcmp( argv[i], "howto" )==0 ){
      *pordre=howto;
      return;

    }
    else{
      die_str("unknown command : ",argv[i]);
    }
  }
}

// returns the list of filenames and the number of files (by reference)
char** intuite_files_dat(int* p_nbfile)
{
  const int max=100;
  char* buf[max];
  char* filename;
  int ibuf=0;
  int i=0;

  // searches the existing files
  do{
    filename = (char*)malloc(100*sizeof(char));
    sprintf(filename,FILEDATPATTERN,(long int)i++);

    // checks whether file exists
    FILE* f = fopen(filename,"r") ;
    if(!f) break;

    buf[ibuf++]=filename;
    fclose(f);
  }
  while(ibuf<max);

  
  *p_nbfile = ibuf;
  char** res = malloc(sizeof(char*)*ibuf);
  for(int i =0; i<ibuf; i++)
  {
    res[i] = buf[i];
  }
  return res;  
}

int main(int argc,char** argv)
{
  long int string = -1; // TODO, that's just stupid to store the search string as ant int, given that it restrains us to 18 caracters... To be improved
  unsigned long int position=0;
  new_fileiterator fi;
  
  int nbfile = 0;
  char** list_filenames = (char**)malloc(1*sizeof(char*));
  list_filenames[0]="pi000000.txt"; // list of the raw data files (ascii or hexbin) 

  fh_log = fopen("/tmp/pi_search.log", "w");
  
  typecommand ordre = help;

  if( sizeof(long int) < 5 )
  {
    printf("désolé, ce programme repose sur des long int de taille minimale 5 octets (ex : 8octets) taille ici %ld - votre compilateur n'est pas compatible - programme validé sous Linux Mint 64bits\n",sizeof(long int));
    exit(666);
  }

  //(int argc,char** argv,long int* pstring, typecommand* pordre, int* p_nbfile, char*** p_filename_list)
  read_opts(argc,argv,&string,&ordre,&nbfile,&list_filenames);
    
  switch(ordre){
  case howto:
    printf(LG==1 ?
"Changez de langage en choisissant LG=1/2 dans le code source\n\
Switch language by setting LG=1 or 2 in the source code\n\
\n\
   -o-o-o-\n\
\n\
PI SEARCH stoque de façon efficace des successions de chiffres, typiquement\n\
les décimales de PI. Il permet également de rechercher ces décimales de façon\n\
efficace.\n\
\n\
Pour démarrer :\n\
\n\
1) Récuperez une base de données de décimales de pi au format ascii ou au\n\
   format compressé, dit hexbin (2 chiffres par octet)\n\
   peut-être ici : https://www.angio.net/pi/digits.html\n\
2) Mettez à jour la constante FILEDATPATTERN décrivant le dossier de\n\
   destination\n\
3) Exécutez le programme pour consituer la base de données au format .dat\n\
   > pi_search create/create2 <files>\n\
   'create' pour un format source ascii et 'create2' pour un format source\n\
   compressé\n\
4) Profitez de votre base de données en effectuant des recherches :\n\
   > pi_search search 323846264\n"
	   :
"Switch language by setting LG=1 or 2 in the source code\n\
\n\
   -o-o-o-\n\
\n\
PI SEARCH stores efficiently successions of digits, namely the decimals of PI. It also allows to search for these decimals in an efficient way.\n\
\n\
For starters :\
\n\
1) Retrieve a database of decimals of pi in ascii format or compressed\n\
   format (also called hexbin) containing 2 digits par byte\n\
   maybe here : https://www.angio.net/pi/digits.html\n\
2) Update FILEDATPATTERN constant to chose a destination folder\n\
3) Execute the program in order to create a database in the .dat format\n\
   > pi_search create/create2 <files>\n\
   Using 'create' for an ascii input format and 'create2' for a compressed\n\
   input format\n\
4) Help yourself, have fun with your database, perform searches :\n\
   > pi_search search 323846264\n"
);
    break;
  case help:
    printf(LG==1 ?
"pi_search howto : guide de démarrage rapide\n\n\
pi_search search <chaine>\n\
  recherche la première occurence de <chaine> dans pi. chaine est un nombre inférieur ou égal à %ld\n\n\
pi_search create <files>\n\
  lit les fichiers ascii donnés et remplit piNNNNNN.dat N de 000000 à autant que nécessaire, 50Mo par fichier.\n\n\
pi_search create2 <files>\n\
  pareil que create mais lit les fichiers dans le format hexbin de Kanada-sensei*1 (les fichiers devraient être placés ici : (pi/pi100m.hexbin.NNN)\n\n\
pi_search read : affiche la totalité des décimales de pi stockées, 12 chiffres par ligne\n\
\n\
pi_search read2 files : affiche la totalité des décimales de pi stockées, 12 chiffres par ligne à partir d'une liste de fichiers au format hexbin*1\n\
\n\
*1 le format hexbin est une façon largement répandue de stocker pi, avec 2 chiffres par octet.\n\n\
REM gdb autolaunch :\n\
  si vous disposez du fichier source pi_search.c, executer <<./pi_search.c gdb [search|create2|help] args...>> lancera le débuger gdb\n\
  (head -n 18 pi_search.c for more details)\n"
:
"pi_search howto : quickstart guide\n\n\
pi_search search <string>\n\
  searches the first position of <string> inside pi. string is a number inferior or equal to %ld\n\n\
pi_search create <files>\n\
  reads the given files in ascii format and fills piNNNNNN.dat N from 000000 to whatever required, 50M bytes per file.\n\n\
pi_search create2 <files>\n\
  same as create but reads the given files in Kanada-sensei's hexbin format*1 (files should be there : (pi/pi100m.hexbin.NNN)\n\n\
pi_search read : shows the whole stored pi digits, 12 digits per line\n\
\n\
pi_search read2 files : shows the whole stored pi digits, 12 digits per line, read from a list of hexbin*1 files\n\
\n\
*1 hexbin format is a widely spread way for storing pi, with two digits per byte.\n\n\
REM gdb autolaunch :\n\
  if you still have the codefile pi_search.c, executing <<./pi_search.c gdb [search|create2|help] args...>> will launch the gdb debugger\n\
  (head -n 18 pi_search.c for more details)\n",MAXSTRING);
    
	  exit(0);

  case test:
	//test_ch();
	//test_type2string();
	//test_modulo();
        //printf("EOF=%d\n",EOF);
    // unpack_hexbin(100000000,100000000); // nb digits à stocker, nb digits par file
    //unpack_hexbin(500000000,500000000);
	exit(0);
	
      case create:	
	// read ascii, create database (compresse pi à 5/12 )
	fi = make_fileiterator(nbfile,list_filenames,format1);
	a2db(&fi); 
	break;
      case create2 :
	// lit du hexbin, crée database au format 12digit-blob
	fi = make_fileiterator(nbfile,list_filenames,format2);	
	a2db(&fi); 
	exit;
	break;
    case showposition:
      //lit l'environnement de pi autour de la n-ième décimale, n étant transmis en paramètre.
      position=string; string=-1; // ugly, mais on a récupéré la position (long int) dans la variable string.      
      list_filenames = intuite_files_dat(&nbfile);
      fi = make_fileiterator(nbfile, list_filenames,format3);
      find_position(&fi,position);
      break;
    case read:
      // lit les files .dat stockés par le programme (dans le répertoire du programme)
      list_filenames = intuite_files_dat(&nbfile);
      fi = make_fileiterator(nbfile, list_filenames,format3);
      read_db(&fi);
      break;
    case read2:
      // lit les files hexbin fournis en ligne de commande, la fonction read_opts a mis les parametres à jour.
      fi = make_fileiterator(nbfile, list_filenames,format2);
      read_db(&fi);
      break;
    case search:    
      list_filenames = intuite_files_dat(&nbfile);
      fi = make_fileiterator(nbfile, list_filenames,format3);
      #ifdef DEBUG
      printf("\n-----\n");
      #endif      
      printf(L("recherche de %ld\n","searching for %ld\n"),string);
      search_db(string,&fi);
      break;
    default:
      die("fatal error",3);
  }
  
}
