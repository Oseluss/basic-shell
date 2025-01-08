#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>


enum {
  N_maxarg = 10,
  Line_maxsize = 1024,
  Path_maxsize = 100,
  Token_maxsize = 100,
  Comand_maxnum = 10,
  Var_maxnum = 2,
  Var_maxlenth = 100,
  Fail = 1,
  Success = 0,
  Exit = 2,
  Permision = S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH |S_IWOTH,
  Cd = '1', //Codification to the builting comands
  Declarevar = '0'
};

enum {
  STDOUT = 1,
  STDIN = 0
};

//Variables globales

char *var[Var_maxnum][2];

struct Comand {
  char *comand_name;
  char * comand_arg[N_maxarg];
  int argcount;
  int pid;
};

struct Fd_pair {
  int fd[2];
};

struct Rediretion_files {
  char *infile;
  char *outfile;
  int fd_files[2]; // 1 to outfile 0 to in file
};

struct Comands {
    struct Comand comand[Comand_maxnum];
    int num_comands;
    struct Fd_pair pipes[Comand_maxnum-1];
    struct Rediretion_files refiles;
    int background_flag;
};

struct Token_list {
	char *list[Token_maxsize]; // max_size 100
	int size;
};

typedef struct Comand Comand;
typedef struct Comands Comands;
typedef struct Fd_pair Fd_pair;
typedef struct Rediretion_files Rediretion_files;


int
getcomand(char * line, Comand *comand) {
  int j;
	char *token, *str;
	char *saveptr;
	char *separator = " \t\n";
	comand->argcount = 0;
  //Extract the comand_name
  str = line;
  token = strtok_r(str, separator, &saveptr);
  if (token == NULL) {
    printf("Error extracting the name\n");
    return Fail;
  }
  comand->comand_name=token;
  // Estracting the arguments
  // First argument how do you call the comand
  comand->comand_arg[0]=token;
  comand->argcount = 1;
  //Rest of argument
	for (j = 0,str = NULL; j < N_maxarg-2; j++, str = NULL) {
		token = strtok_r(str, separator, &saveptr);
		if (token == NULL)
			break;
		comand->comand_arg[j+1]=token;
		comand->argcount = comand->argcount + 1;
	}
	if (j ==  N_maxarg - 2) {
		printf("the number of armunt exceeds the maximum number \n");
		printf("Only use %d argmunets \n", N_maxarg);
	}
  //The last argument always is NULL
  comand->comand_arg[j+1]=NULL;
  return Success;
}

void
printcomand(Comand comand) {
  printf("The comand is :%s \n",comand.comand_name);
  int j = 0;
  while (comand.comand_arg[j]!=NULL) {
  	printf("El argumento %d es: %s \n",j,comand.comand_arg[j]);
    j = j + 1;
  }
}

int
searchexinpwd(char *comand_name,char *path) {
  char *pwd = NULL;
  pwd = getenv("PWD"); // Como solo se puede introducir una variable
  snprintf(path,Path_maxsize,"%s/%s",pwd,comand_name);
  if (access(path,X_OK)==0) {
    return 1;
  }
  return 0;
}

void
extract_tokens( char *var_value,struct Token_list * list) {
	int j;
	char *token, *str;
	char *saveptr;
	char *separator = ":";
	list->size = 0;
	for (j = 0, str = var_value; j < Token_maxsize; j++, str = NULL) {
		token = strtok_r(str, separator, &saveptr);
		if (token == NULL)
			break;
		list->list[j]=token;
		list->size = list->size + 1;
	}
}

void
printtokens(struct Token_list token_list) {
	int i;
	for (i=0; i < token_list.size; i++) {
		printf("%s\n", token_list.list[i]);
	}
}

