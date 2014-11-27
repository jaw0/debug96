

/* 
80196 emulator/debugger 

	Jeff Weisberg
	Feb. 1992
*/
#define s32bit long int
#define s8bit unsigned char
#define mem(x) memory[x] /* hook for reg. windows */

s8bit memory[65536],flag[65536],sfr[16][32];
s32bit psw=0,states=0,adctime=0,signed=0,mode=1;
s32bit opcode,nargs,adrmode,size,src1,src2,asrc1,asrc2,dst,pc=0x2080;
s32bit imask=0,ipend=0,timer=0,timer2=0,wsr=0,hsi_time,ad_result;

extern s32bit pop(), push(), do_instr(), putargs(), getargs(), setflags(), init();

#define caryf (psw&0x8)
#define zerof (psw&0x80)
#define negf  (psw&0x40)
#define ovf   (psw&0x20)
#define ovtf  (psw&0x10)
#define stkf  (psw&0x1)

do_instr(){
	s32bit foo,bar,baz;

	switch(opcode){

		case 0x1c: case 0x1d: case 0x1e: case 0x1f:
		case 0x10: case 0xe4: case 0xe5: case 0xe6:
		case 0xe8: case 0xe9: case 0xec: case 0xed:
		case 0xee: case 0xf1: 
		/* illegal opcodes, generate int */
			push(pc);
			pc = (memory[0x2011]<<8)|memory[0x2010];
			signed = 2;
			break;

		/* nops and unimplemented instrs */
		case 0xc1: /* bmov */
		case 0xcd: /* bmovi */
			pc ++;
		case 00: /* skip */
			pc++;
		case 0xfd: /* nop */
		case 0xea: /* dpts */
		case 0xeb: /* epts */
			signed = 0;
			states +=2;
			break;
		
		case 0xf8: /* setc */
			psw |= 8;
			states +=2;
			signed = 0;
			break;

		case 0xf9: /* clrc */
			psw &= (~8);
			states +=2;
			signed = 0;
			break;
 		
		case 0xfa: /* di */
			psw &= (~2);
			signed = 0;
			states +=2;
			break;

		case 0xfb: /* ei */
			psw |= 2;
			states +=2;
			signed = 0;
			break;

		case 0xfc: /* clrvt */
			psw &= (~0x10);
			states +=2;
			signed = 0;
			break;

		case 0xfe: /* signed flag */
			signed = 0x3;
			states +=2;
			break;


		case 0xf7: /* trap */
			push(pc);
			states +=16;
			pc = ((memory[2013]<<8)|memory[2012])&0xffff;
			signed = 2;
			break;

		case 0xff: /* rst */
			pc = 0x2080;
			init();
			states +=20;
			break;

		case 0xf0: /* ret */
			pc = pop();
			states +=11;
			signed = 0;
			break;

		case 0xef: /* lcall */
			states +=4;
			push(pc+2);
		case 0xe7: /* ljmp */
			foo = memory[pc++] &0xff;
			foo |= (memory[pc++]<<8)&0xff00;
			pc += foo;
			pc &= 0xffff;
			states +=7;
			signed = 0;
			break;

		case 0x28: case 0x29: case 0x2a: case 0x2b:
		case 0x2c: case 0x2d: case 0x2e: case 0x2f: /* scall */
			states +=4;
			push(pc+1);
		case 0x20: case 0x21: case 0x22: case 0x23: 
		case 0x24: case 0x25: case 0x26: case 0x27: /* sjmp */
			foo = memory[pc++]&0xff;
			foo += (opcode&0x7)<<8;
			foo |= (foo&0x400)?0xfffff800:0x00; /* sign extend */
			pc += foo;
			pc &= 0xffff;
			states +=7;
			signed = 0;
			break;
		
		case 0xe3: /* br */
			pc = getword(memory[pc]);
			signed = 0;
			states +=7;
			break;

		case 0xf2: /* pushf */
			foo = (psw&0xff)|((imask<<8)&0xff00);
			push(foo);
			imask &= 0xff00;
			psw &= 0xff00; 
			states +=6;
			signed = 2;
			break;

		case 0xf3: /* popf */
			foo = pop();
			imask &= 0xff00;
			imask |= (foo>>8)&0xff;
			psw &= 0xff00;
			psw |= foo & 0xff;
			states +=7;
			signed = 2;
			break;

		case 0xf4: /* pusha */
			psw = (psw&0xff)|((wsr<<8)&0xff00);
			foo = (psw&0xff)|((imask<<8)&0xff00);
			push(foo); /* psw, imask */
			foo = ((imask>>8)&0xff)|(psw&0xff00);
			push(foo); /*imask1, wsr */
			psw &= 0xff00; /* wsr not clred */
			imask = 0;
			states +=12;
			signed = 2;
			break;

		case 0xf5: /* popa */
			foo = pop();
			psw = (psw&0xff)|(foo&0xff00);
			imask = (imask&0xff)|((foo&0xff)<<8);
			foo = pop();
			psw = (psw&0xff00)|(foo&0xff);
			imask = (imask&0xff00)|((foo>>8)&0xff);
			wsr = (psw>>8)&0xff;
			states +=12;
			signed = 2;
			break;

		case 0xc8: case 0xc9: case 0xca: case 0xcb: /* push */
			getargs(2,1,adrmode);
			push(src1);
			states +=6;
			signed = 0;
			break;

		case 0xcc: case 0xce: case 0xcf: /* pop */
			getargs(2,1,adrmode);
			putword(dst,pop());
			states +=8;
			signed = 0;
			break;

		case 0x40: case 0x41: case 0x42: case 0x43:
		case 0x50: case 0x51: case 0x52: case 0x53:
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x70: case 0x71: case 0x72: case 0x73: /* and */
			getargs(size,nargs,adrmode);
			foo = src1 & src2;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x44: case 0x45: case 0x46: case 0x47:
		case 0x54: case 0x55: case 0x56: case 0x57:
		case 0x64: case 0x65: case 0x66: case 0x67:
		case 0x74: case 0x75: case 0x76: case 0x77: /* add */
			getargs(size,nargs,adrmode);
			foo = src1 + src2;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x48: case 0x49: case 0x4a: case 0x4b:
		case 0x58: case 0x59: case 0x5a: case 0x5b:
		case 0x68: case 0x69: case 0x6a: case 0x6b:
		case 0x78: case 0x79: case 0x7a: case 0x7b: /* sub */
			getargs(size,nargs,adrmode);
			foo = src1 - src2;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x4c: case 0x4d: case 0x4e: case 0x4f:
		case 0x5c: case 0x5d: case 0x5e: case 0x5f:
		case 0x6c: case 0x6d: case 0x6e: case 0x6f:
		case 0x7c: case 0x7d: case 0x7e: case 0x7f: /* mul */
			getargs(size,nargs,adrmode);
			switch (signed&0x1){
				case 0: foo = src1 * src2;break;
				case 1: switch(size){
					case 1:
						src1 |=(src1&0x80)? 0xffffff00:00;
						src2 |=(src2&0x80)? 0xffffff00:00;
						break;
					case 2:
						src1 |=(src1&0x8000)? 0xffff0000:00;
						src2 |=(src2&0x8000)? 0xffff0000:00;
						break;
					}
					foo = src1 * src2;
					break;
			}
			switch(size){
				case 2: putlong(dst,foo);break;
				case 1: putword(dst,foo);break;
			}
			states +=14;
			signed = 0;
			break;

		case 0x80: case 0x81: case 0x82: case 0x83:
		case 0x90: case 0x91: case 0x92: case 0x93: /* or */
			getargs(size,2,adrmode);
			foo = src1 | src2;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x84: case 0x85: case 0x86: case 0x87:
		case 0x94: case 0x95: case 0x96: case 0x97: /* xor */
			getargs(size,2,adrmode);
			foo = src1 ^ src2;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x88: case 0x89: case 0x8a: case 0x8b:
		case 0x98: case 0x99: case 0x9a: case 0x9b: /* cmp */
			getargs(size,2,adrmode);
			foo = src1 - src2;
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x8c: case 0x8d: case 0x8e: case 0x8f:
		case 0x9c: case 0x9d: case 0x9e: case 0x9f: /* div */
			getargs(size,2,adrmode);
			switch (signed&0x1){
				case 0: /* unsigned */
					foo = src1 / src2;
					bar = src1 % src2;
					break;
				case 1: switch(size){
					case 1:
						src1 |=(src1&0x80)? 0xffffff00:00;
						src2 |=(src2&0x80)? 0xffffff00:00;
						break;
					case 2:
						src1 |=(src1&0x8000)? 0xffff0000:00;
						src2 |=(src2&0x8000)? 0xffff0000:00;
						break;
					}
					foo = src1 / src2;
					bar = src1 % src2;
					break;	
			}
			switch(size){
				case 2: 
				putword(dst,foo); putword(dst+2,bar); break;
				case 1: 
				putbyte(dst,foo); putbyte(dst+1,bar); break;
			}
			setflags(foo,size);
			states +=16;
			signed = 0;
			break;

		case 0xa4: case 0xa5: case 0xa6: case 0xa7:
		case 0xb4: case 0xb5: case 0xb6: case 0xb7: /* addc */
			getargs(size,2,adrmode);
			foo = src1 + src2 + ((psw&0x8)>>3);
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0xa8: case 0xa9: case 0xaa: case 0xab:
		case 0xb8: case 0xb9: case 0xba: case 0xbb: /* subc */
			getargs(size,2,adrmode);
			foo = src1 - src2 + ((psw&0x8)>>3) - 1;
			switch(size){
				case 2: putword(dst,foo);break;
				case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0xa0: case 0xa1: case 0xa2: case 0xa3:
		case 0xb0: case 0xb1: case 0xb2: case 0xb3: /* ld */
			getargs(size,2,adrmode);
			switch(size){
				case 2: putword(dst,src2);break;
				case 1: putbyte(dst,src2);break;
			}
			signed = 0;
			states +=4;
			break;

		case 0xc0: case 0xc2: case 0xc3:
		case 0xc4: case 0xc6: case 0xc7: /* st */
			size = (opcode&4)?1:2;
			getargs(size,-2,adrmode);
			switch (size){
				case 2: putword(asrc1,getword(dst)); break;
				case 1: putbyte(asrc1,getbyte(dst)); break;
			}
			signed = 0;
			states +=4;
			break;

		case 0x01: case 0x11: /* clr */
			switch (size){
				case 2: putword(memory[pc++],0);break;
				case 1: putbyte(memory[pc++],0);break;
			}
			states +=3;
			setflags(0,2);
			signed = 0;
			break;

		case 0x02: case 0x12: /* not */
			getargs(size,1,0);
			foo = ~ src1;
			switch(size){
				case 2: putword(dst,foo);break;
			 	case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=3;
			signed = 0;
			break;

		case 0x03: case 0x13: /* neg */
			getargs(size,1,0);
			foo = - src1;
			switch(size){
				case 2: putword(dst,foo);break;
			 	case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=3;
			signed = 0;
			break;

		case 0x05: case 0x15: /* dec */
			getargs(size,1,0);
			foo =  src1 - 1;
			switch(size){
				case 2: putword(dst,foo);break;
			 	case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			states +=4;
			signed = 0;
			break;

		case 0x07: case 0x17: /* inc */
			getargs(size,1,0);
			foo =  src1 + 1;
			switch(size){
				case 2: putword(dst,foo);break;
			 	case 1: putbyte(dst,foo);break;
			}
			setflags(foo,size);
			signed = 0;
			states +=4;
			break;

		case 0x30: case 0x31: case 0x32: case 0x33:
		case 0x34: case 0x35: case 0x36: case 0x37: /* jbc */
		case 0x38: case 0x39: case 0x3a: case 0x3b:
		case 0x3c: case 0x3d: case 0x3e: case 0x3f: /* jbs */
			foo = mem( memory[pc++] );
			bar = memory[pc++];
			pc += (((1<<(opcode&0x07))&foo)^(opcode&0x08)) ?0:bar;	
			signed = 0;
			states +=7;
			break;

		case 0x06: case 0x16: /* ext */
			foo = memory[pc++];
			bar = (size==1)?getbyte(foo):getword(foo);
			switch(size){
				case 1:
					bar |= (bar&0x80)?0xff00:00;
					putword(foo,bar);
					break;
				case 2:
					bar |= (bar&0x8000)?0xffff0000:00;
					putlong(foo,bar);
					break;
			}
			setflags(bar,size);
			signed = 0;
			states +=4;
			break;	

		case 0x04: case 0x14: case 0x0b: case 0x1b: /* xch */
			getargs(size,2,adrmode);
			switch(size){
				case 1:
					putbyte(dst,src1);
					putbyte(asrc1,src2);
					break;
				case 2:
					putword(dst,src1);
					putword(asrc1,src2);
					break;
			}
			states +=5;
			signed = 0;
			break;

		case 0x08: case 0x18: /* shr */
			foo = memory[pc++];
			foo = (foo>15)?getbyte(foo):foo;
			asrc1 = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			src1 <<= (16-foo);
			(size==1)?putbyte(asrc1,src1>>16):putword(asrc1,src1>>16);
			psw &=0x16;
			psw |= (src1&0x80)>>4;/*c*/
			psw |= (src1&0x7f)?1:0;/*st*/
			psw |= (src1&0xffff0000)?0x80:0;/*z*/
			states +=6+foo;
			signed = 0;
			break;

		case 0xa: case 0x1a: /* shra */
			foo = memory[pc++];
			foo = (foo>15)?getbyte(foo):foo;
			asrc1 = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			src2 = src1;
			src1 <<= (16-foo);
			psw &=0x16;
			psw |= (src1&0x80)>>4;/*c*/
			psw |= (src1&0x7f)?1:0;/*st*/
			psw |= (src1&0xffff0000)?0x80:0;/*z*/		
			src2 |= ((size==1)?((src2&0x80)?0xffffff00:00):((src2&0x8000)?0xffff0000:00));
			src2 >>=foo;
			(size==1)?putbyte(asrc1,src2):putword(asrc1,src2);
			states +=6+foo;
			signed = 0;
			break;

		case 0x09: case 0x19: /* shl */
			foo = memory[pc++];
			foo = (foo>15)?getbyte(foo):foo;
			asrc1 = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			src1 <<= foo;
			(size==1)?putbyte(asrc1,src1):putword(asrc1,src1);
			psw &=0x17;
			psw |= (src1&((size==1)?0xff:0xffff))?0x80:0;/*z*/
			src1 >>= (size==1)?7:15;
			psw |= (src1&0x2)<<2;/*c*/
			psw |= (src1&0x1)<<6;/*n*/
			states +=6+foo;
			signed = 0;
			break;

		case 0xd0: /*jnst*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!stkf)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xd1: /*jnh*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += ((!caryf)|zerof)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xd2: /*jgt*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (negf&&zerof)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xd3: /*jnc*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!caryf)?foo:0;
			signed = 0;
			states +=6;
			break;

		case 0xd4: /*jnvt*/
			foo = memory[pc++];
			foo |= (foo&&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!ovtf)?foo:0;
			signed = 0;
			states +=6;
			break;

		case 0xd5: /*jnv*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!ovf)?foo:0;
			signed = 0;
			states +=6;
			break;

		case 0xd6: /*jge*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!negf)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xd7: /*jne*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (!zerof)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xd8: /*jst*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (stkf)?foo:0;
			signed = 0;
			states +=6;
			break;

		case 0xd9: /*jh*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (caryf&&(!zerof))?foo:0;
			signed = 0;
			states +=6;
			break;

		case 0xda: /*jle*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (negf||zerof)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xdb: /*jc*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (caryf)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xdc: /*jvt*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (ovtf)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xde: /*jlt*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (negf)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xdf: /*je*/
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			pc += (zerof)?foo:0;
			states +=6;
			signed = 0;
			break;

		case 0xe0: case 0xe1: /*djnz*/
			size = 0xc2 - opcode;
			asrc1 = memory[pc++];
			src1 = (size==1)?(getbyte(asrc1)&0xff):(getword(asrc1)&0xffff);
			foo = memory[pc++];
			foo |= (foo&0x80)?0xffffff00:0x00; /* sign extend */
			src1 --;
			src1 &= (size==1)?(putbyte(src1,asrc1),0xff):(putword(src1,asrc1),0xffff);
			pc += (src1)?0:foo;
			states +=6;
			signed = 0;
			break;


		
	}
}
