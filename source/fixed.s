		mc68030
		OPT 3
	;	OPT NQLPSMDRBT
	;	fpu
		
		xdef	@FixedByFrac
		
		section text,code
		
		cnop	0,4

@FixedByFrac:	muls.l	d1,d1:d0
		move.w	d1,d0
		swap	d0
		rts
		
		end