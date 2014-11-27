


/* 
80196 emulator/debugger 

	Jeff Weisberg
	Feb. 1992
*/
#include "mnemonic.h"
#include <stdio.h>
float version=1.0;
#define s32bit long int
#define s8bit unsigned char
#define my memory

#define mem(x) memory[x] /* hook for reg. windows */


extern s8bit memory[65536],flag[65536],sfr[16][32];
extern s32bit psw,states,adctime,signed,mode;
extern s32bit opcode,nargs,adrmode,size,src1,src2,asrc1,asrc2,dst,pc;
extern s32bit imask,ipend,timer,timer2,wsr,hsi_time,ad_result;

extern s32bit  check_ints(), debug();
			
s32bit debug(){
	int i, j;
	if (mode&0x2){ /* trace mode */
		for (i=0;i<256;i++){

			if( i % 16 == 0 )
				printf( "00%2.2X: ", i);
			printf("%2.2X ", my[i]);

			if( i % 16 == 15 )
				printf("\n");
		}
	}
	if ((mode&0x3)|(flag[pc]))printf("PC:%4.4X  PSW:%2.2X  OPCODE:%2.2X %s\n",pc,psw,opcode,mnemonic[opcode]);
	if ((mode&0x1)|(flag[pc])){ /* step mode or brkpt hit */
		while(1){
			char buffer[256],buff[256];
			int s=0,foo;
			i=0;
			printf("[96dbg] ");
			gets(buffer);
			while ((buffer[i])&(isspace(buffer[i])))i++;
			switch(buffer[i++]){
				case 'Q':
					printf("BYE-BYE!\n");
					exit(0);
				case 'h': case '?':
					printf("\
?		- help message\n\
v		- version\n\
Q		- quit\n\
t on 		- trace mode on\n\
t off		- trace mode off\n\
l		- list status\n\
b adrs		- set breakpoint\n\
c adrs 		- clear breakpoint\n\
p adrs		- display value\n\
m adrs value	- modify adress\n\
j adrs		- jump to adrs\n\
s		- single step\n\
g		- run\n");
					break;

				case 'v':
					printf("\
96dbg 80196 emulator/debugger (ver %3.2f) -JAWeisberg Feb.1992\n",version);
					break;

				case 's': case 0:
					mode |=1;
					s = 1;
					break;

				case 'l':
					printf("PC:%4.4X  PSW:%2.2X  OPCODE:%2.2X %s\n",pc,psw,opcode,mnemonic[opcode]);
					break;

				case 'g': case 'r':
					mode &= (~1);
					s = 1;
					printf("running...\n");
					break;

				case 'p': case 'd':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ( (sscanf(&(buffer[i]),"%x",&i) ==NULL)){
						printf("display where?\n");
						goto bott;
					}
					printf("%4.4X: %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X %2.2X\n",i,my[i],my[i+1],my[i+2],my[i+3],my[i+4],my[i+5],my[i+6],my[i+7],my[i+8],my[i+9],my[i+10],my[i+11],my[i+12],my[i+13],my[i+14],my[i+15]);
					break;

				case 'j':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ( (sscanf(&(buffer[i]),"%x",&i) ==NULL)){
						printf("jump where?\n");
						goto bott;
					}
					pc = i&0xffff;
					opcode = memory[pc]&0xff;
					nargs = (opcode & 0x20)?2:3;
					size = (opcode & 0x10)?1:2;
					adrmode = opcode & 0x3;
					printf("PC:%4.4X  PSW:%2.2X  OPCODE:%2.2X %s\n",pc,psw,opcode,mnemonic[opcode]);
					break;

				case 'm':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ((sscanf(&(buffer[i]),"%x %x",&i,&foo) ==NULL)){
						printf("set what? where?\n");
						goto bott;
					}
					memory[i&0xffff] = foo&0xff;
					opcode = memory[pc]&0xff;
					nargs = (opcode & 0x20)?2:3;
					size = (opcode & 0x10)?1:2;
					adrmode = opcode & 0x3;
					printf("%4.4X: %2.2X\n",i&0xffff,foo&0xff);
					break;

				case 'b':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ( (sscanf(&(buffer[i]),"%x",&i) ==NULL)){
						printf("set where?\n");
						goto bott;
					}
					flag[i] = 1;
					printf("breakpoint %4.4X set\n",i);
					break;
			
				case 'c':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ( (sscanf(&(buffer[i]),"%x",&i) ==NULL)){
						printf("clear where?\n");
						goto bott;
					}
					flag[i] = 0;
					printf("breakpoint %4.4X cleared\n",i);
					break;

				case 'E':
					printf("E-E-E-E!!!\n");break;
				case 't':
					while ( buffer[i]&&(isspace(buffer[i])))i++;
					if ( (!buffer[i])|(buffer[i]!='o')){
						printf("t on|off ;must specify!\n");
					}else{
						mode &=(~2);
						mode |= (buffer[i+1]=='n')?(printf("trace = on\n"),2):(printf("trace = off\n"),0);
					}
					break;
			
				default: 
					printf("Huh?, type ? for help\n");

				bott:
					break;
			} /*sw*/
			if(s)break;
		} /*wh*/
	}/*if*/
}