int
searchexeinpath(char *comand_name,char *path) {
  char *paths_value;
  paths_value = malloc(Token_maxsize*Path_maxsize);
  snprintf(paths_value,Token_maxsize*Path_maxsize,"%s",getenv("PATH"));
  struct Token_list token_list;
  if (paths_value!=NULL) {
    extract_tokens(paths_value,&token_list);
    for (int i=0; i < token_list.size; i++) {
      snprintf(path,Path_maxsize,"%s/%s",token_list.list[i],comand_name);
      if (access(path,X_OK)==0) {
        free(paths_value);
        return 1;
      }

	  }
  }
  free(paths_value);
  return 0;
}

int
provecd (char *comand_name){
  if ((*comand_name == 'c') && (*(comand_name + 1) == 'd') && (*(comand_name + 2) == '\0')) {
    return Success;
  }
  return Fail;
}

int provedeclarevar(char *comand_name) {
  char *str;
  str = comand_name;
  int found = Fail;
  while (*str != '\0') {
    if (*str == '=') {
      if (found == Success) {
        return Fail;
      } else {
        found = Success;
      }
    }
    str = str + 1;
  }
  return found;
}

int
searchbuiltin (char *comand_name, char *path) {
  if (provecd(comand_name) == Success) {
    snprintf(path,Path_maxsize,"%c%c",'\0',Cd);
    return 0;
  }
  if (provedeclarevar(comand_name) == Success) {
    snprintf(path,Path_maxsize,"%c%c",'\0',Declarevar);
    return 0;
  }
  return 1;
}

int
searchexepath(char *comand_name,char *path) {
  if (searchbuiltin(comand_name,path)==0){
    return 1;
  }
  if (searchexinpwd(comand_name,path)) {
    return 1;
  }
  if (searchexeinpath(comand_name,path)) {
    return 1;
  }

  return 0;
}

void
close_pipes(Fd_pair *pipes_list,int num_pipes) {
  int w;
  for (int i = 0; i < num_pipes; i++) {
    w = close(pipes_list[i].fd[0]);
    if (w == -1) {
      printf("Error closing the pipe %d\n",i);
    }
    w = close(pipes_list[i].fd[1]);
    if (w == -1) {
      printf("Error closing the pipe %d\n",i);
    }
  }
}

void
establishpipe(Fd_pair *pipes_list, int num_comand, int num_comands) {
  if (num_comands > 1) {
    if (num_comand == 0) {
      close(STDOUT);
      dup(pipes_list[0].fd[1]);
    } else if(num_comand == (num_comands-1)) {
      close(STDIN);
      dup(pipes_list[num_comands-2].fd[0]);
    } else {
      close(STDIN);
      dup(pipes_list[num_comand-1].fd[0]);
      close(STDOUT);
      dup(pipes_list[num_comand].fd[1]);
    }

    close_pipes(pipes_list,num_comands-1);
  }
}

void
printpipes(Fd_pair *pipes_list,int num_pipes) {
  for (int i = 0; i < num_pipes; i++) {
    printf("inpipe: %d, outpie: %d \n",pipes_list[i].fd[1],pipes_list[i].fd[2]);
  }
}

void
close_redirections(Rediretion_files refiles) {
  if (refiles.infile != NULL) {
    close(refiles.fd_files[0]);
  }
  if (refiles.outfile != NULL) {
    close(refiles.fd_files[1]);
  }
}

void
establishredirections(int num_comand, int num_comands, Rediretion_files refiles, int background_flag) {
  if ((num_comand == 0) && ((refiles.infile != NULL) || background_flag)) {
    close(STDIN);
    dup(refiles.fd_files[0]);
  }
  if ((num_comand == (num_comands-1)) && (refiles.outfile != NULL)) {
    close(STDOUT);
    dup(refiles.fd_files[1]);
  }
  close_redirections(refiles);
}

