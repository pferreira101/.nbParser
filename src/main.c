#include <fcntl.h> //open file
#include <unistd.h> //read/write
#include <stdio.h> //printf
#include <stdlib.h> //malloc
#include <string.h> //strings
#include <sys/stat.h>

/* PROBLEMA QUE ME SURGIU: RELER OUTRA VEZ APÓS A EXECUÇÃO DO PRIMEIRO COMANDO?? */





int getNumOfCmds(char* file){
	char buffer[50]; 
	int id1, id2, pd1[2], pd2[2], numCmds;

	pipe(pd1);
	pipe(pd2);

	id1 = fork();

	if(id1==0){
		dup2(pd1[1], 1); // redirecionar stdout para pipe escrita 

		close(pd1[0]);
		close(pd2[1]);
		close(pd2[0]);

		execlp("fgrep", "fgrep", "$", file, NULL);
		_exit(-1);
	}
	else{
		id2 = fork();

		if(id2 == 0){
			dup2(pd1[0], 0); // le resultado da execucao do outro filho
			dup2(pd2[1], 1); // manda resultado ao pai

			close(pd1[1]);
			close(pd2[0]);

			execlp("wc", "wc", "-l", NULL);
			_exit(-1);
		}
	}

	close(pd1[0]);
	close(pd1[1]);
	close(pd2[1]);

	for(int i = 0; i < 2; i++){
		wait(NULL);
	}

	int n_read = read(pd2[0], buffer, 50);

	if(n_read < 0){
		printf("error\n");
	}

	sscanf(buffer, "%d", &numCmds);

	return numCmds;
}



char** transformCmdLine(char* cmdLine, int n_args){
	char** cmdArgs = malloc(sizeof(char*) * (n_args+1));
	cmdArgs[n_args] = NULL; // terminar a o array a null para efeitos de exec

	int i = 0;
	char* token; 

  	token = strtok (cmdLine," ");

  	while (token != NULL){
   		cmdArgs[i++] = strdup(token);

    	token = strtok (NULL, " ");
  	}
  	return cmdArgs;

}
 
int main (int argc, char* argv[]){
	char* supDelim = ">>>";
	char* infDelim = "<<<";
	int nFilho = 0;
	char *path = argv[1];	
	char *buffer;
	int n_cmds = getNumOfCmds(argv[1]);

	// array de pipes, tantos como comandos 
	int pipeArray[n_cmds][2];
	// guardar resultados para escrever no doc
	char outputs[n_cmds][2048];

	int file = open(path, O_RDWR); //abrir ficheiro
	int fileSize;

	if (file < 0) {
		printf("Não existe o ficheiro\n");
		return -1;
	} 
	
	struct stat st; // ver tamanho do ficheiro
	if (stat(path, &st) != -1) {
		fileSize = st.st_size; //em bytes
		printf("ficheiro: %d bytes\n", fileSize);
		buffer = malloc(fileSize);
	}

	while(read(file, buffer, fileSize)>0) // Carregar ficheiro para string Buffer

	for (int i=0; i<fileSize; i++) {
		if (buffer[i] == '$') { //carregar comando simples para a string cmd
			char cmdLine[2048]; //max unix line: getconf LINE_MAX
			int count = 1;			

			if (buffer[i+1] == '|') i = i+3;
			else i = i+2;

			for (int lidos = 0; i<fileSize && buffer[i]!='\n'; i++, lidos++) {					
				cmdLine[lidos] = buffer[i];
				if(buffer[i] == ' ') 
					count++;
			}		

			char** cmdArgs = transformCmdLine(cmdLine, count);

			pipe(pipeArray[nFilho]);

			int id = fork();

			if(id == 0){

				dup2(pipeArray[nFilho][1], 1);
				close(pipeArray[nFilho][0]);

				execvp(cmdArgs[0], cmdArgs);
				exit(-1);
			}
			else{
				close(pipeArray[nFilho][1]);
				nFilho++;
			}
		}
	}

	for(int i = 0; i<nFilho;  i++){
		wait(NULL);
	}

	for(int i=0; i<nFilho; i++){
		int n_read = read(pipeArray[i][0], outputs[i], 2048);
		close(pipeArray[i][0]);
		outputs[i][n_read] = '\0';
		printf("%s\n", outputs[i]);
	}
	
	close(file);	

	return 0;
}
