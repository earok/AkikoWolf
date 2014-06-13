//typedef UBYTE unsigned char;
//typedef UBYTE * PLANEPTR;
#include <exec/types.h>
typedef UBYTE * PLANEPTR;
void extern c2p_8_020 (__reg ("a2") UBYTE *fBUFFER,
                      __reg ("a4") PLANEPTR *planes,
                      __reg ("d0") ULONG signals1,
                      __reg ("d1") ULONG signals2,
                      __reg ("d4") ULONG signals3,
                      __reg ("d2") ULONG pixels,     // width*height
                      __reg ("d3") ULONG offset,     // byte offset into plane
                      __reg ("a5") struct Task *othertask,
                      __reg ("a3") UBYTE *chipbuffer); // 2*width*height

void extern c2p_6_020 (__reg ("a2") UBYTE *fBUFFER,
                      __reg ("a4") PLANEPTR *planes,
                      __reg ("d0") ULONG signals1,
                      __reg ("d1") ULONG signals2,
                      __reg ("d4") ULONG signals3,
                      __reg ("d2") ULONG pixels,     // width*height
                      __reg ("d3") ULONG offset,     // byte offset into plane
                      __reg ("a1") UBYTE *xlate,
                      __reg ("a5") struct Task *othertask,
                      __reg ("a3") UBYTE *chipbuffer); // 2*width*height
