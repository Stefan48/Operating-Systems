#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include "hashmap.h"

#define MAX_KEY 50
#define MAX_VALUE 500
#define MAX_NR_DIRS 20
#define MAX_PATH 100
#define MAX_LINE 260
#define MAX_IFS 10


int delimiter(char x)
{
	return (strchr("\t ()[]{}<>=+-*/%!&|^.,:;\n", x) != NULL);
}

// returns NULL on error, processed string otherwise
char *tokenizeAndReplace(Hashmap *hashmap, char *str)
{
	char *result;
	char word[MAX_LINE];
	int pos = 0, posw = 0;
	int i;
	char *val, *new_val;
	result = calloc(MAX_LINE, sizeof(char));
	if(result == NULL)
	{
		return NULL;
	}
	word[0] = '\0';
	for(i = 0; (i == 0 || str[i-1] != '\0'); ++i)
	{
		if(delimiter(str[i]) || str[i] == '\0')
		{
			if(word[0] != '\0')
			{
				word[posw] = '\0';
				posw = 0;
				val = getValue(hashmap, word);
				if(val != NULL)
				{
					new_val = tokenizeAndReplace(hashmap, val);
					if(new_val == NULL)
					{
						free(result);
						return NULL;
					}
					strcat(result, new_val);
					pos += strlen(new_val);
					free(new_val);
				}
				else
				{
					strcat(result, word);
					pos += strlen(word);
				}
			}
			result[pos++] = str[i];
		}
		else
		{
			word[posw++] = str[i];
		}
	}
	return result;
}

//returns 0 on success, error code otherwise
int printLine(Hashmap *hashmap, char *buffer, int len, char *word, char *line, FILE *stream)
{
	int posw = 0, posl = 0;
	int i;
	char *val;
	int empty_line;
	word[0] = '\0';
	memset(line, 0, MAX_LINE);
	for(i = 0; i < len; ++i)
	{
		if(delimiter(buffer[i]))
		{
			if(word[0] != '\0')
			{
				word[posw] = '\0';
				val = tokenizeAndReplace(hashmap, word);
				if(val == NULL)
				{
					return 12;
				}
				strcat(line, val);
				posl += strlen(val);
				free(val);
				word[0] = '\0';
				posw = 0;
			}
			if(buffer[i] == '\t')
			{
				line[posl++] = ' ';
			}
			else
			{
				line[posl++] = buffer[i];
			}
		}
		else
		{
			word[posw++] = buffer[i];
		}
	}
	empty_line = 1;
	for(i = 0; i < posl; ++i)
	{
		if(line[i] != ' ' && line[i] != '\t' && line[i] != '\n')
		{
			empty_line = 0;
			break;
		}
	}
	if(!empty_line)
	{
		fprintf(stream, "%s", line);
	}
	return 0;
}

