#ifndef _ERR_
#define _ERR_

void safe_aux(const char* line_text, int line_number, const char* file_name, const char *emsg);

void syserr(const char *fmt, ...);

void fatal(const char *fmt, ...);

#define safe_call(fun, emsg) {fun<0?safe_aux(#fun,__LINE__,__FILE__,emsg):(void)0;}

#endif
