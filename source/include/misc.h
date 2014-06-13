#ifndef __MISC_H__
#define __MISC_H__

extern int _argc;
extern char **_argv;

void SavePCX256ToFile(unsigned char *buf, int width, int height, unsigned char *pal, char *name);
void SavePCXRGBToFile(unsigned char *buf, int width, int height, char *name);

void set_TimeCount(unsigned long t);
unsigned long get_TimeCount();

long filelength(FILE *fp);
#define stricmp strcasecmp
#define strnicmp strncasecmp
char *strlwr(char *s);

char *itoa(int value, char *string, int radix);
char *ltoa(long value, char *string, int radix);
char *ultoa(unsigned long value, char *string, int radix);

uint16_t SwapInt16L(uint16_t i);
uint32_t SwapInt32L(uint32_t i);

extern FILE * OpenWrite(char *fn);
extern FILE * OpenWriteAppend(char *fn);
extern void CloseWrite(FILE *fp);

extern int WriteSeek(FILE *fp, int offset, int whence);
extern int WritePos(FILE *fp);

extern int WriteInt8(FILE *fp, int8_t d);
extern int WriteInt16(FILE *fp, int16_t d);
extern int WriteInt32(FILE *fp, int32_t d);
extern int WriteBytes(FILE *fp, byte *d, int len);

extern FILE * OpenRead(char *fn);
extern void CloseRead(FILE *fp);

extern int ReadSeek(FILE *fp, int offset, int whence);
extern int ReadLength(FILE *fp);

extern int8_t ReadInt8(FILE *fp);
extern int16_t ReadInt16(FILE *fp);
extern int32_t ReadInt32(FILE *fp);
extern int ReadBytes(FILE *fp, byte *d, int len);



#endif