// preprocesses a file passed through @fin stream
// output is redirected through @fout stream
// @folder is the path of the directory of the input file
// returns 0 on sucess, error code otherwise
int preprocess(Hashmap *hashmap, FILE *fin, FILE *fout, char *folder, char *key, char *value,
			    int nr_dirs, char *dirs[], char *buffer, char *word, char *line)
{
	int i;
	int len, pos, posk, posv;
	int bytes_read;
	char c;
	int posl;
	int inside_string, inside_define, inside_define_string, inside_undef;
	int last_ch_backslash;
	int define_len, undef_len;
	int read_key;
	char *val;
	int inside_if;
	int met_cond[MAX_IFS], do_print[MAX_IFS];
	int if_len, elif_len, else_len, endif_len, ifdef_len, ifndef_len;
	int processed;
	int include_len;
	FILE *file;
	int file_exists;
	int Error = 0;
	
	pos = 0;
	inside_string = inside_define = inside_undef = inside_if = 0;
	define_len = strlen("#define ");
	undef_len = strlen("#undef ");
	if_len = strlen("#if ");
	elif_len = strlen("#elif ");
	else_len = strlen("#else");
	endif_len = strlen("#endif");
	ifdef_len = strlen("#ifdef ");
	ifndef_len = strlen("#ifndef ");
	include_len = strlen("#include");
	
	while(1)
	{
		bytes_read = fread(&c, sizeof(char), 1, fin);
		if(bytes_read == 0)
		{
			break;
		}
		if(inside_define)
		{
			if(read_key) // if reading define key
			{
				if(c == ' ' || c == '\t')
				{
					read_key = 0;
					key[posk] = '\0';
				}
				else if(c == '\\')
				{
					read_key = 0;
					key[posk] = '\0';
					last_ch_backslash = 1;
				}
				else if(c == '\n')
				{
					inside_define = 0;
					read_key = 0;
					pos = 0;
					key[posk] = '\0';
					Error = insertKeyValue(hashmap, key, "");
					if(Error)
					{
						return Error;
					}
				}
				else
				{
					key[posk++] = c;
				}
			}
			else // if reading define value
			{
				if(inside_define_string)
				{
					if(c == '"')
					{
						inside_define_string = 0;
					}
					value[posv++] = c;
				}
				else
				{
					if(c == ' ' || c == '\t')
					{
						last_ch_backslash = 0;
						if(value[posv - 1] != ' ')
						{
							value[posv++] = ' ';
						}
					}
					else if(c == '"')
					{
						last_ch_backslash = 0;
						inside_define_string = 1;
						value[posv++] = c;
					}
					else if(c == '\\')
					{
						last_ch_backslash = 1;
						value[posv++] = c;
					}
					else if(c == '\n')
					{
						if(last_ch_backslash)
						{
							last_ch_backslash = 0;
							posv--;
						}
						else
						{
							inside_define = 0;
							value[posv] = '\0';
							Error = insertKeyValue(hashmap, key, value);
							if(Error)
							{
								return Error;
							}
							pos = 0;
						}
					}
					else
					{
						value[posv++] = c;
					}
				}
			}
		}
		else if(inside_undef)
		{
			if(c == ' ' || c == '\t')
			{
				key[posk] = '\0';
			}
			else if(c == '\n')
			{
				inside_undef = 0;
				pos = 0;
				key[posk] = '\0';
				Error = removeKey(hashmap, key);
				if(Error)
				{
					return Error;
				}
			}
			else
			{
				key[posk++] = c;
			}
		}
		else if(inside_if)
		{
			if(c == '\n')
			{
				buffer[pos++] = c;
				buffer[pos] = '\0';
				
				pos = posl = 0;
				while(buffer[pos] == ' ' || buffer[pos] == '\t')
				{
					pos++;
					if(buffer[pos] == '\0')
					{
						break;
					}
				}
				line[posl] = '\0';
				while(buffer[pos] != '\0')
				{
					line[posl++] = buffer[pos++];
				}
				posl--;
				while(posl >= 0 && (line[posl] == ' ' || line[posl] == '\t' || line[posl] == '\n'))
				{
					line[posl--] = '\0';
				}
				processed = 0;
				// check for #endif
				if(strncmp(line, "#endif ", endif_len) == 0)
				{
					inside_if--;
					pos = 0;
					processed = 1;
				}
				// check for #elif
				else if(strncmp(line, "#elif ", elif_len) == 0)
				{
					if(met_cond[inside_if])
					{
						do_print[inside_if] = 0;
					}
					else
					{
						posl = elif_len;
						while(line[posl] == ' ' || line[posl] == '\t')
						{
							posl++;
						}
						val = tokenizeAndReplace(hashmap, &line[posl]);
						if(val == NULL)
						{
							return 12;
						}
						if(atoi(val) != 0)
						{
							met_cond[inside_if] = 1;
							do_print[inside_if] = 1;
						}
						free(val);
					}
					pos = 0;
					processed = 1;
				}
				// check for #else
				else if(strncmp(line, "#else", else_len) == 0)
				{
					if(met_cond[inside_if])
					{
						do_print[inside_if] = 0;
					}
					else
					{
						met_cond[inside_if] = 1;
						do_print[inside_if] = 1;
					}
					pos = 0;
					processed = 1;
				}
				if(!processed)
				{
					// check for #if
					if(strncmp(line, "#if ", if_len) == 0)
					{
						inside_if++;
						if(do_print[inside_if-1])
						{
							posl = if_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							val = tokenizeAndReplace(hashmap, &line[posl]);
							if(val == NULL)
							{
								return 12;
							}
							if(atoi(val) != 0)
							{
								met_cond[inside_if] = 1;
								do_print[inside_if] = 1;
							}
							else
							{
								met_cond[inside_if] = 0;
								do_print[inside_if] = 0;
							}
							free(val);
						}
						else // if not do_print[inside_if-1]
						{
							met_cond[inside_if] = 1;
							do_print[inside_if] = 0;
						}
					}
					// check for #ifdef
					else if(strncmp(line, "#ifdef ", ifdef_len) == 0)
					{
						inside_if++;
						if(do_print[inside_if-1])
						{
							posl = ifdef_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							val = getValue(hashmap, &line[posl]);
							if(val != NULL)
							{
								met_cond[inside_if] = 1;
								do_print[inside_if] = 1;
							}
							else
							{
								met_cond[inside_if] = 0;
								do_print[inside_if] = 0;
							}
						}
						else // if not do_print[inside_if-1]
						{
							met_cond[inside_if] = 1;
							do_print[inside_if] = 0;
						}
					}
					// check for #ifndef
					else if(strncmp(line, "#ifndef ", ifndef_len) == 0)
					{
						inside_if++;
						if(do_print[inside_if-1])
						{
							posl = ifndef_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							val = getValue(hashmap, &line[posl]);
							if(val == NULL)
							{
								met_cond[inside_if] = 1;
								do_print[inside_if] = 1;
							}
							else
							{
								met_cond[inside_if] = 0;
								do_print[inside_if] = 0;
							}
						}
						else // if not do_print[inside_if-1]
						{
							met_cond[inside_if] = 1;
							do_print[inside_if] = 0;
						}
					}
					else if(do_print[inside_if])
					{
						// check for #define
						if(strncmp(line, "#define ", define_len) == 0)
						{
							posl = define_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							posk = 0;
							while(!delimiter(line[posl]))
							{
								key[posk++] = line[posl++];
							}
							key[posk] = '\0';
							posv = 0;
							value[0] = '\0';
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							while(line[posl] != '\0')
							{
								if(posv == 0 || (line[posl] != ' ' && line[posl] != '\t') || (value[posv-1] != ' '))
								{
									if(line[posl] == '\t')
									{
										value[posv++] = ' ';
									}
									else value[posv++] = line[posl];
								}
								posl++;
							}
							if(value[0] != '\0')
							{
								value[posv] = '\0';
							}
							Error = insertKeyValue(hashmap, key, value);
							if(Error)
							{
								return Error;
							}
						}
						// check for #undef
						else if(strncmp(line, "#undef ", undef_len) == 0)
						{
							posl = undef_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							posk = 0;
							while(!delimiter(line[posl]))
							{
								key[posk++] = line[posl++];
							}
							key[posk] = '\0';
							Error = removeKey(hashmap, key);
							if(Error)
							{
								return Error;
							}
						}
						// check for #include
						else if(strncmp(line, "#include", include_len) == 0)
						{
							posl = include_len;
							while(line[posl] == ' ' || line[posl] == '\t')
							{
								posl++;
							}
							posl++;
							line[posl + strlen(&line[posl]) - 1] = '\0';
							file_exists = 0;
							if(folder[0] != '\0')
							{
								strcpy(buffer, folder);
								strcat(buffer, "/");
								strcat(buffer, &line[posl]);
								file = fopen(buffer, "r");
							}
							else
							{
								file = fopen(&line[posl], "r");
							}
							if(file == NULL)
							{
								for(i = 0; i < nr_dirs; ++i)
								{
									strcpy(buffer, dirs[i]);
									strcat(buffer, "/");
									strcat(buffer, &line[posl]);
									file = fopen(buffer, "r");
									if(file != NULL)
									{
										file_exists = 1;
										break;
									}
								}
							}
							else
							{
								file_exists = 1;
							}
							if(!file_exists)
							{
								return 1;
							}
							Error = preprocess(hashmap, file, fout, folder, key, value, nr_dirs, dirs, buffer, word, line);
							if(Error)
							{
								fclose(file);
								return Error;
							}
							else
							{
								Error = fclose(file);
								if(Error)
								{
									return Error;
								}
							}
						}
						else // just normal line
						{
							Error = printLine(hashmap, buffer, strlen(buffer), word, line, fout);
							if(Error)
							{
								return Error;
							}
						}
					}
					else // if not do_print[inside_if]
					{
					}
					pos = 0;
				}
				processed = 0;
			}
			else
			{
				buffer[pos++] = c;
			}
		}
		else // if not (inside define || inside undef || inside if)
		{
			if(c == '\n')
			{
				buffer[pos++] = c;
				buffer[pos] = '\0';
				len = pos;
				pos = posl = 0;
				while(buffer[pos] == ' ' || buffer[pos] == '\t')
				{
					pos++;
					if(buffer[pos] == '\0')
					{
						break;
					}
				}
				line[posl] = '\0';
				while(buffer[pos] != '\0')
				{
					line[posl++] = buffer[pos++];
				}
				posl--;
				while(posl >= 0 && (line[posl] == ' ' || line[posl] == '\t' || line[posl] == '\n'))
				{
					line[posl--] = '\0';
				}
				// check for #if
				if(strncmp(line, "#if ", if_len) == 0)
				{
					posl = if_len;
					while(line[posl] == ' ' || line[posl] == '\t')
					{
						posl++;
					}
					inside_if = 1;
					val = tokenizeAndReplace(hashmap, &line[posl]);
					if(val == NULL)
					{
						return 12;
					}
					if(atoi(val) == 0)
					{
						met_cond[1] = 0;
						do_print[1] = 0;
					}
					else
					{
						met_cond[1] = 1;
						do_print[1] = 1;
					}
					free(val);
				}
				// check for #ifdef
				else if(strncmp(line, "#ifdef ", ifdef_len) == 0)
				{
					posl = ifdef_len;
					while(line[posl] == ' ' || line[posl] == '\t')
					{
						posl++;
					}
					inside_if = 1;
					val = getValue(hashmap, &line[posl]);
					if(val == NULL)
					{
						met_cond[1] = 0;
						do_print[1] = 0;
					}
					else
					{
						met_cond[1] = 1;
						do_print[1] = 1;
					}
				}
				// check for #ifndef
				else if(strncmp(line, "#ifndef ", ifndef_len) == 0)
				{
					posl = ifndef_len;
					while(line[posl] == ' ' || line[posl] == '\t')
					{
						posl++;
					}
					inside_if = 1;
					val = getValue(hashmap, &line[posl]);
					if(val == NULL)
					{
						met_cond[1] = 1;
						do_print[1] = 1;
					}
					else
					{
						met_cond[1] = 0;
						do_print[1] = 0;
					}
				}
				// check for #include
				else if(strncmp(line, "#include", include_len) == 0)
				{
					posl = include_len;
					while(line[posl] == ' ' || line[posl] == '\t')
					{
						posl++;
					}
					posl++;
					line[posl + strlen(&line[posl]) - 1] = '\0';
					file_exists = 0;
					if(folder[0] != '\0')
					{
						strcpy(buffer, folder);
						strcat(buffer, "/");
						strcat(buffer, &line[posl]);
						file = fopen(buffer, "r");
					}
					else
					{
						file = fopen(&line[posl], "r");
					}
					if(file == NULL)
					{
						for(i = 0; i < nr_dirs; ++i)
						{
							strcpy(buffer, dirs[i]);
							strcat(buffer, "/");
							strcat(buffer, &line[posl]);
							file = fopen(buffer, "r");
							if(file != NULL)
							{
								file_exists = 1;
								break;
							}
						}
					}
					else
					{
						file_exists = 1;
					}
					if(!file_exists)
					{
						return 1;
					}
					Error = preprocess(hashmap, file, fout, folder, key, value, nr_dirs, dirs, buffer, word, line);
					if(Error)
					{
						fclose(file);
						return Error;
					}
					else
					{
						Error = fclose(file);
						if(Error)
						{
							return Error;
						}
					}
				}
				// print current line
				else
				{
					Error = printLine(hashmap, buffer, len, word, line, fout);
					if(Error)
					{
						return Error;
					}
				}
				pos = 0;
			}
			else
			{
				buffer[pos++] = c;
				if(buffer[pos] == '"')
				{
					inside_string = 1 - inside_string;
				}
				else if(pos >= define_len && strncmp(&buffer[pos - define_len], "#define ", define_len) == 0 && !inside_string)
				{
					inside_define = 1;
					inside_define_string = 0;
					last_ch_backslash = 0;
					read_key = 1;
					posk = posv = 0;
					pos = 0;
				}
				else if(pos >= undef_len && strncmp(&buffer[pos - undef_len], "#undef ", undef_len) == 0 && !inside_string)
				{
					inside_undef = 1;
					posk = 0;
					pos = 0;
				}
			}
		}	
	}
	
	return 0;
}

