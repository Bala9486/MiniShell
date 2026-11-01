#include "minishell.h"

/*Builtin commands */
char *builtins[] = {"echo", "printf", "read", "cd", "pwd", "pushd", "popd", "dirs", "let", "eval",
						"set", "unset", "export", "declare", "typeset", "readonly", "getopts", "source",
						"exit", "exec", "shopt", "caller", "true", "type", "hash", "bind", "help", "jobs", "fg", "bg", NULL};

/* Declare array of pointer(FSSD -> 2D array) Globally to store external commands */
char *external_cmds[154];

int status;
pid_t pid;

char prompt[50] = "Minishell$";
char input_string[100];

struct data
{
    int s_pid;
    char s_cmd[100];
};
int count;
struct data *list;

void at_list(pid_t pid,char *input_string)
{
    /* Allocate memory for struct */
    list = realloc(list,(count+1)*sizeof(struct data));

    /* add the pid and cmd */
    list[count].s_pid = pid;
    strcpy(list[count].s_cmd,input_string);

    count++;
}
void delete_list()
{
    count--;
    
    /* Deallocate the memory of struct */
    list = realloc(list,count*sizeof(struct data));
}

void signal_handler(int num)
{
    if(num == SIGINT)
    {
        if(pid == 0)
	{
		printf("\n%s ",prompt);
		fflush(stdout);
	}
    }
    else if(num == SIGTSTP)
    {
        if(pid > 0)
        {
            at_list(pid,input_string);
        }
        else
        {
            printf("\n%s ",prompt);
            fflush(stdout);
        }
    }
    else if(num == SIGCHLD)
    {
        waitpid(-1,&status,WNOHANG);
    }
}
void N_pipe(char **argv,int pipe_count)
{
    int index[pipe_count+1];
    int j = 1;
    index[0] = 0;
    /* Take a backup of stdin and stdout */
    int back_stdin = dup(0);
    int back_stdout = dup(1);

    /* Replace | as NULL */
    for(int i=0; argv[i]!=NULL; i++)
    {
        if(strcmp(argv[i],"|") == 0)
        {
            argv[i] = NULL;

            /* Store start index for next command */
            index[j] = i+1;
            j++;
        }
    }

    int fd[2];

    /* According to pipe_count execute commands */
    for(int i=0; i<pipe_count+1; i++)
    {
       /* Last iter don't execute */
       if(i != pipe_count)
       {
            /* Create pipe */
            if(pipe(fd) == -1)
                perror("pipe");
       }

        int pid = fork();
        if(pid == 0)
        {
            /* Last iter don't execute */
            if(i != pipe_count)
            {
                /* close the read end of pipe */
                close(fd[0]);

                /* Redirect the stdout to write end of pipe */
                dup2(fd[1],1);
            }

            /* Execute commands one by one */
            execvp(argv[index[i]], argv+index[i]);
        }
        else if(pid > 0)
        {
            /* Last iter don't execute */
            if(i != pipe_count)
            {
                /* Close the write end of pipe */
                close(fd[1]);

                /* Redirect the stdin to read end of pipe */
                dup2(fd[0],0);
            }
        }
    }
     /* Close all the child resources by Wait */
    for(int i = 0; i < pipe_count + 1; i++)
    {
        wait(NULL);
    }

    /* Redirect of stdin and stdout */
    dup2(back_stdin,0);
    dup2(back_stdout,1);
}
void echo(char *input_string,int status)
{
    /* Check the input_string is echo$$ or not */
    if(strcmp(input_string,"echo $$") == 0)
    {
        /* Print the minishell process id */
        printf("%d\n",getpid());
    }
    else
    {
        /* Check the input_string is echo$? or not */
        if(strcmp(input_string,"echo $?") == 0)
        {
            /* print the exit status using WEXITSTATUS */
                printf("Process %d terminated with exit code %d\n",getpid(),WEXITSTATUS(status));
        }
        else
        {
            /* Check the input_string is echo$SHELL or not */
            if(strcmp(input_string,"echo $SHELL") == 0)
            {
                /* print minishell executable path */
                char *ret = getenv("SHELL");
                printf("%s\n",ret);
            }
            else
            {
                printf("Command not found\n");
            }
        }
    } 
}