int
cd(char *comand_args[]) {
  int argc = 0;
  while (comand_args[argc] != NULL) {
    argc = argc + 1;
  }
  if (argc > 2) {
    printf("Usage: To mach arguments cd [path]\n");
    return 2;
  }
  if (argc == 2) {
    if (chdir(comand_args[1]) == -1){
      perror("chdir");
      return 2;
    }
    if (setenv("PWD",comand_args[1],1) == -1) {
      perror("setenv");
      return 2;
    }
  } else {
    char *home;
    home = getenv("HOME");
    if (home == NULL) {
      printf("HOME dont set properly");
      return 0;
    }
    if (chdir(home) == -1){
      perror("chdir");
      return 2;
    }
    if (setenv("PWD",home,1) == -1) {
      perror("setenv");
      return 2;
    }
  }
  return 0;
}

int declarevar(char *comand_args[]) {
  int argc = 0;
  while (comand_args[argc] != NULL) {
    argc = argc + 1;
  }
  if (argc > 1) {
    printf("Usage: To many arguments declare a var\n");
    return 2;
  }
  char *str;
  str = comand_args[0];
  int found = 0;
  while ((*str != '\0') && !found) {
    if (*str == '=') {
        *str ='\0';
        found = 1;
    }
    str = str + 1; //puntero al valor de la variable
  }
  int i = 0;
  while (var[i][0] != NULL) {
    i = i + 1;
  }
  if (i == Var_maxnum){
    printf("Max num of var reached");
    return Fail;
  }
  var[i][0] = malloc(Var_maxlenth);
  var[i][1] = malloc(Var_maxlenth);
  snprintf(var[i][0],Var_maxlenth,"%s",comand_args[0]);
  snprintf(var[i][1],Var_maxlenth,"%s",str);
  var[i+1][0] = NULL;
  var[i+1][1] = NULL;
  return Success;

}

void
executebuiltin(char *path, char *comand_args[]) {
  switch (*(path + 1)) {
    case Cd:
      cd(comand_args);
      break;
    case Declarevar:
      declarevar(comand_args);
      break;
    default:
      printf("ESTO NO PUEDE PASAR SI PASA REVISAR EL CODIGO\n");
  }
}

int
executecomand(Comands *comands,int num_comand,char *path) {
  comands->comand[num_comand].pid = fork();
  if (comands->comand[num_comand].pid == -1) {
   printf("Fork error");
  }
  if (comands->comand[num_comand].pid == 0) { /* Code executed by child */
    establishpipe(comands->pipes,num_comand,comands->num_comands);
    establishredirections(num_comand,comands->num_comands,comands->refiles,comands->background_flag);
    if (*(path) == '\0') {
      // There are a builtin
      executebuiltin(path, comands->comand[num_comand].comand_arg);
      return Exit;
    } else {
      execv(path, comands->comand[num_comand].comand_arg);
      exit(2);
    }
  } else {                    /* Code executed by parent */
    if (*(path) == '\0') {
      if (*(path + 1) == Cd) {
        cd(comands->comand[num_comand].comand_arg);
      }
      if (*(path + 1) == Declarevar) {
        declarevar(comands->comand[num_comand].comand_arg);
      }
    }
  }
  free(path);
  return Success;
}

int
getcomands(char *line,Comands *comands) {
  int j;
	char *token, *str;
	char *saveptr;
	char *separator = "|\n";
	comands->num_comands = 0;
  str = line;
  if ((*str) == '|') {
    return Fail;
  }
	for (j = 0; j < Comand_maxnum; j++, str = NULL) {
		token = strtok_r(str, separator, &saveptr);
		if (token == NULL) {
    	break;
    }
		if(getcomand(token,&(comands->comand[j])) == Fail){
      return Fail;
    }
    comands->num_comands = comands->num_comands + 1;
	}
	if (j ==  Comand_maxnum) {
		printf("The numer of comands exceeds the max \n");
		printf("Only use %d comands \n", Comand_maxnum);
	}
  return Success;
}

int
setandtestname(char **file_name) {
  if (*file_name == NULL) {
    return Success;//No value no redirection
  }
	char *token, *str;
	char *saveptr;
	char *separator = " \t\n";
  str = *file_name;
  if ((*str) == '|') {
    return Fail;
  }
  token = strtok_r(str, separator, &saveptr);
  if (token == NULL) {
    printf("Usage: no file to the redirect");
    return Fail;
  }
  *file_name = token;
  str = NULL;
  token = strtok_r(str, separator, &saveptr);
  if (token == NULL) {
    return Success; //One word only
  }
  printf("Usege: cant be 2 words to a file name\n");
  return Fail; // Two words
}

