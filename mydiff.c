#include <stdio.h>
#include <stdlib.h>

#define LG 2 // 1=fr 2=en
#define L(fr,en) LG==1 ? fr : en 

// ne remarque les blocs identiques que s'il y a SENSIBILITE octets consécutifs
// will notice identical blocs only if they are more than SENSIBILITE bytes long
#define SENSIBILITE 12
 

void die(const char* mess){
  fprintf(stderr,"%s",mess);
  exit(1);
}
void die_str(const char* mess, const char* info){
  fprintf(stderr,"%s : %s\n",mess,info);
  exit(1);
}

// fonction diff qui indique si les fichiers sont différents ou identiques
int main(int argc, char** argv)
{
  
  long int nbok=0;
  long int nbnok=0;
  long int nbunreported=0;
  int matches=1;
  if(argc != 3){
    printf("usage : mydiff file1 file2\n");
    exit(1);
  }
  char* fich1 = argv[1];
  char* fich2 = argv[2];
  
  int c1;
  int c2;
  
  
  FILE* fh1 = fopen(fich1,"r");
  if(!fh1) die_str(L("ouverture impossible","can't open"),fich1);
  FILE* fh2 = fopen(fich2,"r");
  if(!fh2) die_str(L("ouverture impossible","can't open"),fich2);
  
  do{
    c1 = fgetc(fh1);
    c2 = fgetc(fh2);
       
    if(c1==c2){	  
      if(c1 == EOF){
	if(nbnok) printf(L("%ld caractères différents\n","%ld characters differ\n"),nbnok);
	if(nbok) printf(L("%ld caractères identiques\n","%ld characters match\n"),nbok);
	printf(L("fin de fichier atteinte\n","reached end of file\n"));
	exit(0);
      }
      nbok++;

    }
    else{
      matches=0;
      if(c1 == EOF || c2 == EOF){
	if(nbnok) printf(L("%ld caractères différents\n","%ld characters differ\n"),nbnok);
	if(nbok) printf(L("%ld caractères identiques\n","%ld characters match\n"),nbok);
	if(c1 == EOF) printf(L("fin prématurée de %s\n","premature end of file %s\n"),fich1);
	if(c2 == EOF) printf(L("fin prématurée de %s\n","premature end of file %s\n"),fich2);
	exit(0);
      }
      //printf("DIFF %c-%c\n",c1,c2);
      if(nbok > SENSIBILITE){
	if(nbnok){
	  printf(L("%ld différents\n","%ld differ\n"),nbnok);
	}
	nbnok=0;
	printf(L("%ld identiques\n","%ld match\n"),nbok);
      }
      else{
	nbnok += nbok;
      }
      nbok=0;
      nbnok++;
    }
  }
  while(c1 != EOF && c2 != EOF);
    
  printf(matches ? L("fichiers identiques\n","files match\n") : L("fichiers différents\n","files don't match\n"));
  return 0;
}
