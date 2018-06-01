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
#define PIPE_READ(x) ((x)*2)
#define PIPE_WRITE(x) ((x)*2 + 1)
#define FAUX_PATH "/tmp/notebook/"
#define SUP_DELIM ">>>\n"
#define INF_DELIM "<<<\n"

// Estrutura para o parser do ficheiro
struct cmd{
	int id; // será preciso ou basta o indice?
	char* text;
	char** cmd;
	char* full_cmd; // comando completo com $ e args
	int* needs_me;
	int needs_me_len;
	int my_input_id;
};

void printCMD(struct cmd *Cmd, int x){
	printf("COMMAND %d:\n", x);

	printf("Texto: %s\n", Cmd[x].text);
	printf("Preciso do: %d\n", Cmd[x].my_input_id);

	printf("Precisa de mim: ");
	for(int i = 0; i<Cmd[x].needs_me_len; i++) printf("%d, ", Cmd[x].needs_me[i]);
	printf("\n*********************\n");

	
}

// Variável global com os pids dos filhos
int* son_pids;


// Ctrl + C
void interrupt(int x){
	printf("\nProcessamento cancelado pelo utilizador.\n");
	exit(-1);
	//falta matar filhos
}

char * mystrdup (const char *s) {
    if(s == NULL) return NULL;          
    char *d = malloc (strlen (s) + 1); 
    if (d == NULL) return NULL;       
    strcpy (d,s);                    
    return d;                       
}

// Funcao para ver se o processo que faz redirecionamento do output pode fechar um descritor ou nao
int elem(int* needs_me, int needs_me_len, int n){
	for(int i=0; i<needs_me_len; i++)
		if(n == needs_me[i])
			return 1;

    return 0;
}

// Auxiliares ao parser

int getDependentNumber(char* str){
	char* aux = malloc(sizeof(str));
	int i;

	if(str[1] == ' ') return -1; // "$  cmd"
	if(str[1] == '|') return 1; // obrigaga a que o texto seja escrito com a regra "$|" 
	
	for(i = 1; !isalpha(str[i]) && str[i] != '|'; i++){
		aux[i-1] = str[i];
	}
	aux[i] = '\n';

	int r = atoi(aux);
	if(r == 0) return -1; // para distinguir do primeiro comando
	return r;
}


char* getCmd(char* cmd){
	int i, j;
		printf("got here\n");

	char* r = malloc(sizeof(cmd));

	
	for(j = 0, i = i; cmd[i] != '\n'; i++, j++){
		r[j] = cmd[i];
	}

	r[j] = '\0';
	printf("got here\n");
	return r;
}



char** transformCmdLine(char* cmd_line){
	int n_args = 0, i, first_alpha;

	for(i = 0; !isalpha(cmd_line[i]); i++);
	first_alpha = i;	
		
	for (i = 0; cmd_line[i]!='\0'; i++){					
		if(cmd_line[i] == ' ') n_args++;
	}

	char** cmd_args = malloc(sizeof(char*) * (n_args+1));
	cmd_args[n_args] = NULL; // terminar a o array a null para efeitos de exec

	i = 0;
	char* token; 

  	token = strtok (cmd_line + first_alpha," ");

  	while(token != NULL){
   		cmd_args[i++] = strdup(token);
   		printf("%s\n", token);
    	token = strtok (NULL, " ");
  	}
  	
  	return cmd_args;
}


int* addNeedsMe(int* needs_me, int n, int x){
	int* aux = malloc((n+1)*sizeof(int));

	for(int i = 0; i < n; i++){
		aux[i] = needs_me[i];
	}

	aux[n] = x;
	
	return aux;
}



// Parser para a estrutura

