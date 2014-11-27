


/* 
80196 emulator/debugger 

	Jeff Weisberg
	Feb. 1992
*/
#include <stdio.h>
#define s32bit long int
#define s8bit unsigned char
#define mem(x) memory[x] /* hook for reg. windows */


extern s8bit memory[65536],flag[65536],sfr[16][32];
extern s32bit psw,states,adctime,signed,mode;
extern s32bit opcode,nargs,adrmode,size,src1,src2,asrc1,asrc2,dst,pc;
extern s32bit imask,ipend,timer,timer2,wsr,hsi_time,ad_result;

extern s32bit pop(), push(), cycle(), do_instr(), putargs(), getargs(), setflags(), update_sfr(), check_ints(), debug();

main(argc,argv)
	int argc;
	char **argv;{
	int loc=0,i;
	FILE *fI;

	if ((argc !=2)&(argc !=3)){
		printf("usage: %s file [adrs]\n",argv[0]);
		exit(-1);
	}
	if ((fI=fopen(argv[1],"r"))==NULL){
		printf("%s: could not open file %s\n",argv[0],argv[1]);
		exit(-1);
	}
	if (argc==3) loc = atoi(argv[2]);
	while ( !feof(fI) ) fread( &(memory[(loc++)&0xffff]),1,1,fI);
	for(i=0;i<65536;i++)flag[i]=0;
	init();
	cycle();
}
s32bit init(){
	int i,j;
	pc = 0x2080;
	psw = wsr = imask = ipend = timer = 0;
	for (i=0;i<0x18;i++)memory[i] = 0;
	for(j=0;j<16;j++)for(i=0;i<0x18;i++)sfr[j][i] = 0;
}
s32bit cycle(){

	while(1){
		int i,nwsr;
		for(i=0;i<0x18;i++)sfr[wsr][i]=memory[i];
		timer = (sfr[0][0xa]&0xff)|((sfr[0][0xb]<<8)&0xff00);
		imask = (memory[8])|((memory[0x13]<<8)&0xff00);
		ipend = (memory[9])|((memory[0x12]<<8)&0xff00);
		timer += (states>>3);
		ipend |= (timer&0x10000)?(timer&=0xffff),0x20:0;
		states &= 7;
		sfr[0][0x0a] = timer&0xff;
		sfr[0][0x0b] = (timer>>8)&0xff;
		nwsr = memory[0x14];
		for(i=0;i<0x18;i++)memory[i] = sfr[nwsr][i];
		check_ints();
		memory[0x14] = nwsr;
		memory[0x08] = imask&0xff;
		memory[0x13] = (imask>>8)&0xff;
		memory[0x09] = ipend&0xff;
		memory[0x12] = (ipend>>8)&0xff;
		memory[0] = 0;
		pc &= 0xffff;
		opcode = memory[pc]&0xff;
		nargs = (opcode & 0x20)?2:3;
		size = (opcode & 0x10)?1:2;
		adrmode = opcode & 0x3;
		debug();
		pc++;
		do_instr();
	}
}

s32bit push(foo)
	s32bit foo;{
	mem(0x18) -= 2;
	mem(0x19) -= ( ((mem(0x18)&0xfe)==0xfe)?1:0); /* sp-- */
	putword( ((mem(0x19)<<8)|mem(0x18)),foo);
}

s32bit pop(){
	s32bit foo;
	foo = getword( ((mem(0x19)<<8)|mem(0x18)) );
	mem(0x18) += 2;
	mem(0x19) += ( ((mem(0x18)&0xfe)==0x00)?1:0);
	return(foo);
}
s32bit getword(foo)
	s32bit foo;{
	s32bit bar;
	bar = mem(foo++) &0xff;
	bar |= (mem(foo)<<8)&0xff00;
	return (bar);
}
s32bit putword(foo, bar)
	s32bit foo, bar;{
	mem(foo) = bar & 0xff;
	mem(foo + 1) = (bar & 0xff00) >> 8;
}

s32bit getbyte(foo)
	s32bit foo;{
	s32bit bar;
	bar = mem(foo)&0xff;
	return(bar);
}
s32bit putbyte(foo,bar)
	s32bit foo,bar;
{
	if( foo == 7 ){
		/* serial output */
		putchar( bar );
		/* set SP_TI */
		/* XXX - should deal with window */
		mem(18) |= 32;
		sfr[0][18] = mem(18);
	}
	mem(foo) = bar&0xff;
}
s32bit getlong(foo)
	s32bit foo;{
	s32bit bar;
	bar = getword(foo)|(getword(foo+2)<<16);
	return(bar);
}
s32bit putlong(foo,bar)
	s32bit foo,bar;{
	putword(foo,bar);
	putword(foo+2,bar>>16);
}


getargs(size,num,admode)
	s32bit size,num,admode;{

	switch(admode){
		s32bit foo;
		case 00:
			asrc1 = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			break;
		case 01:
			asrc1 = 00;
			src1 = memory[pc++]&0xff;
			if (size==2) src1 |= (memory[pc++]<<8)&0xff00;
			states ++;
			break;
		case 02:
			foo = memory[pc++];
			asrc1 = getword(foo&0xfe);
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			if (foo&0x1) putword(foo&0xfe,asrc1+size);
			states += 2 + foo&0x1;
			break;
		case 03:
			foo = memory[pc++];
			asrc1 = getword(foo&0xfe) + (memory[pc++]&0xff);
			if (foo&0x1) asrc1 += ((memory[pc++]<<8)&0xff00);
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			states += 2 + foo&0x1;
			break;
	}
	switch(num){
		case 1:
			src2 = src1; 
			dst = asrc1;
			break;
		case 2:
			src2 = src1;
			asrc1 = dst = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			break;
	        case -2:
		        /* st hack */
		        dst = memory[pc++];
			break;
			
		case 3:
			src2 = src1;
			asrc1 = memory[pc++];
			src1 = (size==1)?getbyte(asrc1):getword(asrc1);
			dst = memory[pc++];
			break;
	}
}

s32bit setflags(val,siz)
	s32bit val,siz;{
	psw &= 0x37;
	psw |= ( ((val&((siz==1)?0xff:0xffff))==00)?0x80:00);/*z*/
	psw |= ( ((val&((siz==1)?0x80:0x8000))==00)?00:0x40);/*n*/
	psw |= ( ((val&((siz==1)?0x100:0x10000))==00)?0:0x8);/*c*/




}

s32bit check_ints(){
	s32bit i,n;
	if ((!(signed&2))&&(psw&0x2)){
		i = imask&ipend;
		n = (i&0x8000)?15:(i&0x4000)?14:(i&0x2000)?13:(i&0x1000)?12:(i&0x800)?11:(i&0x400)?10:(i&0x200)?9:(i&0x100)?8:(i&0x80)?7:(i&0x40)?6:(i&0x20)?5:(i&0x10)?4:(i&0x8)?3:(i&4)?2:(i&1)?1:0;
		if (i){
			push(pc);
			pc = 0x2000 + n*2 + (n>7)?0x20:0;
			ipend ^= (1<<n);
			signed = 2;
		}
	}
}
