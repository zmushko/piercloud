#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <regex.h>
#include <dirent.h>
#include <stddef.h>
#include <unistd.h>
#include <stdarg.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>

void gcCollect(void*** gc, void* ptr)
{
	if (gc == NULL || ptr == NULL)
	{
	        errno = EINVAL;
		return;
	}

	size_t len_ptr	= sizeof(void*);
        size_t i	= 0;

	if (*gc)
	{
		while ((*gc)[i])
		{
			i++;
		}
	}

	void** t = (void**)realloc(*gc, len_ptr*(i + 2));
	if (!t)
	{
		free((*gc)[i]);
		(*gc)[i]	= NULL;
		errno		= ENOMEM;
		return;
	}

	*gc		= t;
	(*gc)[i]	= ptr;
	(*gc)[i + 1]	= NULL;

	return;
}

void gcClean(void** gc)
{
	if (gc == NULL)
	{
		return;
	}

        size_t i = 0;
        while (gc[i])
	{
		free(*((void**)gc[i]));
		i++;
	}
	free(gc);
}

ssize_t lstPush(char*** lst, const char* str)
{
	if(!lst || !str)
	{
	        errno = EINVAL;
		return -1;
	}

	size_t len_ptr	= sizeof(void*);
        size_t i	= 0;

	if(*lst == NULL)
	{
		*lst = (char**)malloc(len_ptr);
		if(*lst == NULL)
		{
			errno = ENOMEM;
			return -1;
		}
		(*lst)[0] = NULL;
	}
	else
	{
		while((*lst)[i])
		{
			i++;
		}
	}

	(*lst)[i] = (char*)malloc(strlen(str) + 1);
	if((*lst)[i] == NULL)
	{
		errno = ENOMEM;
		return -1;
	}
	strcpy((*lst)[i], str);

	char** t = (char**)realloc(*lst, len_ptr*(i + 2));
	if(!t)
	{
		free((*lst)[i]);
		(*lst)[i]	= NULL;
		errno		= ENOMEM;
		return -1;
	}

	*lst		= t;
	(*lst)[i + 1]	= NULL;

	return i;
}

size_t lstSize(char** lst)
{
	if(!lst)
	{
		return 0;
	}

	size_t i = 0;
        while(lst[i])
	{
		i++;
	}

	return i;
}

size_t lstLength(char** lst)
{
	if(!lst)
	{
		return 0;
	}

	size_t i = 0;
	size_t length = 0;
        while(lst[i])
	{
		length += strlen(lst[i]) + 1;
		i++;
	}

	return length + 1;
}

char* lstGlue(char** lst)
{
	if (lst == NULL)
	{
		return NULL;
	}

	char* rval = NULL;
        size_t i = 0;
        while (lst[i])
	{
		size_t rlen = rval ? strlen(rval) : 0;
		char* t = realloc(rval, rlen + strlen(lst[i]) + 1);
		if (t == NULL)
		{
			free(rval);
			errno = ENOMEM;
			return NULL;
		}
		rval = t;
		strcpy(rval + rlen, lst[i]);
		i++;
	}
	return rval;
}

void lstFree(char** lst)
{
	if(!lst)
	{
		return;
	}

        size_t i = 0;
        while(lst[i])
	{
		free(lst[i]);
		i++;
	}
	free(lst);
}

void delGaps(char* str)
{
	size_t i = 0;
	size_t j = 0;
	for (; str[i]; ++i)
	{
		if (!isspace(str[i]))
		{
			str[j++] = str[i];
		}
	}
	str[j] = '\0';
}

ssize_t mapPush(char*** map, const char* key, const char* value)
{
	if (-1 == lstPush(map, key))
	{
		return -1;
	}

	return lstPush(map, value);
}

char* getMapValue(char** map, const char* key)
{
	if (map == NULL || key == NULL)
	{
		return NULL;
	}

	if (0 != (lstSize(map) % 2))
	{
		return NULL;
	}

	size_t i = 0;
	while (map[i])
	{
		if (!strcmp(map[i], key))
		{
			return map[i + 1];
		}
		i += 2;
	}

	return NULL;
}

