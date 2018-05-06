#include <fcntl.h> //open file
#include <unistd.h> //read/write
#include <stdio.h> //printf
#include <stdlib.h> //malloc

int main () {
	
	char *buffer=malloc(sizeof(char)*1024);

	int file = open("exemplo.nb",O_RDWR,0666); //abrir ficheiro	

	if (file>0) {
		while (read(file,buffer,1024)>0)
			printf("%s\n",buffer); //teste leitura
		close(file);	
	}

	return 0;
}
