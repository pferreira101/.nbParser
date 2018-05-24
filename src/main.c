#include <fcntl.h> //open file
#include <unistd.h> //read/write
#include <stdio.h> //printf
#include <stdlib.h> //malloc
#include <string.h> //strings
#include <sys/stat.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <ctype.h>


// Como cada processo tem dois pipes, assim sabemos qual o seu indice
#define PIPE_READ(x) = (x*2)
#define PIPE_WRITE(x) = (x*2 + 1)


// Estrutura para o parser do ficheiro
struct cmd{
	int id; // será preciso ou basta o indice?
	char* text;
	char** cmd;
	int* needs_me;
	int my_input_id;
};


// Variável global com os pids dos filhos
int* son_pids;


// Ctrl + C
void interrupt(int x){
	printf("\nProcessamento cancelado pelo utilizador.\n");
	exit(-1);
	//falta matar filhos
}


// Parser para a estrutura

int getDependentNumber(char* str){
	char* aux = malloc(sizeof(str));

	if(str[2] == '|') return 1; // nada genérico assim
	
	int i = 2;
	for(i = i; !isalpha(str[i]) && str[i] != '|'; i++){
		aux[i-2] = str[i];
	}
	aux[i] = '\n';

	int r = atoi(aux);
	if(r == 0) r = -1; // para se depender do primeiro comando dar para distinguir

	return r;
}

char* getCmd(char* cmd){
	int i;
	char* r = malloc(sizeof(cmd));

	for(i = 0; !isalpha(cmd[i]); i++);
	for(int j = 0, i = i; cmd[i] != '\n'; i++, j++){
		r[j] = cmd[i];
	}

	return r;
}

char** transformCmdLine(char* cmd_line){
	int n_args = 0;

	for (int i = 0; cmd_line[i]!='\0'; i++) {					
		if(cmd_line[i] == ' ') n_args++;
	}

	char** cmd_args = malloc(sizeof(char*) * (n_args+1));
	cmd_args[n_args] = NULL; // terminar a o array a null para efeitos de exec

	int i = 0;
	char* token; 

  	token = strtok (cmd_line," ");

  	while(token != NULL){
   		cmd_args[i++] = strdup(token);

    	token = strtok (NULL, " ");
  	}
  	
  	return cmd_args;
}


void loadCmds(struct cmd *cmds, char* file, int n_args){
	int cmd_id = 0;

	char* line = malloc(sizeof(file));

	// Analisa o ficheiro todo
	for(int i = 0; file[i] != '\0'; i++){
		// Analisa linha a linha - Sabemos que estamos a inicia uma linha nova no inicio do ciclo	
		int j;

		// É texto
		if(isalpha(file[i])){ 
			for(j = 0; file[i] != '\n'; i++, j++) line[j] = file[i];
			
			line[j] = '\0';
			
			cmds[cmd_id].text = strdup(line);

			/* Debugging */ printf("Processou texto\n");
		}

		// É comando
		else if(file[i] == '$'){ 
			for(j = 0; file[i] != '\n'; i++, j++) {
				line[j] = file[i];
				/* Debugging */ printf("%c\n", line[j]);
			}

			line[j] = '\0';

			char* cmd_line = getCmd(line);
			int x = getDependentNumber(line);

			cmds[cmd_id].cmd = transformCmdLine(cmd_line);
			cmds[cmd_id].my_input_id = cmd_id - x;
			//cmds[cmd_id - x].needs_me = addNeedsMe(cmd_id);

			cmd_id++;
			/* Debugging */ printf("%s\n", line); // Nao imprime
			/* Debugging */ printf("Processou comando\n");
		}

		// É resultado de comando ">>>" ... "<<<" (ignora apenas)
		else if(file[i] == '>'){ 
			while(file[i] != '<' && file[i+1] != '\n') i++;
			/* Debugging */ printf("Processou resultado\n");
		}

		/* Debugging */ printf("Processou uma linha\n");		
	}
}




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
		printf("Erro\n");
	}

	sscanf(buffer, "%d", &numCmds);

	return numCmds;
}


 
int main (int argc, char* argv[]){
	signal(SIGINT, interrupt);
	
	if(argc == 1){
		printf("Especifique um ficheiro\n");
		return -1;
	
	}

	char* supDelim = ">>>\n";
	char* infDelim = "<<<\n";
	int nFilho = 0;
	char *path = argv[1];	
	char *buffer;
	
	int n_cmds = getNumOfCmds(argv[1]);
	
	struct cmd *cmds = malloc(sizeof(struct cmd)*n_cmds);

	

	int file = open(path, O_RDWR); //abrir ficheiro
	int fileSize;

	if(file < 0){
		printf("Não existe o ficheiro\n");
		return -1;
	} 

	//sleep(10); // apenas para testar o sinal


	struct stat st; // ver tamanho do ficheiro
	if (stat(path, &st) != -1) {
		fileSize = st.st_size; //em bytes
		//printf("ficheiro: %d bytes\n", fileSize);
		buffer = malloc(fileSize);
	}

	while(read(file, buffer, fileSize)>0) // Carregar ficheiro para string Buffer
	

	// Parser do ficheiro para a estrutura
	loadCmds(cmds, buffer, n_cmds);


	// Cria 2*n_cmds pipes
	int pipe_array[n_cmds*2][2];
	for(int i=0; i<n_cmds; i++) pipe(pipe_array[i]);

	/* ---------------------------------------------- Não mudei a partir daqui ------------------------------------------------------*/

	// array de pipes, tantos como comandos 
	int pipeArray[n_cmds][2];
	// guardar resultados para escrever no doc
	char outputs[n_cmds][2048];


	
	for (int i=0; i<fileSize; i++) {
		if (buffer[i] == '$') { //carregar comando simples para a string cmd
			char cmdLine[2048]; //max unix line: getconf LINE_MAX
			int count = 1;			

			if (buffer[i+1] == '|') i = i+3;
			else i = i+2;

	

			char** cmdArgs = transformCmdLine(cmdLine);

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
		printf("%s\n", outputs[i]);
	}
	
	close(file);	

	return 0;
}
