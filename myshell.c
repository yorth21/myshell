#include <sys/wait.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#define BUFSIZE 1024
#define TOK_BUFSIZE 64
#define TOK_DELIM " \t\r\n\a"

int detectar_segundo_plano(char **args)
{
	int background = 0;
	for (int i = 0; args[i] != NULL; i++)
	{
		if (strcmp(args[i], "&") == 0)
		{
			background = 1;
			args[i] = NULL;
			break;
		}
	}
	return background;
}

void detectar_redireccionamiento(char **args, int *redirect_input, int *redirect_output, int *append, char **input_filename, char **output_filename)
{
	*redirect_input = 0;
	*redirect_output = 0;
	*append = 0;
	*input_filename = NULL;
	*output_filename = NULL;
	char **p = args;
	while (*p != NULL)
	{
		if (strcmp(*p, ">") == 0 || strcmp(*p, ">>") == 0)
		{
			if (strcmp(*p, ">") == 0)
				*redirect_output = 1;
			else
				*redirect_output = *append = 1;
			*output_filename = *(p + 1);
			// Eliminar el operador de redireccionamiento y el nombre del archivo
			char **q = p;
			while (*q != NULL)
			{
				*q = *(q + 2);
				q++;
			}
			p--;
		}
		else if (strcmp(*p, "<") == 0)
		{
			*redirect_input = 1;
			*input_filename = *(p + 1);
			// Eliminar el operador de redireccionamiento y el nombre del archivo
			char **q = p;
			while (*q != NULL)
			{
				*q = *(q + 2);
				q++;
			}
			p--;
		}
		p++;
	}
}