void loadCmds(struct cmd *cmds, char* file, int fileSize){
	int cmd_id = 0;

	char* line = malloc(sizeof(char)*fileSize);

	// Analisa o ficheiro todo
	for(int i = 0; file[i] != '\0'; i++){
		// Analisa linha a linha - Sabemos que estamos a iniciar uma linha nova no inicio do ciclo	
		int j;

		// É texto
		if(isalpha(file[i])){ 
			for(j = 0; file[i] != '\n' && file[i] != '\0'; i++, j++) line[j] = file[i];
			
			line[j] = '\0';
			cmds[cmd_id].text = strdup(line);

			/* Debugging */ printf("Texto: %s\n", cmds[cmd_id].text);
			/* Debugging */ printf("Processou texto\n");
		}

		// É comando
		else if(file[i] == '$'){ 
			for(j = 0; file[i] != '\n' && file[i] != '\0'; i++, j++) {
				line[j] = file[i];
			}

			line[j] = '\0';
			/* Debugging */ printf("Linha com comando: %s\n", line);

			int x = getDependentNumber(line);
			/* Debugging */ //printf("Depende do comando %d (%d comandos atrás)\n", cmd_id-x, x);

			cmds[cmd_id].full_cmd = strdup(line);			
			cmds[cmd_id].cmd = transformCmdLine(line);

			cmds[cmd_id - x].needs_me_len=0;
			if(x >= 0) cmds[cmd_id].my_input_id = cmd_id - x;
			else cmds[cmd_id].my_input_id = -1;
			if(x != -1){
				cmds[cmd_id - x].needs_me = addNeedsMe(cmds[cmd_id - x].needs_me, cmds[cmd_id - x].needs_me_len, cmd_id);
				cmds[cmd_id - x].needs_me_len++;
			}
			printf("%d\n", cmds[cmd_id - x].needs_me_len);
			cmd_id++;
			/* Debugging */ printf("Processou comando\n");
		}

		// É resultado de comando ">>>" ... "<<<" (ignora apenas)
		else if(file[i] == '>'){ 
			while(file[i-1] != '<' && file[i] != '\n' && file[i] != '\0') i++; // MUDAR - !(file[i-1] == '<' && file[i] == '\n') dá segf
			/* Debugging */ printf("Processou resultado\n");
		}

		/* Debugging */ printf("****************************Processou uma linha****************************\n\n");		
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

	close(pd2[0]);

	sscanf(buffer, "%d", &numCmds);

	return numCmds;
}


 
int main (int argc, char* argv[]){
	signal(SIGINT, interrupt);
	
	if(argc == 1){
		printf("Especifique um ficheiro\n");
		return -1;
	}

	int nFilho = 0;
	char *path = argv[1];	
	char *buffer;
	
	int n_cmds = getNumOfCmds(argv[1]);
	
	struct cmd *cmds = malloc(sizeof(struct cmd)*n_cmds);

	int file = open(path, O_RDWR); //abrir ficheiro
	int fileSize=0;

	if(file < 0){
		printf("Não existe o ficheiro\n");
		return -1;
	} 

	//sleep(10); // apenas para testar o sinal

	// Ver tamanho do ficheiro
	struct stat st; 
	if (stat(path, &st) != -1) {
		fileSize = st.st_size; // em bytes
		//printf("ficheiro: %d bytes\n", fileSize);
		buffer = malloc(fileSize);
	}

	// Carregar ficheiro para string Buffer
	while(read(file, buffer, fileSize)>0); 
	
	close(file);	

	// Parser do ficheiro para a estrutura
	loadCmds(cmds, buffer,fileSize);

	/* Debugging */
	for(int i = 0; i<n_cmds; i++){
		printCMD(cmds, i);
	}	

	// Cria 2*n_cmds pipes
	int pipeArray[n_cmds*2][2];
	for(int i=0; i < n_cmds*2; i++) pipe(pipeArray[i]);


	for(nFilho =0; nFilho < n_cmds; nFilho++){

		int id = fork();

		if(id == 0){
			struct cmd to_exec = cmds[nFilho];

			for(int i=0; i<n_cmds; i++){
				close(pipeArray[PIPE_READ(i)][1]);
				close(pipeArray[PIPE_WRITE(i)][0]);

				if(i != nFilho){ 
					close(pipeArray[PIPE_READ(i)][0]);
					close(pipeArray[PIPE_WRITE(i)][1]);
				}					
			}

			if(to_exec.my_input_id == -1) // teste se precisa de resultado de outro comando
				close(pipeArray[PIPE_READ(nFilho)][0]);  // se nao precisar fecha
			else 
				dup2(pipeArray[PIPE_READ(nFilho)][0], 0);

			dup2(pipeArray[PIPE_WRITE(nFilho)][1], 1);

			execvp(to_exec.cmd[0], to_exec.cmd);
			exit(-1);
		}
		else{

			id = fork();
			if(id == 0){
				char output_file[0];
				sprintf(output_file, "%scomando%d.txt", FAUX_PATH, nFilho);
				int output_des = open(output_file, O_RDWR | O_CREAT, 0666);  	
				int to_send, n_read;
				char* result_buffer = malloc(1024 * sizeof(char));
				struct cmd to_exec = cmds[nFilho];
				for(int i=0; i<n_cmds; i++){
					if(i != nFilho) close(pipeArray[PIPE_WRITE(i)][0]);						
					if(i != nFilho && elem(to_exec.needs_me, to_exec.needs_me_len, i) == 0)						
						close(pipeArray[PIPE_READ(i)][1]); // fechar descritor de escrita porque nao vai ser preciso enviar resultado a este
					
					close(pipeArray[PIPE_WRITE(i)][1]);
					close(pipeArray[PIPE_READ(i)][0]);

				}
				close(pipeArray[PIPE_READ(nFilho)][1]);

 				while((n_read = read(pipeArray[PIPE_WRITE(nFilho)][0], result_buffer, 1024)) > 0){
						for(int i=0; i< to_exec.needs_me_len; i++){
							to_send = to_exec.needs_me[i];
							write(pipeArray[PIPE_READ(to_send)][1], result_buffer, n_read);
						}
						write(output_des, result_buffer, n_read);
				}	

				for(int i=0; i< to_exec.needs_me_len; i++){ // fechar os descritores para escrita
					to_send = to_exec.needs_me[i];
					close(pipeArray[PIPE_READ(to_send)][1]);
				}

				close(pipeArray[PIPE_WRITE(nFilho)][0]);
				close(output_des);
				exit(0);		
			}
		}
	}

	for(int i=0; i<n_cmds; i++){
		close(pipeArray[PIPE_WRITE(i)][0]);
		close(pipeArray[PIPE_WRITE(i)][1]);
		close(pipeArray[PIPE_READ(i)][0]);
		close(pipeArray[PIPE_READ(i)][1]);
	}

	for(int i = 0; i<n_cmds*2;  i++){
		wait(NULL);
	}
	
	// escrita final: conquer
	int n_read;
	int fid = open(argv[1],O_TRUNC | O_WRONLY,0666);
	
	for (int i=0; i<n_cmds; i++) {
		write(fid,cmds[i].text,strlen(cmds[i].text));
		write(fid,"\n",1);
		write(fid,cmds[i].full_cmd,strlen(cmds[i].full_cmd));
		write(fid,"\n",1);

		char res_buf;

		char output_file[0];
		sprintf(output_file, "%scomando%d.txt", FAUX_PATH, i);
		int output_src = open(output_file, O_RDWR | O_CREAT, 0666);

		write(fid,SUP_DELIM,4);
		while(n_read=read(output_src,&res_buf,1)>0){
			write(fid,&res_buf,n_read);
		}
		write(fid,INF_DELIM,4);
		close(output_src);
	}
	close(fid);

	return 0;
}
