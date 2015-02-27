/*
 *  liblst.h
 *
 *  framework library
 *
 *  Author: Andrey Zmushko
 */

#ifndef __LIBLST_H
#define __LIBLST_H

#include <stdlib.h> // size_t definition

#ifdef __cplusplus

extern "C" {

#endif

ssize_t lstPush(char*** lst, const char* str);
size_t	lstSize(char** lst);
size_t	lstLength(char** lst);
char*	lstGlue(char** lst);
void	lstFree(char** lst);
void	delGaps(char* str);
ssize_t mapPush(char*** map, const char* key, const char* value);
char*	getMapValue(char** map, const char* key);
char**	getMapValues(char** map, const char* key);
void	mapFree(char** map);
char**	lstSplitStr(const char splitter, const char* s);
char**	lstEgrep(char* const* lst, const char* regexp);
char**	lstEgrepFormat(char* const* lst, const char* format, ...);
char**	lstReadDir(const char* path);

void	gcCollect(void*** gc, void* ptr);
void	gcClean(void** gc);

char*	ptReplaceStr(const char* string, const char* search, const char* replace);
int	replaceStrFormat(char** string, const char* search, const char* format, ...);
char*	String(const char* format, ...);
int	chopStr(char* s);
int	trimStr(char* s);
char*	ptCutStr(const char* string, const char* begin, const char* end);
size_t	Read(int fd, char** rval);
size_t	Write(int fd, const char* data, size_t len);

char*	ptGetFileContent(const int fn);
char*	ptGetFileContentFormat(const char* format, ...);
int	getFileContentFormat(char** content, const char* format, ...);
int	writeToFileFormat(const int fd, const char* format, ...);
int	getShellContentFormat(char** content, const char* format, ...);
int	executeShellFormat(const char* format, ...);
int 	isDirFormat(const char* format, ...);
int	unlinkFormat(const char* format, ...);

ssize_t	readFileRow(char** data, const int fd);
char*	ptGetFileRowFormat(const char* format, ...);

char**	readConfig(const char* path);
char*	getConfigValue(char** conf, const char* key, const char* dval);

#ifdef __cplusplus

}

#endif

#endif