char** getMapValues(char** map, const char* key)
{
	if (map == NULL || key == NULL)
	{
		return NULL;
	}

	if (0 != (lstSize(map) % 2))
	{
		return NULL;
	}

	char** rval = NULL;
	size_t i = 0;
	for (i = 0; map[i]; i += 2)
	{
		if (strcmp(map[i], key))
		{
			continue;
		}
		if (-1 == lstPush(&rval, map[i + 1]))
		{
			lstFree(rval);
			rval = NULL;
			break;
		}
	}

	return rval;
}

void mapFree(char** map)
{
	lstFree(map);
}

char** readConfig(const char* path)
{
	int safe_errno = errno;
	FILE* fp = fopen(path, "r");
	if (fp == NULL)
	{
		errno = safe_errno;
		return NULL;
	}

	char** rval	= NULL;
	char* line	= NULL;
	size_t len	= 0;
	ssize_t read	= 0;
	while (0 < (read = getline(&line, &len, fp)))
	{
		delGaps(line);
		char* value = NULL;
		char* key = NULL;
		if (2 == sscanf(line, "%m[^=]=%ms", &key, &value))
		{
			if(key == NULL || key[0] == '#')
			{
				free(value);
				free(key);
				continue;
			}
			if (-1 == mapPush(&rval, key, value))
			{
				free(value);
				free(key);
				mapFree(rval);
				rval = NULL;
				break;
			}
		}
		free(value);
		free(key);
	}
	free(line);
	fclose(fp);

	return rval;
}

char* getConfigValue(char** map, const char* key, const char* dval)
{
	if (map == NULL || key == NULL)
	{
		return (char*)dval;
	}

	char* rval = getMapValue(map, key);
	if (rval == NULL)
	{
		return (char*)dval;
	}

	return rval;
}

char** lstSplitStr(const char splitter, const char* s)
{
	if(!s)
	{
	        errno = EINVAL;
		return NULL;
	}

	char* t = (char*)malloc(strlen(s) + 1);
	if(!t)
	{
		errno = ENOMEM;
		return NULL;
	}
	strcpy(t, s);

	char* p		= t;
	char* b		= NULL;
	char** lst	= NULL;

	while(*p)
	{
		if(*p == splitter)
		{
			*p = '\0';
			if(!b)
			{
				p++;
				continue;
			}

			if(-1 == lstPush(&lst, b))
			{
				lstFree(lst);
			        free(t);
				return NULL;
			}
			b = NULL;
			p++;
			continue;
                }

		if(!b)
		{
			b = p;
		}
		p++;
	}

	if(b)
	{
		if(-1 == lstPush(&lst, b))
		{
			lstFree(lst);
			free(t);
			return NULL;
                }
	}
        free(t);

	return lst;
}

char** lstEgrep(char* const* lst, const char* regexp)
{
	if(!lst || !regexp)
	{
		errno = EINVAL;
		return NULL;
	}

	char** new_lst	= NULL;
	size_t i	= 0;

	while(lst[i])
	{
		regex_t reg;
		regex_t* preg = &reg;
		memset(preg, 0, sizeof(regex_t));

		if(0 != regcomp(preg, regexp, REG_EXTENDED))
		{
			lstFree(new_lst);
			return NULL;
		}

		if(REG_NOMATCH == regexec(preg, lst[i], 0, NULL, 0))
		{
			regfree(preg);
			i++;
			continue;
		}

		if(-1 == lstPush(&new_lst, lst[i]))
		{
			lstFree(new_lst);
			regfree(preg);
			return NULL;
		}
		regfree(preg);
		i++;
	}

	return new_lst;
}