void manejar_redireccionamiento(int redirect_input, int redirect_output, int append, char *input_filename, char *output_filename)
{
	int input_fd, output_fd;

	if (redirect_output) // Salida
	{
		// append=1; NO sobreescriba el archivo
		if (append)
		{
			output_fd = open(output_filename, O_WRONLY | O_CREAT | O_APPEND, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
		else
		{
			output_fd = open(output_filename, O_WRONLY | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
		}
		if (output_fd == -1)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (dup2(output_fd, STDOUT_FILENO) == -1)
		{
			perror("dup2");
			exit(EXIT_FAILURE);
		}
	}

	if (redirect_input) // Entrada
	{
		input_fd = open(input_filename, O_RDONLY);
		if (input_fd == -1)
		{
			perror("open");
			exit(EXIT_FAILURE);
		}
		if (dup2(input_fd, STDIN_FILENO) == -1)
		{
			perror("dup2");
			exit(EXIT_FAILURE);
		}
	}
}

int ejecutar_comando(char **args)
{
	// Detectar si toca ejecutar en segundo plano
	int background = detectar_segundo_plano(args);

	// Variables para el redireccionamiento
	int redirect_input = 0;
	int redirect_output = 0;
	int append = 0;
	char *input_filename = NULL;
	char *output_filename = NULL;
	detectar_redireccionamiento(args, &redirect_input, &redirect_output, &append, &input_filename, &output_filename);

	pid_t pid = fork();
	if (pid == 0)
	{
		// Proceso hijo
		manejar_redireccionamiento(redirect_input, redirect_output, append, input_filename, output_filename);
		execvp(args[0], args);
		printf("\033[1;31mError. El comando digitado no existe\e[0m\n");
		exit(EXIT_FAILURE);
	}
	else if (pid < 0)
	{
		perror("fork");
	}
	else
	{
		// Proceso padre
		if (background)
		{
			// Imprimir el PID del proceso en segundo plano
			printf("Proceso en segundo plano con PID %d\n", pid);
		}
		else
		{
			// Esperar a que termine el proceso hijo
			int status;
			waitpid(pid, &status, 0);
		}
	}
	return 1;
}

int comando(char **args)
{
	// Validar los comandos y programar los que no funcionan con el execvp
	if (strcmp(args[0], "quit") == 0 || strcmp(args[0], "exit") == 0)
	{
		printf("\033[1;31mHasta la vista baby...\e[0m\n");
		return 0;
	}

	if (strcmp(args[0], "environ") == 0)
		args[0] = "env";

	if (strcmp(args[0], "clr") == 0)
		args[0] = "clear";

	if (strcmp(args[0], "pause") == 0)
	{
		printf("Presione Enter para continuar...");
		fflush(stdout);
		while (getchar() != '\n')
			;
		return 1;
	}

	if (strcmp(args[0], "cd") == 0)
	{
		if (args[1] == NULL)
		{
			// Si no se especifica un directorio, cambiar al directorio de inicio del usuario
			chdir(getenv("HOME"));
		}
		else
		{
			if (chdir(args[1]) != 0)
			{
				perror("chdir");
			}
		}
		return 1;
	}

	if (strcmp(args[0], "help") == 0)
	{
		int count = 0;
		while (args[count] != NULL)
		{
			count++;
		}

		char **new_args = NULL;
		new_args = malloc(sizeof(char *) * (count + 2));
		new_args[0] = "more";
		new_args[1] = "readme";
		// Cuando el "hlep" viene sin argumentos
		if (count == 1)
		{
			new_args[2] = NULL;
			return ejecutar_comando(new_args);
		}
		else
		{
			// Agregar los demas argumentos al nuevo array
			for (int i = 1; i < count; i++)
			{
				new_args[i + 1] = args[i];
			}
			new_args[count + 1] = NULL;
			int estado = ejecutar_comando(new_args);
			free(new_args);
			return estado;
		}
	}

	return ejecutar_comando(args);
}

char *leer_linea(void)
{
	char *buffer = NULL;
	size_t bufsize = 0;
	ssize_t caracteres_leidos = getline(&buffer, &bufsize, stdin);
	if (caracteres_leidos == -1)
	{
		if (feof(stdin))
		{
			exit(EXIT_SUCCESS);
		}
		else
		{
			perror("getline");
			exit(EXIT_FAILURE);
		}
	}
	// Eliminar el salto de línea al final de la línea leída
	buffer[strcspn(buffer, "\n")] = '\0';
	return buffer;
}

char **dividir_linea(char *line)
{
	int bufsize = TOK_BUFSIZE, posicion = 0;
	char **tokens = malloc(bufsize * sizeof(char *));
	char *token, **tokens_backup;
	if (!tokens)
	{
		printf("Error, comando no existente\n");
		exit(EXIT_FAILURE);
	}
	token = strtok(line, TOK_DELIM);
	while (token != NULL)
	{
		tokens[posicion] = token;
		posicion++;
		if (posicion >= bufsize)
		{
			bufsize += TOK_BUFSIZE;
			tokens_backup = tokens;
			tokens = realloc(tokens, bufsize * sizeof(char *));
			if (!tokens)
			{
				free(tokens_backup);
				printf("Error de sintaxis\n");
				exit(EXIT_FAILURE);
			}
		}
		token = strtok(NULL, TOK_DELIM);
	}
	tokens[posicion] = NULL;
	return tokens;
}

void ejecutar_archivo(const char *filename)
{
	char **args;
	int estado;
	FILE *fp = fopen(filename, "r");
	if (fp == NULL)
	{
		fprintf(stderr, "No se pudo abrir el archivo: %s\n", filename);
		exit(EXIT_FAILURE);
	}

	char *line = NULL; 
	size_t line_size = 0;
	ssize_t read;

	int i = 1;
	while ((read = getline(&line, &line_size, fp)) != -1)
	{
		printf("======================================== Linea %i ========================================\n", i);
		// Validar que la linea no esta vacia
		if (line != NULL && strlen(line) > 1)
		{
			args = dividir_linea(line);
			// Validacion extra en caso de un error en "dividir_linea"
			if (args != NULL)
			{
				estado = comando(args);
				if (estado == 0)
					printf("Comando de la linea %i no se ejecuto\n", i);

				free(args);	
			}
		} else {
			printf("\033[1;31mLinea en blanco\e[0m\n");
		}
		i++;
	}
	printf("\033[1;32;1mSe termino de leer el archivo\e[0m\n");
	// Liberar memoria
	free(line);
	line = NULL;
	line_size = 0;

	fclose(fp);
}

int main(int argc, char **argv)
{
	// Variable de entorno shell
	char *shell_path = "/home/yorth21/myshell/myshell";
    setenv("shell", shell_path, 1);

	char *linea;
	char **args;
	int estado;
	do
	{
		char cwd[1024];
		getcwd(cwd, sizeof(cwd));
		char *user = getlogin();
		// Verificar si el programa viene con un argumento
		if (argv[1] != NULL)
		{
			ejecutar_archivo(argv[1]);
			estado = 0;
			return EXIT_SUCCESS;
		}

		// Prompt personalizado a mi gusto Yorth21
		printf("\e[0;36m┌─(\033[1;34m%s@yorth\e[0;36m)-[\e[0m%s\e[0;36m]\n└─\033[1;34;1m$\e[0m ", user, cwd);

		// Si viene sin argumentos
		if (argc == 1)
		{
			linea = leer_linea();
			args = dividir_linea(linea);
			estado = comando(args);
			free(linea);
			free(args);
		}
	} while (estado);
	return EXIT_SUCCESS;
}
