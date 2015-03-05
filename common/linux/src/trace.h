/*
 *  trace.h
 *
 *  Author: Andrey Zmushko
 */

#ifndef __TRACE_H
#define __TRACE_H

#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <string.h>

#define TRACEFILE	"/var/log/piercloud.log"
#define DATATRACE	"/var/log/.piercloud_data_trace"

#define TRACE(...)	do{ \
				if (!access(TRACEFILE, F_OK)) \
				{ \
					FILE* f = fopen(TRACEFILE, "a"); \
					char datetime[256] = {'\0', }; \
					time_t t = time(NULL);	\
					struct tm tms; \
					struct tm* tmp = localtime_r(&t, &tms); \
					if (tmp) \
					{ \
						strftime(datetime, sizeof(datetime), "%b %e %Y %H:%M:%S", tmp); \
					} \
					fprintf(f, "- %s %s:%d pid:%d\n  ", datetime, __FILE__, __LINE__, getpid()); \
					fprintf(f, __VA_ARGS__); \
					fprintf(f, "\n"); \
					fclose(f); \
				} \
			}while(0)

#define ERROR(...)	do{ \
				int safe_errno = errno; \
				if (!access(TRACEFILE, F_OK)) \
				{ \
					FILE* f = fopen(TRACEFILE, "a"); \
					char datetime[256] = {'\0', }; \
					time_t t = time(NULL);	\
					struct tm tms; \
					struct tm* tmp = localtime_r(&t, &tms); \
					if (tmp) \
					{ \
						strftime(datetime, sizeof(datetime), "%b %e %Y %H:%M:%S", tmp); \
					} \
					fprintf(f, "- %s %s:%d pid:%d\n  ", datetime, __FILE__, __LINE__, getpid()); \
					fprintf(f, "*** ERROR ***:"); \
					fprintf(f, __VA_ARGS__); \
					fprintf(f, ", errno=%d:%s\n", safe_errno, strerror(safe_errno)); \
					fclose(f); \
				} \
				errno = safe_errno; \
			}while(0)

#define ASSERT(A)	do{ \
				if ((A)) \
				{ \
					int safe_errno = errno; \
					if (!access(TRACEFILE, F_OK)) \
					{ \
						FILE* f = fopen(TRACEFILE, "a"); \
						char datetime[256] = {'\0', }; \
						time_t t = time(NULL);	\
						struct tm tms; \
						struct tm* tmp = localtime_r(&t, &tms); \
						if (tmp) \
						{ \
							strftime(datetime, sizeof(datetime), "%b %e %Y %H:%M:%S", tmp); \
						} \
						fprintf(f, "- %s %s:%d pid:%d\n  ", datetime, __FILE__, __LINE__, getpid()); \
						fprintf(f, "ABORT:"); \
						fprintf(f, "%s", #A); \
						fprintf(f, ", errno=%d:%s\n", safe_errno, safe_errno ? strerror(safe_errno) : ""); \
						fclose(f); \
					} \
					errno = safe_errno; \
					exit(EXIT_SUCCESS); \
				} \
			}while(0)

#define TDATA(...)	do{ \
				if (!access(TRACEFILE, F_OK) && !access(DATATRACE, F_OK)) \
				{ \
					FILE* f = fopen(TRACEFILE, "a"); \
					fprintf(f, __VA_ARGS__); \
					fclose(f); \
				} \
			}while(0)

#endif