char** lstEgrepFormat(char* const* lst, const char* format, ...)
{
	if(!lst || !format)
	{
		errno = EINVAL;
		return NULL;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* regexp = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!regexp)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return NULL;
	}
	vsprintf(regexp, format, ap);
        va_end(ap);
        va_end(apn);

	char** rval = lstEgrep(lst, regexp);
	free(regexp);

	return rval;
}

char** lstReadDir(const char* path)
{
	if(!path)
	{
		errno = EINVAL;
		return NULL;
	}

	DIR* dp	= opendir(path);
	if(dp == NULL)
	{
		return NULL;
	}

	size_t dirent_sz	= offsetof(struct dirent, d_name) + pathconf(path, _PC_NAME_MAX);
	struct dirent* entry	= (struct dirent*)malloc(dirent_sz + 1);
	if(entry == NULL)
	{
		closedir(dp);
		return NULL;
	}

	char** lst = NULL;
	for(;;)
	{
		struct dirent* result = NULL;
		if(0 != readdir_r(dp, entry, &result)
			|| result == NULL)
		{
			break;
		}

                if(!strcmp(entry->d_name, ".")
			|| !strcmp(entry->d_name, ".."))
		{
                        continue;
		}

		if(-1 == lstPush(&lst, result->d_name))
		{
			lstFree(lst);
			free(entry);
			closedir(dp);
			return NULL;
		}
	}

	free(entry);
	closedir(dp);

	return lst;
}

char* ptReplaceStr(const char* string, const char* search, const char* replace)
{

	if(!string
		|| !search
		|| !replace)
	{
		errno = EINVAL;
		return NULL;
	}

        size_t len_string   = strlen(string);
        size_t len_search   = strlen(search);
        size_t len_replace  = strlen(replace);

	if(!len_search)
	{
		char* rval = (char*)malloc(len_string + len_replace + 1);
		if(!rval)
		{
			errno = ENOMEM;
			return NULL;
		}
                *rval = '\0';
                strcat(rval, string);
                strcat(rval, replace);
		return rval;
	}

	char* copy = (char*)malloc(strlen(string) + 1);
	if(!copy)
	{
		errno = ENOMEM;
		return NULL;
	}
	strcpy(copy, string);

	char* rval = (char*)malloc(1);
	if(!rval)
	{
		free(copy);
		errno = ENOMEM;
		return NULL;
	}
	*rval = '\0';

	char* p = copy;
	char* s = NULL;
	char* t = NULL;

	while((s = strstr(p, search)))
	{
		*s = '\0';
		t = (char*)realloc(rval, strlen(rval) + strlen(p) + len_replace + 1);
		if(t == NULL)
		{
			free(copy);
			free(rval);
			errno = ENOMEM;
			return NULL;
		}
		rval = t;
		strcat(rval, p);
		strcat(rval, replace);
		p = s + len_search;
	}

	t = (char*)realloc(rval, strlen(rval) + strlen(p) + 1);
	if(t == NULL)
	{
		free(copy);
		free(rval);
		return NULL;
	}

	rval = t;
	strcat(rval, p);
	free(copy);

	return rval;
}

int replaceStrFormat(char** string, const char* search, const char* format, ...)
{
	if(!string
		|| !search
		|| !format)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* replace = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!replace)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(replace, format, ap);
        va_end(ap);
        va_end(apn);

	char* t = ptReplaceStr(*string, search, replace);
        if(!t)
	{
		free(replace);
		return -1;
	}
	free(*string);
	*string = t;
	free(replace);

	return 0;
}

char* String(const char* format, ...)
{
	if(!format)
	{
		errno = EINVAL;
		return NULL;
	}

	va_list ap, apt;
        va_start(ap, format);
	va_copy(apt, ap);

	char* rval = (char*)malloc(vsnprintf(NULL, 0, format, apt) + 1);
        if(rval == NULL)
	{
	        va_end(ap);
	        va_end(apt);
		errno = ENOMEM;
		return NULL;
	}
	vsprintf(rval, format, ap);

	va_end(ap);
        va_end(apt);

	return rval;
}