/* Extract the external commands from external.txt file */
void extract_external_commands(char **external_cmds)
{
    /* Open the extern.txt file in read mode */
    int fd = open("extern.txt",O_RDONLY);
    if(fd == -1)
    {
        perror("open");
        return;
    }

    /* Read character by character upto end of file */
    char ch;
    char cmds[100];
    int col = 0;
    int i = 0;

    while(read(fd,&ch,1) > 0)
    {
        /* Check the character is '\n' or not*/
        if(ch == '\n')
        {
            /* Store '\0'*/
            cmds[i] = '\0';

            /* Allocate memory for external_cmds of string as local array size+1 */ 
            external_cmds[col] = malloc(strlen(cmds)+1);
         
            /* Copy the local array to external_cmds */
            strcpy(external_cmds[col],cmds);
            col++;
            i=0;
        }
        else
        {
            cmds[i] = ch;
            i++;
        }
    }
    /* Handle last command if file doesn't end with newline */
    if (i > 0)
    {
        cmds[i] = '\0';

        external_cmds[col] = malloc(strlen(cmds) + 1);
        strcpy(external_cmds[col], cmds);
        col++;
    }

    external_cmds[col] = NULL;

    close(fd);
}

/* Execute the external commands */
void execute_external_commands(char *input_string)
{
    /* Convert the input_string into 2D array */
    char *argv[20];
    int i = 0;
    int pipe_count = 0;

    char *token = strtok(input_string," ");
    
    while(token != NULL)
    {
        if(strcmp(token,"|") == 0)
        {
            pipe_count++;
        }

        argv[i] = token;
        i++;

        /* Continue from where the previous strtok() stopped */
        token = strtok(NULL," ");
    }
    argv[i] = NULL;

    /* Check pipe count */
    if(pipe_count == 0)
    {
        int status1;
            /* Execute command by using execvp */
            execvp(argv[0],argv);
    }
    else if(pipe_count > 0)
    {
        /* Call the N_pipe function to handle multiple commands by using pipe */
        N_pipe(argv,pipe_count);
    }
    
}

/* Execute the builtin commands */
void execute_internal_commands(char *input_string)
{
    /* Check the input_string is fg or not */
    if(strcmp(input_string,"fg") == 0)
    {
        if(count > 0)
        { 
            pid_t fg_pid =  list[count-1].s_pid;          
            /* Continue the last stop process in foreground */
            kill(fg_pid,SIGCONT);

            /* Clear the resources */
            waitpid(fg_pid,&status,WUNTRACED);

            delete_list();
        }
    }
    /* Check the input_string is bg or not */
    else if(strcmp(input_string,"bg") == 0)
    {
        if(count > 0)
        {
            /* Continue the last stop process in background */
            kill(list[count-1].s_pid,SIGCONT);

            signal(SIGCHLD,signal_handler);
            delete_list();
        }
    }
    /* Check the input_string is jobs or not */
    else if(strcmp(input_string,"jobs") == 0)
    {
        /* List all stopped process */
        for(int i=0; i<count; i++)
        {
            printf("[%d]+ Stopped    %s\n",list[i].s_pid,list[i].s_cmd);
        }
    }
    /* Check the input_string is pwd or not */
    else if(strcmp(input_string,"pwd") == 0)
    {
        char *buffer = getcwd(NULL,0);
        printf("%s\n",buffer);
    }
    else
    {
        /* Check the input_string is having cd or not */
        if(strncmp(input_string,"cd",2) == 0)
        {
            int ret = chdir(input_string+3);
            if(ret == 0)
            {
                printf("Successfully change the path\n");
            }
            else
            {
                perror("chdir Failure");
            }
        }
        else
        {
            /* Check the input string is exit or not */
            if(strcmp(input_string,"exit") == 0)
            {
                /* Terminate all the process */
                exit(0);
            }
             
            /* Check the input_string is having echo or not */
            if(strncmp(input_string,"echo",4) == 0)
            {
                /* Call the echo function */
                echo(input_string,status);
            }
            else
            {
                printf("Error: Command not found\n");

            }
        }
    }
} 