int
getredirections(char *line, char **infile_p, char **outfile_p) {
	char *outfile,*infile,*str;
  outfile = NULL;
  infile = NULL;
  *infile_p = infile;
  *outfile_p = outfile;
  int notfound = 1;
  str = line;
  if (((*str) == '<') ||((*str) == '>')) {
    printf("Usage: no comand to redirrection \n");
    return Fail;
  }
  //The first redirection
  while ((*str != '\0') && notfound) {
    if (*str == '<') {
      //input file found
      infile=str+1;
      *str = '\0';
      notfound = 0;
    }
    if (*str == '>') {
      //output file found
      outfile=str+1;
      *str = '\0';
      notfound = 0;
    }
    str = str + 1;
  }
  // The second redirection
  while (*str != '\0') {
    if (*str == '<') {
      //input file found
      if (infile == NULL) {
        infile=str+1;
        *str = '\0';
      } else {
        printf("Usage: Cant be 2 infiles\n");
        return Fail;
      }
    }
    if ((*str == '>')) {
      //Output file found
      if (outfile == NULL) {
        outfile=str+1;
        *str = '\0';
      } else {
        printf("Usage: be 2 outfiles\n");
        return Fail;
      }
    }
    str=str+1;
  }
  if ((setandtestname(&outfile)==Success) && (setandtestname(&infile)==Success)) {
    *infile_p = infile;
    *outfile_p = outfile;
    return Success;
  }
  return Fail;
}

int
getbacground(char *line, int *flag) {
  char *str;
  int notfound = 1;
  str = line;
  while ((*str != '\0') && notfound) {
    if (*str == '&') {
      //getbacground found
      *str = '\0';
      notfound = 0;
    }
    str = str + 1;
  }
  while ((*str != '\0')) {
    if ((*str != ' ')&&(*str != '\t')&&(*str != '\n')) {
      return Fail;
    }
    str = str + 1;
  }
  *(flag) = !notfound;
  return Success;

}

int
provesutitution(char **string) { //Success in case of no sutitution or correct sutitution
  char *str;
  str = *string;
  if (*str == '$') {
    str = str + 1;
    int j = 0;
    int found = 0;
    while ((var[j][0] != NULL) && !found) {
      if (strcmp(str,var[j][0]) == 0) {
        found = 1;
      }
      j = j + 1;
    }
    if (!found) {
      printf("error: var %s does not exist\n",str);
      return Fail;
    } else {
      *string = var[j-1][1];
      return Success;
    }
  } else {
    return Success;
  }
}

int
getsutitutions(Comands *comands) {
  int i = 0;
  int j = 0;
  for (i = 0; i < comands->num_comands; i++) {
    if (provesutitution(&(comands->comand[i].comand_name)) == Fail) {
      return Fail;
    }
    for (j = 0; j < comands->comand[i].argcount; j++) {
      if (provesutitution(&(comands->comand[i].comand_arg[j])) == Fail) {
        return Fail;
      }
    }
  }
  return Success;
}