int isDirFormat(const char* format, ...)
{
	if(format == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(path == NULL)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(path, format, ap);
        va_end(ap);
        va_end(apn);

	struct stat sb;
	memset(&sb, 0, sizeof(struct stat));

	errno = 0;
	if(-1 == stat(path, &sb))
	{
		if(errno == ENOENT)
		{
			free(path);
			return 0;
		}
		free(path);
		return -1;
	}
	if(S_ISDIR(sb.st_mode))
	{
		free(path);
		return 1;
	}
	free(path);
	return 0;
}

int unlinkFormat(const char* format, ...)
{
	if(format == NULL)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(path == NULL)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(path, format, ap);
        va_end(ap);
        va_end(apn);

	int rval = unlink(path);

	free(path);

	return rval;
}

int chopStr(char* s)
{
	if(!s)
	{
		errno = EINVAL;
		return -1;
	}

        char* t = ptReplaceStr(s, "\n", "");
        if(!t)
	{
		return -1;
	}

	char* t2 = ptReplaceStr(t, "\r", "");
	if(!t2)
	{
	        free(t);
		return -1;
	}

	strcpy(s, t2);
	free(t2);
	free(t);

	return 0;
}

int trimStr(char* s)
{
	if(!s)
	{
		errno = EINVAL;
		return -1;
	}

	char* copy = (char*)malloc(strlen(s) + 1);
	if(!copy)
	{
		errno = ENOMEM;
		return -1;
	}
	strcpy(copy, s);

	char* p = NULL;
	char* e = NULL;
	for(p = copy; *p && isspace(*p); *p = 0, p++);
	for(e = p + strlen(p) - 1; *e && isspace(*e); *e = 0, e--);
	strcpy(s, p);
	free(copy);

	return 0;
}

char* ptCutStr(const char* string, const char* begin, const char* end)
{
	if(!string
		|| !begin
		|| !end)
	{
		errno = EINVAL;
		return NULL;
	}

	char* s = (char*)malloc(strlen(string) + 1);
	if(!s)
	{
		errno = ENOMEM;
		return NULL;
	}
	strcpy(s, string);

	char* p	= strstr(s, begin);
	if(!p)
	{
		char* rval = (char*)malloc(1);
		if(!rval)
		{
			free(s);
			errno = ENOMEM;
			return NULL;
		}
		*rval = '\0';
		free(s);
		return rval;
	}

	p += strlen(begin);
	if(strlen(end))
	{
		char* e = strstr(p, end);
		if(e)
		{
			*e = '\0';
		}
	}

	char* rval = (char*)malloc(strlen(p) + 1);
	if(!rval)
	{
		free(s);
		errno = ENOMEM;
		return NULL;
	}
	strcpy(rval, p);
	free(s);

	return rval;
}

char* ptGetFileContent(const int fn)
{
	char* rval	= NULL;
	ssize_t r	= 0;
	size_t total	= 0;
	char buf[2048]	= {'\0'};

	int safe_errno  = errno;
	while((r = read(fn, buf, sizeof(buf))))
	{
		if(-1 == r)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK)
			{
				errno = safe_errno;
				continue;
			}
			if(rval)
			{
				free(rval);
			}
			return NULL;
		}
		char* t = (char*)realloc(rval, total + r + 1);
		if(!t)
		{
			if(rval)
			{
				free(rval);
			}
			return NULL;
		}
		rval = t;
		memcpy(rval + total, buf, r);
		total += r;
	}
	if(rval)
	{
		*(rval + total) = '\0';
	}

	return rval;
}

char* ptGetFileContentFormat(const char* format, ...)
{
	char* content = NULL;
	if(!format)
	{
	        errno = EINVAL;
		return NULL;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!path)
	{
		va_end(ap);
		va_end(apn);
		errno = ENOMEM;
		return NULL;
	}
	vsprintf(path, format, ap);
        va_end(ap);
        va_end(apn);

	int fn = open(path, O_RDONLY);
	if(fn == -1)
	{
		free(path);
		return NULL;
	}

	content = ptGetFileContent(fn);
	if(content == NULL)
	{
		free(path);
		close(fn);
		return NULL;
	}

	if(-1 == close(fn))
	{
		free(path);
		free(content);
		return NULL;
	}

        free(path);

	return content;
}