int main(int argc, char *argv[])
{
	Hashmap hashmap;
	int i;
	char arg = '\0';
	int pos, posk;
	char key[MAX_KEY], value[MAX_VALUE];
	int nr_dirs = 0;
	char *dirs[MAX_NR_DIRS];
	char in_file[MAX_PATH];
	char out_file[MAX_PATH];
	char folder[MAX_PATH];
	int Error = 0;
	FILE *fin = NULL, *fout = NULL;
	char buffer[MAX_LINE];
	char word[MAX_LINE], line[MAX_LINE];
	
	memset(key, 0, MAX_KEY);
	memset(value, 0, MAX_VALUE);
	memset(buffer, 0, MAX_LINE);
	memset(word, 0, MAX_LINE);
	memset(line, 0, MAX_LINE);
	memset(folder, 0, MAX_PATH);
	
	// initialize hashmap
	Error = initializeHashmap(&hashmap);
	if(Error)
	{
		goto finish;
	}
	
	// process console arguments
	in_file[0] = out_file[0] = '\0';
	for(i = 1; i < argc; ++i)
	{
		if(arg == 'D')
		{
			arg = '\0';
			pos = posk = 0;
			while(argv[i][pos] != '=' && argv[i][pos] != '\0')
			{
				key[posk++] = argv[i][pos++];
			}
			key[posk] = '\0';
			if(argv[i][pos] == '=')
			{
				pos++;
				strcpy(value, &argv[i][pos]);
			}
			else
			{
				value[0] = '\0';
			}
			Error = insertKeyValue(&hashmap, key, value);
			if(Error)
			{
				goto finish;
			}
		}
		else if(arg == 'I')
		{
			arg = '\0';
			dirs[nr_dirs] = malloc((strlen(argv[i]) + 1) * sizeof(char));
			if(dirs[nr_dirs] == NULL)
			{
				Error = 12;
				goto finish;
			}
			strcpy(dirs[nr_dirs], argv[i]);
			nr_dirs++;
		}
		else if(arg == 'o')
		{
			arg = '\0';
			strcpy(out_file, argv[i]);
		}
		else if(argv[i][0] == '-')
		{
			if(argv[i][1] == 'D')
			{
				pos = 2;
				if(argv[i][pos] == '\0')
				{
					arg = 'D';
				}
				else
				{
					posk = 0;
					while(argv[i][pos] != '=' && argv[i][pos] != '\0')
					{
						key[posk++] = argv[i][pos++];
					}
					key[posk] = '\0';
					if(argv[i][pos] == '=')
					{
						pos++;
						strcpy(value, &argv[i][pos]);
					}
					else
					{
						value[0] = '\0';
					}
					Error = insertKeyValue(&hashmap, key, value);
					if(Error)
					{
						goto finish;
					}
				}
			}
			else if(argv[i][1] == 'I')
			{
				pos = 2;
				if(argv[i][pos] == '\0')
				{
					arg = 'I';
				}
				else
				{
					dirs[nr_dirs] = malloc((strlen(&argv[i][pos]) + 1) * sizeof(char));
					if(dirs[nr_dirs] == NULL)
					{
						Error = 12;
						goto finish;
					}
					strcpy(dirs[nr_dirs], &argv[i][pos]);
					nr_dirs++;
				}
			}
			else if(argv[i][1] == 'o')
			{
				pos = 2;
				if(argv[i][pos] == '\0')
				{
					arg = 'o';
				}
				else
				{
					strcpy(out_file, &argv[i][pos]);
				}
			}
			// else invalid flag
		}
		else
		{
			if(in_file[0] == '\0')
			{
				strcpy(in_file, argv[i]);
			}
			else if(out_file[0] == '\0')
			{
				strcpy(out_file, argv[i]);
			}
			else
			{
				// invalid argument
				Error = 1;
				goto finish;
			}
		}
	}
	
	// set input and output streams
	if(in_file[0] != '\0')
	{
		fin = fopen(in_file, "r");
		if(fin == NULL)
		{
			Error = 1;
			goto finish;
		}
		strcpy(folder, in_file);
		i = strlen(folder);
		while(i >= 0 && folder[i] != '/')
		{
			folder[i--] = '\0';
		}
		if(i >= 0)
		{
			folder[i--] = '\0';
		}
	}
	else
	{
		fin = stdin;
	}
	
	
	if(out_file[0] != '\0')
	{
		fout = fopen(out_file, "w");
		if(fout == NULL)
		{
			Error = 1;
			goto finish;
		}
	}
	else
	{
		fout = stdout;
	}
	
	// preprocess file
	Error = preprocess(&hashmap, fin, fout, folder, key, value, nr_dirs, dirs, buffer, word, line);
	if(Error)
	{
		goto finish;
	}
	
	
finish:
	// destroy hashmap
	destroyHashmap(&hashmap);
	
	// free dirs
	for(i = 0; i < nr_dirs; ++i)
	{
		if(dirs[i] != NULL)
		{
			free(dirs[i]);
		}
	}
	
	// close files
	if(fin != NULL && fin != stdin)
	{
		if(Error)
		{
			fclose(fin);
		}
		else Error = fclose(fin);
	}
	if(fout != NULL && fout != stdout)
	{
		
		if(Error)
		{
			fclose(fout);
		}
		else Error = fclose(fout);
	}
	
	if(Error)
	{
		exit(Error);
	}
	
	return 0;
}
