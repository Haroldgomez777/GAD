/*

 	This is a program to convert html files to txt files						 
	Author: Harold Gomez								
	It is not complete									
	compile using 										
	gcc													

*/ 



#include<stdio.h>
int main(int argc, char const *argv[]) {
FILE *fp,*fp2;
int c;
int flag=0;
char str[10];
if ( argc < 2 ) /* argc should be 2 for correct execution */
{
    /* We print argv[0] assuming it is the program name */
    printf( "usage: %s filename\nTry it using 'gad example.html' \nIf you are in Gad Folder\n", argv[0] );
}

else {

fp = fopen(argv[1],"rw");
if (fp == 0) {
        fprintf(stderr, "Error while opening");
        return 1;
}


argv[2]==NULL?(fp2=fopen("copy.txt","w+")):(fp2=fopen(argv[2],"w+"));

if(fp2==0) {
	fprintf(stderr, "Error while opening");
	return 1;
}

	while( (c=fgetc(fp))!=EOF ) {
	
	if(c=='<') {
	flag=1;
	continue;
	}
	if(c=='>') {
		flag=0;
		continue;
	}
	if(flag==0)
	fprintf(fp2,"%c",c);
	}

fclose(fp2);
fclose(fp);

}
return 0;

}