int getFileContentFormat(char** content, const char* format, ...)
{
	if(!content || !format)
	{
	        errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!path)
	{
		va_end(ap);
		va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(path, format, ap);
        va_end(ap);
        va_end(apn);

	int fn = open(path, O_RDONLY);
	if(fn == -1)
	{
		free(path);
		return -1;
	}

	*content = ptGetFileContent(fn);
	if(*content == NULL)
	{
		free(path);
		close(fn);
		return -1;
	}

	if(-1 == close(fn))
	{
		free(*content);
		free(path);
		return -1;
	}

        free(path);

	return 0;
}

int writeToFileFormat(const int fd, const char* format, ...)
{
	if(!format)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* buf = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!buf)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(buf, format, ap);
        va_end(apn);
        va_end(ap);

	size_t len	= strlen(buf);
	ssize_t n	= 0;
	size_t t	= 0;
	int safe_errno	= errno;
	errno		= 0;
	while((n = write(fd, buf + t, len)))
	{
		if(-1 == n)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK
				|| errno == EPIPE)
			{
				errno = safe_errno;
				continue;
			}
			free(buf);
			return -1;
		}
		t   += n;
		len -= n;
	}
	
	free(buf);
	errno = safe_errno;

	return n;
}

int getShellContentFormat(char** content, const char* format, ...)
{
	if(!content || !format)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!path)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(path, format, ap);
        va_end(apn);
        va_end(ap);

	FILE* p = popen(path, "r");
	if(!p)
	{
		free(path);
		return -1;
	}

	int fn = fileno(p);
	if(-1 == fn)
	{
		free(path);
		pclose(p);
		return -1;
	}

	*content = ptGetFileContent(fn);
	if(*content == NULL)
	{
		free(path);
		pclose(p);
		return -1;
	}
	free(path);
	pclose(p);

	return 0;
}

int executeShellFormat(const char* format, ...)
{
	if(!format)
	{
		errno = EINVAL;
		return -1;
	}

	va_list ap, apn;
        va_start(ap, format);
	va_copy(apn, ap);

	char* shell = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!shell)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return -1;
	}
	vsprintf(shell, format, ap);
        va_end(apn);
        va_end(ap);

	int rval = system(shell);
	free(shell);

	return rval;
}

static ssize_t read_chunk_str(char** data, const int fd)
{
	size_t size = 512;
	if(!data)
	{
		errno = EINVAL;
		return -1;
	}

	char* buf = (char*)malloc(size + 1);
	if(!buf)
	{
		errno = ENOMEM;
		return -1;
	}
	memset(buf, 0, size + 1);

	ssize_t r	= 0;
	*data		= NULL;

	int safe_errno  = errno;
	while((r = read(fd, buf, size)))
	{
		if(-1 == r)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK)
			{
				errno = safe_errno;
				continue;
			}

			free(buf);
			return -1;
		}

		*data = (char*)malloc(r + 1);
		if(*data == NULL)
		{
			free(buf);
			errno = ENOMEM;
			return -1;
		}
		memcpy(*data, buf, r);
		(*data)[r] = '\0';
		break;
	}
	free(buf);
	return r;
}

static int cut_rest(char** data, char** rest)
{
	char* p = strchr(*rest, '\n');
	if(p)
	{
		*p	= '\0';
		*data	= (char*)malloc(strlen(*rest) + 2);
		if(*data == NULL)
		{
			return -1;
		}
		strcpy(*data, *rest);
		*(*data + strlen(*rest)) = '\n';
		*(*data + strlen(*rest) + 1) = '\0';
		*p = '\n';

		char* t = ptReplaceStr(*rest, *data, "");
		if(!t)
		{
			return -1;
		}
		free(*rest);
		*rest = t;
	}
	else
	{
		*data = NULL;
	}
	return 0;
}