int
proccesline(char *line,Comands *comands){
  if (getbacground(line,&(comands->background_flag))==Fail) {
    return Fail;
  }
  if (getredirections(line,&(comands->refiles.infile),&(comands->refiles.outfile)) == Fail) {
    return Fail;
  }
  if (getcomands(line,comands) == Fail) {
    return Fail;
  }
  if (getsutitutions(comands) == Fail) {
    return Fail;
  }
  if (comands->refiles.infile != NULL) {
    comands->refiles.fd_files[0] = open(comands->refiles.infile,O_RDONLY);
    if (comands->refiles.fd_files[0] == -1) {
      printf("usage: inputfile not found \n");
      return Fail;
    }
  } else if ((comands->background_flag)) {
    //Sending the estandart imput of the proces to the dev/nill
    comands->refiles.fd_files[0] = open("/dev/null",O_RDONLY);
    if (comands->refiles.fd_files[0] == -1) {
      printf("usage: inputfile not found \n");
      return Fail;
    }
  }
  if (comands->refiles.outfile != NULL) {
     comands->refiles.fd_files[1] = open(comands->refiles.outfile,O_WRONLY|O_CREAT|O_TRUNC,Permision);
     if (comands->refiles.fd_files[1] == -1) {
       printf("usage: can no create the outputfile \n");
       return Fail;
     }
  }
  return Success;

}

void
createpipes(Fd_pair *pipes_list,int num_pipes) {
  for (size_t i = 0; i < num_pipes; i++) {
    if(pipe(pipes_list[i].fd)==-1) {
      perror("pipe");
    }
  }
}

int
setcomandspaths(char **path_list, Comand *comand_list,int num_paths){
  for (int i = 0; i < num_paths; i++) {
    if(!searchexepath(comand_list[i].comand_name,path_list[i])){
      printf("Usage: Comand not found\n");
      return Fail;
    };
  }
  return Success;
}

int
executecomands(Comands *comands) {
  char *path_list[comands->num_comands];
  for (int i = 0; i < comands->num_comands; i++) {
    path_list[i] = malloc(Path_maxsize);
  }
  if (setcomandspaths(path_list,comands->comand,comands->num_comands) == Success) {
    for (int j = 0; j < comands->num_comands; j++) {
      if (executecomand(comands,j,path_list[j]) == Exit) {
        for (int i = j; i < comands->num_comands; i++) {
          free(path_list[i]);
        }
        return Exit;
      }
    }
    return Success;
  } else {
    for (int i = 0; i < comands->num_comands; i++) {
      free(path_list[i]);
    }
    return Fail;
  }
}

void
waitchilds(Comand *comand, int num_comands) {
  int wstatus;
  pid_t w;
  for (int j = 0; j < num_comands; j++) {
    w = waitpid(comand[j].pid, &wstatus, WUNTRACED | WCONTINUED);
    if (w == -1) {
      perror("waitpid");
    }
    if (WEXITSTATUS(wstatus) != 0){
      //printf("some error in the childs \n");
   	}
  }
}

void printvar() {
  int j = 0;
  while (var[j][0] != NULL) {
    printf("%s\n",var[j][0]);
    printf("%s\n",var[j][1]);
    j = j + 1;
  }
  printf("%d\n",j );
}

int main(int argc, char const *argv[]) {
  int exit;
  char *line,*aux_p;
  line = malloc(Line_maxsize);
  aux_p = line;
  var[0][0] = NULL;
  var[0][1] = NULL;
  Comands comands;
  for (exit = 0; !exit ;) {
//    printf("~%s$ ",getenv("PWD"));
    if (fgets(line,Line_maxsize,stdin) == NULL){
      break;
    }
    if (proccesline(line,&comands) == Success) {
      if (comands.num_comands > 0) {
        createpipes(comands.pipes,comands.num_comands-1);
        switch (executecomands(&comands)) {
          case Success:
            close_pipes(comands.pipes,comands.num_comands-1);
            close_redirections(comands.refiles);
            if (!(comands.background_flag)) {
              waitchilds(comands.comand,comands.num_comands);
            }
            break;
          case Fail:
            close_redirections(comands.refiles);
            close_pipes(comands.pipes,comands.num_comands-1);
            break;
          case Exit:
            close_redirections(comands.refiles);
            close_pipes(comands.pipes,comands.num_comands-1);
            exit = 1;
        }
      }
    } else {
      printf("Usage: syntax error\n");
    }
  }
  int j = 0;
  while (var[j][0] != NULL) {
    free(var[j][0]);
    free(var[j][1]);
    j = j + 1;
  }
  free(aux_p);
  return 0;
}
