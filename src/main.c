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
#define SUP_DELIM ">>>"
#define INF_DELIM "<<<"

// Estrutura para o parser do ficheiro
typedef struct cmd{
	char* text;
	char** cmd;
	char* full_cmd; // comando completo com $ e args
	int* needs_me;
	int needs_me_len;
	int my_input_id;
}*Comando;

// Funcao para inicializar uma variavel do tipo cmd
Comando initComando(){
	Comando new = malloc(sizeof(struct cmd));
	new->text = NULL;
	new->cmd = NULL;
	new->full_cmd = NULL;
	new->needs_me = NULL;
	new->needs_me_len = 0;
	new->my_input_id = -1;

	return new;
}


void printCMD(Comando cmd){
	printf("Texto: %s\n", cmd->text);
	printf("Preciso do: %d\n", cmd->my_input_id);

	printf("Precisa de mim: ");
	for(int i = 0; i<cmd->needs_me_len; i++) printf("%d, ", cmd->needs_me[i]);
	printf("\n*********************\n");

	
}

// Variável global com os pids dos filhos
int* son_pids = NULL;
int son_pids_len = 0;

// Ctrl + C
void interrupt(int x){
	printf("\nProcessamento cancelado pelo utilizador.\n");
	for(int i = 0; i < son_pids_len; i++){
		kill(son_pids[i], SIGKILL);
	}
	exit(-1);
}

// Quando ocorre um erro
void error(int x){
	printf("\nProcessamento cancelado devido a erro\n");
	for(int i = 0; i < son_pids_len; i++){
		kill(son_pids[i], SIGKILL);
	}
	exit(-2);
}


char* mystrndup(char* string, int n_chars){
  char* result;
  int len = strlen(string);

  if(n_chars < len)
    len = n_chars;

  result = malloc(len + 1);
  memcpy (result, string, len);
  result[len] = '\0';

  return result;
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
	char* aux = malloc(strlen(str));
	int i;

	if(str[1] == ' ') return -1; // "$  cmd"
	if(str[1] == '|') return 1; // obriga a que o texto seja escrito com a regra "$|" 
	
	for(i = 1;  str[i] != '|'; i++){
		aux[i-1] = str[i];
	}
	aux[i] = '\0';
	// ***** testar com \0 para ver se nao rebenta
	int r = atoi(aux);

	return r;
}


char** transformCmdLine(char* cmd_line){
	int n_args = 0, n, i , j, first_alpha;

	for(i = 0; !isalpha(cmd_line[i]); i++);
	first_alpha = i;	
		
	for(i = 0; cmd_line[i]!='\0'; i++){					
		if(cmd_line[i] == ' ') n_args++;
	}

	char** cmd_args = malloc(sizeof(char*) * (n_args+1));
	cmd_args[n_args] = NULL; // terminar o array a null para efeitos de exec
	
	i = n = 0;
	while((cmd_line+first_alpha)[i] != '\0'){
		j = i;
		while((cmd_line+first_alpha)[j] != '\0' && (cmd_line+first_alpha)[j] != ' ') j++; 
		cmd_args[n++] = mystrndup(cmd_line+first_alpha, j-i);
		/* Debbuging */ printf("%s\n", cmd_args[n-1]);
		i = j;
	}

  	return cmd_args;
}


int* addToArray(int* v, int n, int x){
	int* aux = malloc((n+1)*sizeof(int));

	for(int i = 0; i < n; i++){
		aux[i] = v[i];
	}

	aux[n] = x;
	
	return aux;
}



// Parser para a estrutura

