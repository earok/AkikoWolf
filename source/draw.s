    machine 68030
    OPT 3
;    OPT NQLPSMDRBT
    xdef _ScaledDraw
    xdef _ScaledDrawTrans
_ScaledDraw:
*    movem.l d0-d3/a2-a3,-(sp)
*	 bra.s l4
	dbra.s	d2,l3
*	subq.b	#1,d2
*	tst.b	d2
*	bne.s l3
    rts
l3
    move.l  d3,d0
	swap	d0

*	 move.w	 d0,d1
	move.b	(a3,d0.w),(a2)
	add.w	#320,a2
*    swap    d3
	add.l	d4,d3
l4

*	subq.b	#1,d2
*	tst.b	d2
*	bne.s l3
	dbra.s d2,l3
*	 move.b	 d2,d0
*    movem.l (sp)+,d0-d3/a2-a3
    rts

_ScaledDrawTrans:
*    movem.l d0-d3/a2-a3,-(sp)
*	 bra.s l4
	dbra.s	d2, l5
*	subq.b	#1,d2
*	tst.b	d2
*	bne.s l5
    rts
l5
    move.l  d3,d0
	swap	d0

*	move.w	 d0,(a3,d0.w)
	cmp.b #255,(a3,d0.w)
	beq.s   l6
	move.b	(a3,d0.w),(a2)
l6
	add.w	#320,a2
*    swap    d3
	add.l	d4,d3

*	subq.b	#1,d2
*	tst.b	d2
*	bne.s l5
	dbra.s d2,l5
*	 move.b	 d2,d0
*    movem.l (sp)+,d0-d3/a2-a3
    rts