/* Check the command type is builtin or extern */
int check_command_type(char *command)
{
    /* Check the command is builtin or not */
    for(int i=0; builtins[i]!=NULL; i++)
    {
        if(strcmp(command , builtins[i]) == 0)
        {
            return BUILTIN;
        }
    }
    
    /* Check the command is external or not */
    for(int i=0; external_cmds[i]!=NULL; i++)
    {
        /* Check the command is external or not */
        if(strcmp(command ,external_cmds[i]) == 0)
        {
            return EXTERNAL;
        }
    }
    return NO_COMMAND;
}

/* Get the command from user */
char *get_command(char *input_string)
{
    char str[20];
    int i = 0;

    /* Get the first word from input_string */
    while(input_string[i]!=' ' && input_string[i]!= '\0')
    {
        str[i] = input_string[i];
        i++;
    }

    str[i] = '\0';
    char *ptr = malloc(strlen(str)+1);

    strcpy(ptr,str);

    /* Return the word */
    return ptr;
}

/* Change the prompt name */
void copy_change(char *prompt,char *input_string)
{
    /* Check if the input starts exactly with "PS1" without spaces */
    if(strncmp(input_string,"PS1=",4) == 0)
    {
        /* Copy the value after "PS1=" into the prompt buffer */
        strcpy(prompt,input_string+4);
    }
    else
    {
        printf("Error: Invalid PS1 Syntax! Usage: PS1=new_prompt\n");
    }
}

/* Get the input from user */
void scan_input(char *prompt,char *input_string)
{
    /* Register SIGINT and SIGTSTP */
    signal(SIGINT,signal_handler);
   signal(SIGTSTP,signal_handler);

    while(1)
    {
        /* Print the prompt */
        printf("%s ",prompt);
        fflush(stdout);

        /* Read the input string from user */
        scanf("%[^\n]",input_string);
        getchar();

        /* If only Enter is pressed */
        if(strlen(input_string) == 0)
        {
          continue;
        }

        /* Check the input_string is having PS1 or not */
        if(strstr(input_string,"PS1") != NULL)
        {
            /* Change the prompt name */
            copy_change(prompt,input_string);

            /* Back to the prompt again */
            continue;
        }
        else
        {
            /* Get command */
            char *cmd = get_command(input_string);
            if(cmd == NULL)
              continue;

            /* Check the command is builtin or external */    
            int ret = check_command_type(cmd);
            if(ret == BUILTIN)
            {
                printf("It's a builtin command\n");
                execute_internal_commands(input_string);
            }
            else if(ret == EXTERNAL)
            {
                printf("It's a external command\n");

                /* Create a child process by using fork() system call */
                pid = fork();

                /* Child process */
                if(pid == 0)
                {
                    signal(SIGINT,SIG_DFL);
                    signal(SIGTSTP,SIG_DFL);

                    execute_external_commands(input_string);
                    exit(0);
                }
                else if(pid > 0) /* Parent process */
                {
                    /* Wait to terminte or stop the child process */
                    waitpid(pid,&status,WUNTRACED);
                    pid = 0;
                }
            }
            else
            {
                printf("%s : command not found\n",cmd);
            }        
        }            
    }
}
int main()
{
   /* Call the function to extract the external commands from extern.txt file */
    extract_external_commands(external_cmds);

    /* Clear the terminal */
    system("clear");

    /* Call the function to read prompt from user */
    scan_input(prompt, input_string);

    return 0;
}
