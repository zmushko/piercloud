/*
 * base64.h
 *
 */

#ifndef BASE64_H_
#define BASE64_H_

int b64_decode(const char *in, long in_size, char **out, long *out_size);
int b64_encode(const char *in, long in_size, char **out, long *out_size);

#endif /* BASE64_H_ */
