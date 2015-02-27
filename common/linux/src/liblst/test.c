#include <stdio.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "liblst.h"

main()
{

	printf("Testing readSocketStr()\n");
	int fd = open("fifo", O_RDONLY);
	if(-1 == fd)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}


	ssize_t n = 0;

	char* buf = NULL;
	while(0 < readFileRow(&buf, fd))
	{
		chopStr(buf);
		printf("buf=%s<--\n", chopStr(buf));
		free(buf);
	}

	if(-1 == n)
	{
		printf("=%s\n", strerror(errno));
		close(fd);
		return -1;
	}

	close(fd);

	return 0;



	printf("Testing lstSplitStr()\n");

	const char* s = "- - - ----- 1 - 2 -3 444- 555555-----55556";

	printf("%s\n", s);

	char** list = lstSplitStr('-', s);
	if(list == NULL)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}

	printf("lstSize=%d\n", lstSize(list));
	int i = 0;
	while(list[i])
	{
		printf("list[%d]=%s=\n", i, list[i]);
		i++;
	}

	lstFree(list);


	printf("Testing lstReadDir()\n");

	list = lstReadDir("/");
	if(list == NULL)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}

	printf("lstSize=%d\n", lstSize(list));
	i = 0;
	while(list[i])
	{
		printf("list[%d]=%s=\n", i, list[i]);
		i++;
	}

	printf("Testing lstEgrep()\n");

	char** list2 = lstEgrep(list, "ro");
	if(list2 == NULL)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}

	printf("lstSize=%d\n", lstSize(list2));
	i = 0;
	while(list2[i])
	{
		printf("list2[%d]=%s=\n", i, list2[i]);
		i++;
	}

	lstFree(list);
	lstFree(list2);

	printf("Testing chopStr()\n");
	char* str = strdup("test\n test\r\n test\n\n\n");
	printf("str=%s\n", str);
	if(chopStr(str))
	{
		printf("%s\n", strerror(errno));
		return -1;
	}

	printf("str=%s\n", str);
	free(str);

	printf("Testing ptCutStr()\n");
	str = strdup("Hello <a>test</a> World!\n");
	printf("str=%s\n", str);
	char* p = ptCutStr(str, "<a>", "</a>");
	if(!p)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}

	printf("p=%s\n", p);
	free(p);

	printf("Testing writeToFileFormat()\n file: test_test\n");

	fd = open("test_test", O_WRONLY | O_CREAT);
	if(-1 == fd)
	{
		printf("%s\n", strerror(errno));
		return -1;
	}
	if(-1 == writeToFileFormat(fd, "%s %s %s\n", "bla", "bla", "bla"))
	{
		printf("%s\n", strerror(errno));
		return -1;
	}
	close(fd);

	printf("Testing ptGetFileContent()\n");

	char* fcontent = NULL;
	if(-1 == getFileContentFormat(&fcontent, "%s", "test_test"))
	{
		printf("%s\n", strerror(errno));
		return -1;
	}
	printf("File content:\n%s\n", fcontent);

	printf("Testing replaceStrFormat()\n");
	str = strdup("Hello <a>test</a> World!\n");
	printf("str=%s\n", str);
	if(-1 == replaceStrFormat(&str, "test", "%s", "sweet"))
	{
		printf("%s\n", strerror(errno));
		return -1;
	}
	printf("str=%s\n", str);

	printf("\n\nTesting getShellContentFormat()\n");
	if(-1 == getShellContentFormat(&str, "ls %s %s", "-l", "-a *"))
	{
		printf("%s\n", strerror(errno));
		return -1;
	}
	printf("str=%s\n", str);

	return 0;
}
