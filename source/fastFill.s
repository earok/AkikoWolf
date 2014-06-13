		mc68030
		OPT 3

		xdef	_FastFillScreen
		
		section text,code
		
		cnop	0,4

; void __asm FastClearScreen (register __a6 byte *gfx,
;			    register __d1 long value);
; a6 -> GfxBuffer address
; d1 -> filling value

_FastFillScreen:
		movem.l	d0-d7/a0-a5,-(sp)
		move.l	sp, TempSp	
		move.l	a6, sp
		move.l	d1, d2
		move.l	d1, d3
		move.l	d1, d4
		move.l	d1, d5
		move.l	d1, d6
		move.l  d1, d7
		move.l	d1, a0
		move.l	d1, a1
		move.l	d1, a2
		move.l	d1, a3
		move.l	d1, a4
		move.l	d1, a5
		move.l	d1, a6
		move.l	#56, d0
loop1	movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		movem.l	d1-d7/a0-a6,-(sp)
		dbra.s	d0, loop1
		movem.l d1-d7/a0-a6,-(sp)
		movem.l	d1-d4,-(sp)
		move.l	TempSp, sp
		movem.l	(sp)+,d0-d7/a0-a5
		rts
TempSp:
		dc.l 0		
		end