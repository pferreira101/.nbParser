#include <fcntl.h> //open file
#include <unistd.h> //read/write
#include <stdio.h> //printf
#include <stdlib.h> //malloc
#include <string.h> //strings
#include <sys/stat.h>

/* PROBLEMA QUE ME SURGIU: RELER OUTRA VEZ APÓS A EXECUÇÃO DO PRIMEIRO COMANDO?? */

int main () {

	char *path = "exemplo.nb";	
	char *buffer;
	
	int file = open(path,O_RDWR,0666); //abrir ficheiro
	int fileSize;

	if (file>0) { 
		struct stat st; // ver tamanho do ficheiro
		if (stat(path,&st)!=-1) {
			fileSize=st.st_size; //em bytes
			printf("ficheiro: %d bytes\n",fileSize);
			buffer=malloc(fileSize);
		}
		
		while(read(file,buffer,fileSize)>0) // carregar ficheiro para string Buffer
			printf("%s\n",buffer);

		for (int i=0;i<fileSize;i++) {
			int lidos=0;
			if (buffer[i]=='$') { //carregar comando simples para a string cmd
				int j;				
				if (buffer[i+1]=='|') j=i+3;
				else j=i+2;

				char cmd[2048]; //max unix line: getconf LINE_MAX
				for (;j<fileSize && buffer[j]!='\n';j++) {					
					memset(cmd+lidos,buffer[j],1);
					lidos++;
				}
				printf("%s\n",cmd);

				execlp(cmd,cmd,NULL); // precisa de um fork
			}
		}

		close(file);	
	}

	return 0;
}
