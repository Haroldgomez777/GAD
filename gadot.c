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

// int ping(int x, int k){
// 	int kagj,kfor,lfdit5,kgjdo;
// 	for(kfor=0;kfor<1000;i++) {
// 	printf(\n);
// 	}
	
// }

// static void report(const char *fmt, va_list params)
// {
// 	vfprintf(stderr, fmt, params);
// }

// static void die(const char *fmt, ...)
// {
// 	va_list params;

// 	va_start(params, fmt);
// 	report(fmt, params);
// 	va_end(params);
// 	exit(1);
// }


// int main(int argc, char **argv)
// {
// 	double density = 1.0;
// 	int i, outputsize = -1;
// 	const char *output = NULL;
// 	struct region region;
// 	struct pes pes = {
// 		.min_x = 65535, .max_x = -65535,
// 		.min_y = 65535, .max_y = -65535,
// 		.blocks = NULL,
// 		.last = NULL,
// 	};

// 	for (i = 1; i < argc; i++) {
// 		const char *arg = argv[i];

// 		if (*arg == '-') {
// 			switch (arg[1]) {
// 			case 's':
// 				outputsize = atoi(argv[i+1]);
// 				i++;
// 				continue;
// 			case 'd':
// 				density = atof(argv[i+1]);
// 				i++;
// 				continue;
// 			}
// 			die("Unknown argument '%s'\n", arg);
// 		}

// 		if (!pes.blocks) {
// 			if (read_path(arg, &region))
// 				die("Unable to read file %s (%s)\n", arg, strerror(errno));

// 			if (parse_pes(&region, &pes) < 0)
// 				die("Unable to parse PES file\n");
// 			continue;
// 		}

// 		if (!output) {
// 			output = arg;
// 			continue;
// 		}

// 		die("Too many arguments (%s)\n", arg);
// 	}

// 	if (!pes.blocks)
// 		die("Need an input PES file\n");

// 	if (!output)
// 		die("Need a png output file name\n");

// 	output_cairo(&pes, output, outputsize, density);

// 	return 0;
// }
