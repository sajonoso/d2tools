#include <windows.h>
#include <stdio.h>
#include "Types.h"
#include "Dclib.h"

extern	UInt32	ExtWavUnp1(UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x01
extern	UInt32	ExtWavUnp2(UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x40
extern	UInt32	ExtWavUnp3(UInt32,UInt32,UInt32,UInt32); // Call for metod: 0x80

int		ReadMpqInfo();
void	BuildBaseMassive();
int		InitializeLocals();
void	FreeLocals();
DWORD	Crc(char *string,DWORD *massive_base,DWORD massive_base_offset);
void	Decode(DWORD *data_in, DWORD *massive_base, DWORD crc, DWORD lenght);
UInt16	read_data(UInt8 *buffer,UInt16 size,void *crap);
void	write_data(UInt8 *buffer,UInt16 size,void *crap);
int		ExtractTo(FILE *fp_new,DWORD entry);


DWORD	offset_mpq;			// Offset to MPQ file data
DWORD	offset_htbl;		// Offset to hash_table of MPQ
DWORD	offset_btbl;		// Offset to block_table of MPQ
DWORD	lenght_mpq_part;	// Lenght of MPQ file data
DWORD	lenght_htbl;		// Lenght of hash table
DWORD	lenght_btbl;		// Lenght of block table
DWORD	*hash_table;		// Hash table
DWORD	*block_table;		// Block table
DWORD	count_files;		// Number of files in MPQ (calculated from size of block_table)
DWORD	massive_base[0x500];// This massive is used to calculate crc and decode files
char	*filename_table;	// Array of MPQ filenames
char	*identify_table;	// Bitmap table of MPQ filenames 1 - if file name for this entry is known, 0 - if is not

char	*file_name;				// Name of archive
char	work_dir[_MAX_PATH];	// Work directory
char	prnbuf[_MAX_PATH+100];	// Buffer
char	default_list[_MAX_PATH];// Path to list file
FILE	*fpMpq;

// This is used to decompress DCL-compressed and WAVE files
DWORD	avail_metods[4]={0x08,0x01,0x40,0x80};
DWORD	lenght_write;
UInt8	*global_buffer,*read_buffer_start,*write_buffer_start,*explode_buffer;
UInt32	*file_header;
typedef struct {
	char	*buf_in;
	char	*buf_out;
} params;


int main(int argc, char* argv[])
{
	char	*tmp;
	int		command=0,entry,i,len;
	FILE	*fp_new;

	printf("MPQ Viewer - example console application to deal with MPQ archives.\nAuthors: Tom Amigo and AK74. August, 2000.\n");
	if(argc==3) {
		if(!stricmp(argv[1],TEXT("l")))
			command=1;
	} else {
		if(argc==4) {
			if(!stricmp(argv[1],TEXT("e"))) {
				entry=atol(argv[3]);
				if(entry)
					command=2;
			}
		}
	}
	if(!command) {
		printf("\nUsage:\nList files in archive:     mpqview l <archive>\nExtract file from archive: mpqview e <archive> <entry #>\n");
		return 0;
	}
	file_name=argv[2];
	fpMpq=fopen(file_name,"rb");
	if(!fpMpq) {
		printf("\nError! Can't open file: %s",file_name);
		return -1;
	}

	GetModuleFileName(NULL,work_dir,_MAX_PATH);
	tmp=strrchr(work_dir,'\\');
	if(tmp)
		*tmp=0;
	sprintf(default_list,"%s\\list.txt",work_dir);
	
	printf("\nAnalizing archive...");
	if(ReadMpqInfo())
		return -1;
	switch(command) {
		case 1:
			printf("\n    N  Name                                                                Size");
			printf("\n ------------------------------------------------------------------------------");
			for(i=0;i<(int)count_files;i++) {
				printf("\n%5i  %s",i+1,filename_table+i*_MAX_PATH);
				len=63-strlen(filename_table+i*_MAX_PATH);
				while(len>0) {
					printf(" ");
					len--;
				}
				printf("%9i",*(block_table+i*4+2));
			}
			printf("\n ------------------------------------------------------------------------------\n");
			break;
		case 2:
			entry--;
			if(entry<0 || entry>(int)count_files-1)
				printf("\nError! Invalid entry number %i (Valid entry numbers are 1-%i).",entry+1,count_files);
			else {
				tmp=strrchr(filename_table+entry*_MAX_PATH,'\\');
				if(tmp)
					tmp++;
				else
					tmp=filename_table+entry*_MAX_PATH;
				sprintf(prnbuf,"%s\\%s",work_dir,tmp);
				fp_new=fopen(prnbuf,"wb");
				if(!fp_new) {
					printf("\nError: Can't create file for output \'%s\'",prnbuf);
					break;
				}
				printf("\nExtracting \'%s\'...",tmp);
				ExtractTo(fp_new,entry);
				fclose(fp_new);
				printf("\nDone.");
			}
	}
	fclose(fpMpq);
	FreeLocals();
	return 0;
}

/******************************************************************************
*
*	FUNCTION:	ReadMpqInfo() - analizing archive
*
******************************************************************************/
int ReadMpqInfo()
{
	DWORD	mpq_header[2]={0x1a51504d,0x00000020};
	int		i,j;
	DWORD	detected=0;
	DWORD	tmp,scrc1,scrc2,scrc3,pointer_ht;
	FILE	*fpList;

	static char	*name_htable="(hash table)";	// Name of hash table (used to decode hash table)
	static char	*name_btable="(block table)";	// Name of block table (used to decode block tabl
 
	while(fread(&tmp,sizeof(DWORD),1,fpMpq)) {
		if(mpq_header[0]==tmp) {
			fread(&tmp,sizeof(DWORD),1,fpMpq);
			if(mpq_header[1]==tmp) {
				detected=1;
				break;
			}
		}
	}
	if(!detected) {
		printf("\nError: File \'%s\' is not valid MPQ archive",file_name);
		fclose(fpMpq);
		return -1;
	}
	offset_mpq=ftell(fpMpq)-8;
	fread(&lenght_mpq_part,sizeof(DWORD),1,fpMpq);
	fseek(fpMpq,offset_mpq+16,SEEK_SET);
	fread(&offset_htbl,sizeof(DWORD),1,fpMpq);
	fread(&offset_btbl,sizeof(DWORD),1,fpMpq);
	fread(&lenght_htbl,sizeof(DWORD),1,fpMpq);
	lenght_htbl*=4;
	fread(&lenght_btbl,sizeof(DWORD),1,fpMpq);
	count_files=lenght_btbl;
	lenght_btbl*=4;

	BuildBaseMassive();
	if(InitializeLocals()) {
		fclose(fpMpq);
		return -2;
	}

	fseek(fpMpq,offset_mpq+offset_htbl,SEEK_SET);
	fread(hash_table,sizeof(DWORD),lenght_htbl,fpMpq);
	fseek(fpMpq,offset_mpq+offset_btbl,SEEK_SET);
	fread(block_table,sizeof(DWORD),lenght_btbl,fpMpq);
	tmp=Crc(name_htable,massive_base,0x300);
	Decode(hash_table,massive_base,tmp,lenght_htbl);
	tmp=Crc(name_btable,massive_base,0x300);
	Decode(block_table,massive_base,tmp,lenght_btbl);
 
	fpList=fopen(default_list,"rt");
	if(fpList)	{
		while(fgets(prnbuf,_MAX_PATH,fpList)!=0) {
			if(*(prnbuf+strlen(prnbuf)-1)=='\n')
				*(prnbuf+strlen(prnbuf)-1)=0;
			scrc1=Crc(prnbuf,massive_base,0);
			scrc2=Crc(prnbuf,massive_base,0x100);
			scrc3=Crc(prnbuf,massive_base,0x200);
			pointer_ht=(scrc1&(lenght_htbl/4-1))*4;
			for(;pointer_ht<lenght_htbl;pointer_ht+=4) {
				if((*(hash_table+pointer_ht)==scrc2) && (*(hash_table+pointer_ht+1)==scrc3)) {
					sprintf((filename_table+_MAX_PATH*(*(hash_table+pointer_ht+3))),"%s",prnbuf);	// - fill filename_table
					*(identify_table+*(hash_table+pointer_ht+3))=1;									// . fill identify table
					break;
				}
			}
		}
		fclose(fpList);
	} else
		printf("\nWarning: Can't open default list: %s",default_list);
	j=1;
	for(i=0;i<(int)count_files;i++) {					// if there are not identified files, then
		if(!*(identify_table+i)) {						// fill filename_table with "unknow\unknowN
			sprintf(filename_table+MAX_PATH*i,"unknow\\unk%05li.xxx",j);
			j++;										
		}
	}
	return 0;
}

/******************************************************************************
*
*	FUNCTION:	InitializeLocals() - Allocation of memory for hash_table,block_table,
*                                    filename_table,identify_table and working buffers 
*                                    to decompress files
*				                      
******************************************************************************/
int InitializeLocals()
{
	global_buffer=(UInt8 *)malloc(0x60000);				// Allocation 384 KB for global_buffer
	if(!global_buffer) {
		printf("\nError! Insufficient memory");
		return -1;
	}
	read_buffer_start=global_buffer;					// 4 KB for read_buffer
	write_buffer_start=global_buffer+0x1000;			// 4 KB for write_buffer
	explode_buffer=global_buffer+0x2000;				// 16 KB for explode_buffer
	file_header=(DWORD *)(global_buffer+0x6000);		// 360 KB for file_header (max size of unpacked file can't exceed 360 MB)

	hash_table=(DWORD *)malloc(lenght_htbl*4);
	block_table=(DWORD *)malloc(lenght_btbl*4);
	filename_table=(char *)calloc(lenght_btbl/4,_MAX_PATH);
	identify_table=(char *)calloc(lenght_btbl/4,sizeof (char));
	if(hash_table && block_table && filename_table && identify_table)
		return 0;
	else {
		printf("\nError! Insufficient memory");
		return -1;
	}
}

/******************************************************************************
*
*	FUNCTION:	FreeLocals() - free memory
*				                      
******************************************************************************/
void FreeLocals()
{	
	if(global_buffer)
		free(global_buffer);
	if(hash_table)
		free(hash_table);
	if(block_table)
		free(block_table);
	if(filename_table)
		free(filename_table);
	if(identify_table)
		free(identify_table);
	return;
}

/******************************************************************************
*
*	FUNCTION:	BuildBaseMassive() - fill massive_base
*
******************************************************************************/
void BuildBaseMassive()
{
	DWORD	s1;
	int	i,j;
	ldiv_t divres;

	divres.rem=0x100001;
	for(i=0;i<0x100;i++) {
		for(j=0;j<5;j++) {
			divres=ldiv(divres.rem*125+3,0x002AAAAB);
			s1=(divres.rem&0xFFFFL)<<0x10;
			divres=ldiv(divres.rem*125+3,0x002AAAAB);
			s1=s1|(divres.rem&0xFFFFL);
			massive_base[i+0x100*j]=s1;
		}
	}
	return;
}

/******************************************************************************
*
*	FUNCTION:	Crc(char *,DWORD *,DWORD) - calculate crc
*
******************************************************************************/
DWORD Crc(char *string,DWORD *massive_base,DWORD massive_base_offset)
{
 char	byte;
 DWORD	crc=0x7fed7fed;
 DWORD	s1=0xEEEEEEEE;
 
 byte=*string;
 while(byte) {
	if(byte>0x60 && byte<0x7B)
		byte-=0x20;
	crc=*(massive_base+massive_base_offset+byte)^(crc+s1);
	s1+=crc+(s1<<5)+byte+3;
	string++;
	byte=*string;
 }
 return crc;
}

/******************************************************************************
*
*	FUNCTION:	read_data(UInt8 *,UInt16,void *) (called by explode)
*
******************************************************************************/
UInt16 read_data(UInt8 *buffer,UInt16 size,void *crap)
{
	params *param=(params *)crap;
	memcpy(buffer,param->buf_in,size);
	param->buf_in+=size;
	return size;
}
/******************************************************************************
*
*	FUNCTION:	write_data(UInt8 *,UInt16,void *) (called by explode)
*
******************************************************************************/
void write_data(UInt8 *buffer,UInt16 size,void *crap)
{
	params *param=(params *)crap;
	memcpy(param->buf_out,buffer,size);
	param->buf_out+=size;
	lenght_write+=size;
}


/******************************************************************************
*
*	FUNCTION:	Decode(DWORD *,DWORD *,DWORD,DWORD) - decode data
*
******************************************************************************/
void Decode(DWORD *data_in, DWORD *massive_base, DWORD crc, DWORD lenght)
{
 DWORD	i,dec;
 DWORD	s1=0xEEEEEEEE;
 for(i=0;i<lenght;i++) {
	s1+=*(massive_base+0x400+(crc&0xFFL));
	dec=*(data_in+i)^(s1+crc);
	s1+=dec+(s1<<5)+3;
	*(data_in+i)=dec;
	crc=(crc>>0x0b)|(0x11111111+((crc^0x7FFL)<<0x15));
 }
 return;
}

/******************************************************************************
*
*	FUNCTION:	GetUnknowCrc(DWORD) - calculate crc for file without name
*
******************************************************************************/
DWORD GetUnknowCrc(DWORD entry)
{	
	DWORD	tmp,i,j,coded_dword,crc_file;
	DWORD	flag,size_pack,size_unpack,num_block,offset_body;
	DWORD	sign_riff1=0x46464952; // 'RIFF'
	DWORD	sign_riff3=0x45564157; // 'WAVE'
	DWORD	sign_mpq1=0x1a51504d; // 'MPQ'
	DWORD	sign_mpq2=0x00000020;
	ldiv_t	divres;

	offset_body=*(block_table+entry*4);								// get offset of analized file
	flag=*(block_table+entry*4+3);									// get flag of analized file
	fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
	fread(&coded_dword,sizeof(DWORD),1,fpMpq);						// read first coded dword from file

	if(flag&0x200 || flag&0x100) {								// IF FILE PACKED:
		size_unpack=*(block_table+entry*4+2);						// . get size of unpacked file
		size_pack=*(block_table+entry*4+1);							// . get size of packed file
		divres=ldiv(size_unpack-1,0x1000);
		num_block=divres.quot+2;									// . calculate lenght of file header
		for(j=0;j<=0xff;j++) {										// . now we're gonna find crc_file of 0x100 possible variants
			crc_file=((num_block*4)^coded_dword) - 0xeeeeeeee - *(massive_base+0x400+j);// . calculate possible crc
			if((crc_file&0xffL) == j) {								// . IF FIRST CHECK is succesfull - do second one
				fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
				fread(file_header,sizeof(DWORD),num_block,fpMpq);	// . read file header 
				Decode(file_header,massive_base,crc_file,num_block);// . decode file header with possible crc
				tmp=num_block*4;									// . tmp = size header (bytes)
				if(tmp == *file_header) {							// . IF SECOND CHECK is succesfull - do third one
					for(i=0;i<num_block-1;i++) {
	  					tmp+=*(file_header+i+1)-*(file_header+i);
						if(*(file_header+i+1)-*(file_header+i)>0x1000) {
							tmp=0xffffffff;
							break;
						}
					}
					if(tmp!=0xffffffff) {							// . IF THIRD CHECK is succesfull
						crc_file++;									// . great! we got right crc_file
						break;
					}
				}
			}
			crc_file=0;												// . if its impossible to get right crc return 0
		}

	} else {													// IF FILE IS NOT PACKED:
		for(j=0;j<=0xff;j++) {										// Calculate crc as if it was WAV FILE
			crc_file=(sign_riff1^coded_dword) - 0xeeeeeeee - *(massive_base+0x400+j);// . calculate possible crc
			if((crc_file&0xff)==j) {								// . IF FIRST CHECK is succesfull - do second one
				fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
				fread(file_header,sizeof(DWORD),3,fpMpq);			// . read file file_header 
				Decode(file_header,massive_base,crc_file,3);		// . decode file file_header with possible crc
				if(sign_riff1==*file_header) {
					if(sign_riff3==*(file_header+2))				// . IF SECOND CHECK is succesfull - we got right crc
						break;
				}
			}
			crc_file=0;												// . if its impossible to get right crc return 0
		}
		if(!crc_file) {												// Calculate crc as if it was MPQ FILE
			for(j=0;j<=0xff;j++) {
				crc_file=(sign_mpq1^coded_dword) - 0xeeeeeeee - *(massive_base+0x400+j);
				if((crc_file&0xffL) == j) {
					fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
					fread(file_header,sizeof(DWORD),2,fpMpq);
					Decode(file_header,massive_base,crc_file,2);
					if(sign_mpq1 == *file_header) {
						if(sign_mpq2 == *(file_header+1))
							break;
					}
				}
				crc_file=0;
			}
		}
	}
	return crc_file;
}

/******************************************************************************
*
*	FUNCTION:	ExtractTo(FILE,DWORD) - extract file from archive
*
******************************************************************************/
int ExtractTo(FILE *fp_new,DWORD entry)
{
	DWORD	size_pack,size_unpack;
	UInt8	*read_buffer,*write_buffer;
	UInt32	i,j,offset_body,flag,crc_file;
	UInt32	num_block,lenght_read,iteration;
	UInt8	*szNameFile;
	UInt8	metod;
	ldiv_t	divres;
	params	param;
 
	offset_body=*(block_table+entry*4);							// get offset of file in mpq
	size_unpack=*(block_table+entry*4+2);						// get unpacked size of file
	flag=*(block_table+entry*4+3);								// get flags for file

	if(flag&0x30000) {										// If file is coded, calculate its crc
		if(*(identify_table+entry)&0x1) {						// . Calculate crc_file for identified file:
			szNameFile=filename_table+_MAX_PATH*entry;			// . . get name of file
			if(strrchr(szNameFile,'\\'))
				szNameFile=strrchr(szNameFile,'\\')+1;
			crc_file=Crc(szNameFile,massive_base,0x300);		// . . calculate crc_file (for Diablo I MPQs)
			if(flag&0x20200)									// . . if flag indicates Starcraft MPQ
				crc_file=(crc_file+offset_body)^size_unpack;	// . . calculate crc_file (for Starcraft MPQs)
		}
		else
			crc_file=GetUnknowCrc(entry);						// . calculate crc_file for not identified file:
	}

	if(flag&0x200 || flag&0x100) {							// IF FILE IS PACKED:
		divres=ldiv(size_unpack-1,0x1000);
		num_block=divres.quot+2;								// . calculate lenght of file header
		fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
		fread(file_header,sizeof(DWORD),num_block,fpMpq);		// . read file header 
		if(flag&0x30000)
			Decode(file_header,massive_base,(crc_file-1),num_block);// . decode file header (if file is coded)
		read_buffer=read_buffer_start;
		for(j=0;j<(num_block-1);j++) {
			lenght_read=*(file_header+j+1)-*(file_header+j);	// . get lenght of block to read
			fread(read_buffer,sizeof(char),lenght_read,fpMpq);	// . read block
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,massive_base,crc_file,lenght_read/4);			// . decode block (if file is coded)
			if(lenght_read==0x1000 || (j==num_block-2 && lenght_read==(size_unpack&0xFFF)))	// . if block is unpacked (its lenght=0x1000 or its last block and lenght=remainder)
					fwrite(read_buffer,sizeof(char),lenght_read,fp_new);					// . write block "as is"
			else {												// . block is packed
				if(flag&0x200) {								// . If this file is from Starcraft MPQ (or Diablo 2), then
					metod=*read_buffer;							// . . first byte determinates metod of packing
					iteration=0;
					for(i=0;i<4;i++) {							// . . calculate number of iterations
						if(metod&avail_metods[i])
							iteration++;
					}
					read_buffer+=1;
					lenght_read-=1;
				} else {										// . Else: file is from Diablo I MPQ, then
					iteration=1;
					metod=8;									// . .file is compressed with DCL
				}
				write_buffer=write_buffer_start;
				if(metod&0x08) {
					param.buf_in =read_buffer;
					param.buf_out=write_buffer;
					lenght_write=0;
					explode(&read_data,&write_data,&param);
					lenght_read=lenght_write;
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=read_buffer_start;
					}
				}
				if(metod&0x01) {
					lenght_read=ExtWavUnp1((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
					iteration--;
					if(iteration) {
						read_buffer=write_buffer;
						write_buffer=read_buffer_start;
					}
				}
				if(metod&0x40)
					lenght_read=ExtWavUnp2((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
				if(metod&0x80)
					lenght_read=ExtWavUnp3((UInt32)read_buffer,(UInt32)lenght_read,(UInt32)write_buffer,0x1000);
				fwrite(write_buffer,1,lenght_read,fp_new);
				read_buffer=read_buffer_start;
			}
			crc_file++;											// . calculate crc_file for next block
		}
	}

	else {													// IF FILE IS NOT PACKED
		size_pack=*(block_table+entry*4+1);					// get size  of file
		if(flag&0x30000)
			lenght_read=0x1000;								// if file is coded, lenght_read=0x1000 (4 KB)
		else
			lenght_read=0x60000;							// if file is not coded, lenght_read=0x60000 (384KB)
		if(size_pack<lenght_read)
			lenght_read=size_pack;							// if size of file < lenght_read, lenght read = size of file
		read_buffer=read_buffer_start;
		if(lenght_read) {
			divres=ldiv(size_pack,lenght_read);					// .
			num_block=divres.quot;								// .
		} else {												// .
			num_block=0;										// .
			divres.rem=0;										// .
		}
		fseek(fpMpq,offset_mpq+offset_body,SEEK_SET);
		for (j=0;j<num_block;j++) {
			fread(read_buffer,1,lenght_read,fpMpq);
			if(flag&0x30000) {
				Decode((DWORD *)read_buffer,massive_base,crc_file,lenght_read/4);	// if file is coded, decode block
				crc_file++;										// and calculate crc_file for next block
			}
			fwrite(read_buffer,1,lenght_read,fp_new);
		}
		if(divres.rem) {
			fread(read_buffer,1,divres.rem,fpMpq);
			if(flag&0x30000)
				Decode((DWORD *)read_buffer,massive_base,crc_file,divres.rem/4);
			fwrite(read_buffer,1,divres.rem,fp_new);
		}
	}
	return 0;
} 