ssize_t readFileRow(char** data, const int fd)
{
	if (!data)
	{
		errno = EINVAL;
		return -1;
	}

	static char* rest = NULL;
	if (!rest)
	{
		rest = (char*)malloc(1);
		if (!rest)
		{
			errno = ENOMEM;
			return -1;
		}
		*rest = '\0';
	}

	*data = NULL;
	if (-1 == cut_rest(data, &rest))
	{
		free(rest);
		return -1;
	}

	if (*data)
	{
		chopStr(*data);
		return strlen(*data) + 1;
	}

	char* buf = NULL;
	ssize_t n = 0;

	while (0 < (n = read_chunk_str(&buf, fd)))
	{
		if (!buf)
		{
			free(rest);
			return -1;
		}

		char* t = ptReplaceStr(rest, "", buf);
		if (!t)
		{
			free(rest);
			free(buf);
			return -1;
		}
		free(rest);
		rest = t;

		if (-1 == cut_rest(data, &rest))
		{
			free(rest);
			free(buf);
			return -1;
		}
		if (*data)
		{
			free(buf);
			chopStr(*data);
			return strlen(*data) + 1;
		}
	}

	if(!n && strlen(rest))
	{
		*data	= (char*)malloc(strlen(rest) + 1);
		if(*data == NULL)
		{
			free(rest);
			return -1;
		}
		strcpy(*data, rest);
		chopStr(*data);
		return strlen(*data) + 1;
	}
	free(rest);
	rest = NULL;
	if (buf)
	{
		free(buf);
	}
	return n;
}

char* ptGetFileRowFormat(const char* format, ...)
{
	if(!format)
	{
		errno = EINVAL;
		return NULL;
	}

	va_list ap, apn;
	va_start(ap, format);
	va_copy(apn, ap);

	char* path = (char*)malloc(vsnprintf(NULL, 0, format, apn) + 1);
        if(!path)
	{
	        va_end(ap);
	        va_end(apn);
		errno = ENOMEM;
		return NULL;
	}
	vsprintf(path, format, ap);
        va_end(ap);
        va_end(apn);

	int fd = open(path, O_RDONLY);
	if(-1 == fd)
	{
		free(path);
		return NULL;
	}

	char* data = NULL;
	if(-1 == readFileRow(&data, fd))
	{
		free(path);
		if (data)
		{
			free(data);
		}
		close(fd);
		return NULL;
	}

	if(-1 == close(fd))
	{
		free(data);
		free(path);
		return NULL;
	}

	free(path);
	return data;
}

size_t Read(int fd, char** rval)
{
	ssize_t r	= 0;
	size_t total	= 0;
	char buf[2048]	= {'\0', };

	int safe_errno = errno;
	while((r = read(fd, buf, sizeof(buf))))
	{
		if(-1 == r)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK)
			{
				errno = safe_errno;
				continue;
			}
			if(*rval)
			{
				free(*rval);
			}
			return 0;
		}
		char* t = (void*)realloc(*rval, total + r + 1);
		if(!t)
		{
			if(*rval)
			{
				free(*rval);
			}
			return 0;
		}
		*rval = t;
		memcpy(*rval + total, buf, r);
		total += r;
	}

	return total;
}

size_t Write(int fd, const char* data, size_t len)
{
	ssize_t n	= 0;
	size_t t	= 0;
	int safe_errno	= errno;
	errno		= 0;
	while((n = write(fd, data + t, len)))
	{
		if(-1 == n)
		{
			if(errno == EINTR
				|| errno == EAGAIN
				|| errno == EWOULDBLOCK
				|| errno == EPIPE)
			{
				errno = safe_errno;
				continue;
			}
			return 0;
		}
		t   += n;
		len -= n;
	}
	errno = safe_errno;

	return t;
}