void loadCmds(Comando* cmds, char* file, int fileSize){
	int cmd_id = 0;
	int i=0;
	char* line = malloc(sizeof(char)*fileSize);

	while(file[i] != '\0'){
		// Analisa linha a linha - Sabemos que estamos a iniciar uma linha nova no inicio do ciclo	
		int j;
		
		// Le uma linha
		for(j = 0; file[i] != '\0' && file[i] != '\n' ; i++, j++) line[j] = file[i];
		i++; // para avancar \n 
		line[j] = '\0';			

		// É texto
		if((line[0]) != '$' ){ 
			// se tiver encontrado um resultado deve ignorar
			if(!strcmp(line, SUP_DELIM)){
				do{ 
					j=0;
					while(file[i] != '\0' && file[i] != '\n') line[j++] = file[i++];
					line[j]='\n'; line[j+1]='\0'; i++;
				}while(!strcmp(line, INF_DELIM));

			}
			else{ // caso tenha encontrado uma linha de texto guarda-a
				cmds[cmd_id]->text = strdup(line);
			}
		}
		// É comando
		else if(line[0] == '$'){ 

			cmds[cmd_id]->full_cmd = strdup(line);			
			cmds[cmd_id]->cmd = transformCmdLine(line);
			/* Debbuging */ printf("Transformação bem sucedida - %s\n", cmds[cmd_id]->cmd[0]);
			int x = getDependentNumber(line);
			if(x >= 0) cmds[cmd_id]->my_input_id = cmd_id - x;

			if(x != -1){
				cmds[cmd_id - x]->needs_me = addToArray(cmds[cmd_id - x]->needs_me, cmds[cmd_id - x]->needs_me_len, cmd_id);
				cmds[cmd_id - x]->needs_me_len++;
			}
			cmd_id++;
		}
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
	signal(SIGALRM, error);

	if(argc == 1){
		printf("Especifique um ficheiro\n");
		return -1;
	}

	int nFilho = 0;
	char *path = argv[1];	
	char *buffer;
	int n_cmds = getNumOfCmds(argv[1]);

	// Inicializa estrutura com size = n_cmds
	Comando cmds[n_cmds];
	for(int i=0; i<n_cmds; i++)
		cmds[i] = initComando();


	// Muda o descritor do std_error
	int f_error = open("/tmp/notebook/error.txt", O_CREAT | O_RDWR, 0666);
	if(f_error<0) exit(-1);
	dup2(f_error, 2);


	// Abrir ficheiro
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
		buffer = malloc(fileSize+1);
	}

	// Carregar ficheiro para string Buffer
	while(read(file, buffer, fileSize)>0);
	buffer[fileSize] = '\0';

	close(file);	

	// Parser do ficheiro para a estrutura
	loadCmds(cmds, buffer, fileSize);

	// Cria 2*n_cmds pipes
	int pipeArray[n_cmds*2][2];
	for(int i=0; i < n_cmds*2; i++) pipe(pipeArray[i]);





	for(nFilho =0; nFilho < n_cmds; nFilho++){

		int id = fork();

		// Adiciona o pid do filho ao array global que guarda todos os pids dos filhos
		son_pids = addToArray(son_pids, son_pids_len, id);
		son_pids_len++;

		if(id == 0){
			Comando to_exec = cmds[nFilho];

			for(int i=0; i<n_cmds; i++){
				close(pipeArray[PIPE_READ(i)][1]);
				close(pipeArray[PIPE_WRITE(i)][0]);

				if(i != nFilho){ 
					close(pipeArray[PIPE_READ(i)][0]);
					close(pipeArray[PIPE_WRITE(i)][1]);
				}					
			}

			if(to_exec->my_input_id == -1) // teste se precisa de resultado de outro comando
				close(pipeArray[PIPE_READ(nFilho)][0]);  // se nao precisar fecha
			else 
				dup2(pipeArray[PIPE_READ(nFilho)][0], 0);

			dup2(pipeArray[PIPE_WRITE(nFilho)][1], 1);

			execvp(to_exec->cmd[0], to_exec->cmd);
			exit(-1);
		}
		else{

			id = fork();
			if(id == 0){
				char output_file[100];
				sprintf(output_file, "%scomando%d.txt", FAUX_PATH, nFilho);
				int output_des = open(output_file, O_RDWR | O_CREAT, 0666);	
				int to_send, n_read;
				char* result_buffer = malloc(1024 * sizeof(char));
				Comando to_exec = cmds[nFilho];
				for(int i=0; i<n_cmds; i++){
					if(i != nFilho) close(pipeArray[PIPE_WRITE(i)][0]);						
					if(i != nFilho && elem(to_exec->needs_me, to_exec->needs_me_len, i) == 0)						
						close(pipeArray[PIPE_READ(i)][1]); // fechar descritor de escrita porque nao vai ser preciso enviar resultado a este
					
					close(pipeArray[PIPE_WRITE(i)][1]);
					close(pipeArray[PIPE_READ(i)][0]);

				}
				close(pipeArray[PIPE_READ(nFilho)][1]);

 				while((n_read = read(pipeArray[PIPE_WRITE(nFilho)][0], result_buffer, 1024)) > 0){
						for(int i=0; i< to_exec->needs_me_len; i++){
							to_send = to_exec->needs_me[i];
							write(pipeArray[PIPE_READ(to_send)][1], result_buffer, n_read);
						}
						write(output_des, result_buffer, n_read);
				}	

				for(int i=0; i< to_exec->needs_me_len; i++){ // fechar os descritores para escrita
					to_send = to_exec->needs_me[i];
					close(pipeArray[PIPE_READ(to_send)][1]);
				}

				close(pipeArray[PIPE_WRITE(nFilho)][0]);
				close(output_des);
				exit(0);		
			}
		}
	}


	// Filho que verifica se foi escrito algo no ficheiro de erros. Se sim, a execução é terminada
	int check_error = fork();
	int n_read;
	char a;

	if(check_error == 0){
		while((n_read = read(f_error, &a, 1)) == 0);

		if(n_read > 0) alarm(0); // Quando conseguiu ler algo no ficheiro
		else exit(0);
	}
	close(f_error);


	for(int i=0; i<n_cmds; i++){
		close(pipeArray[PIPE_WRITE(i)][0]);
		close(pipeArray[PIPE_WRITE(i)][1]);
		close(pipeArray[PIPE_READ(i)][0]);
		close(pipeArray[PIPE_READ(i)][1]);
	}

	for(int i = 0; i<n_cmds*2;  i++){
		wait(NULL);
	}

	// Mata o filho que estava encarregue de verificar se existiu algum erro durante a execução
	kill(check_error, SIGKILL);



/*
	// Reescrita no ficheiro
	int n_read;
	int fid = open(argv[1],O_TRUNC | O_WRONLY,0666);
	
	for (int i=0; i<n_cmds; i++) {
		write(fid,cmds[i]->text,strlen(cmds[i]->text));
		write(fid,"\n",1);
		write(fid,cmds[i]->full_cmd,strlen(cmds[i]->full_cmd));
		write(fid,"\n",1);

		char res_buf;

		char output_file[0];
		sprintf(output_file, "%scomando%d.txt", FAUX_PATH, i);
		int output_src = open(output_file, O_RDONLY);

		write(fid,SUP_DELIM,4);
		while((n_read=read(output_src,&res_buf,1))>0){
			write(fid,&res_buf,n_read);
		}
		write(fid,INF_DELIM,4);
		close(output_src);
	}
	
	close(fid);
*/
	return 0;
}
