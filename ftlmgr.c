#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "flash.h"
// 필요한 경우 헤더파일을 추가한다

FILE *flashfp;	// fdevicedriver.c에서 사용
int create_flashmemory (char argv[2], int num, char *blockbuf);
int dd_read(int ppn, char *pagebuf);
int dd_write(int ppn, char *pagebuf);
int dd_erase(int pbn);

//
// 이 함수는 FTL의 역할 중 일부분을 수행하는데 물리적인 저장장치 flash memory에 Flash device driver를 이용하여 데이터를
// 읽고 쓰거나 블록을 소거하는 일을 한다 (동영상 강의를 참조).
// flash memory에 데이터를 읽고 쓰거나 소거하기 위해서 fdevicedriver.c에서 제공하는 인터페이스를
// 호출하면 된다. 이때 해당되는 인터페이스를 호출할 때 연산의 단위를 정확히 사용해야 한다.
// 읽기와 쓰기는 페이지 단위이며 소거는 블록 단위이다.
// 
int main(int argc, char *argv[])
{	
	char sectorbuf[SECTOR_SIZE];
	char sparebuf[SPARE_SIZE];
	char pagebuf[PAGE_SIZE];
	char *blockbuf;
	char *searchingfreeblock;
	int blocknum, ppn, isbigger, fsize, bsize, psize, pbn;
	int flashsize;
	char *test;
	char ch;
	char d[4];
	int ppnofblk, blkofppn; //in place update 구현 위해 필요한 variables

	if(argc<3){
		fprintf(stderr,"명령인자 갯수가 부족합니다.");
		exit(1);
	}
	switch(argv[1][0]){
	// flash memory 파일 생성: 위에서 선언한 flashfp를 사용하여 flash 파일을 생성한다. 그 이유는 fdevicedriver.c에서 
	//                 flashfp 파일포인터를 extern으로 선언하여 사용하기 때문이다.
  	    case 'c' :
			blocknum=atoi(argv[3]);
			create_flashmemory(argv[2], blocknum, blockbuf);
			break;
	// 페이지 쓰기: pagebuf의 섹터와 스페어에 각각 입력된 데이터를 정확히 저장하고 난 후 해당 인터페이스를 호출한다
		case 'w' :
			flashfp=fopen(argv[2],"r+");
			ppn=atoi(argv[3]);
			fseek(flashfp, PAGE_SIZE*ppn, SEEK_SET); //해당 페이지에 데이터가 저장되어있는지 확인하기 위해 맨 앞글자를 읽음
			ch=fgetc(flashfp);
			if(ch!=(char)0xFF){ //만약 0xFF가 아닐경우 -> 데이터가 이미 저장되어 있을때
				
				ppnofblk =(ppn%4); //해당 페이지가 블락 안에서 몇 번째에 있는지 (0,1,2,3 중)
				blkofppn =(ppn/4); //해당 페이지가 포함된 블락이 몇 번째 블락인지

				fseek(flashfp, BLOCK_SIZE*blkofppn, SEEK_SET);
				fread(blockbuf, BLOCK_SIZE, 1, flashfp);
				memset(blockbuf+(PAGE_SIZE*ppnofblk), (char)0xFF, PAGE_SIZE);
				//블락 중 바꿔야 하는 페이지를 제외한 다른 페이지만 blockbuf에 저장

				fseek(flashfp, 0, SEEK_END);
				flashsize =ftell(flashfp);//플래시 메모리 사이즈
				fseek(flashfp, 0, SEEK_SET);
				bsize=(flashsize/BLOCK_SIZE); //블락의 수 
				psize=(flashsize/PAGE_SIZE); //페이지 수


			    for(int i=0; i<bsize; i++){//비어있는 freeblock을 찾아줌
					fseek(flashfp, BLOCK_SIZE*i, SEEK_SET);
					searchingfreeblock = (char*) malloc(sizeof(char)*BLOCK_SIZE);
					fread(searchingfreeblock, BLOCK_SIZE, 1, flashfp); //블락 저장
					for(int j=0; j<4; j++) //블락의 각 페이지 첫 글자를 따옴
					   d[j]=searchingfreeblock[j*PAGE_SIZE];
					

					if((d[0]==(char)0xFF)&&(d[1]==(char)0xFF)&&(d[2]==(char)0xFF)&&(d[3]==(char)0xFF)){ //만약 4개 모두 0xFF 일 때 -> FREEBLOCK일 때

						fseek(flashfp, BLOCK_SIZE*i, SEEK_SET);
						fwrite(blockbuf, BLOCK_SIZE, 1, flashfp); //freeblock에 기존 블락을 넣어줌

						fseek(flashfp, BLOCK_SIZE*i, SEEK_SET); 
						fread(blockbuf, BLOCK_SIZE, 1, flashfp); //freeblock에 넣어진 정보 blockbuf로 가져옴 
						dd_erase(i); //freeblock에 넣어준 정보 지움

						break;
					}

				}


				dd_erase(blkofppn); //넣고자 하던 페이지가 들어간 블락을 지움 (기존 블락을 지움)

				fseek(flashfp, BLOCK_SIZE*blkofppn, SEEK_SET);
				fwrite((void *)blockbuf, BLOCK_SIZE, 1, flashfp);//blockbuf에 들어가 있던 데이터를 다시 써줌 
			
			}
			
			memset(pagebuf, (char)0xFF, PAGE_SIZE); //그리고 쓰려고 했던 데이터를 다시 해당 페이지에 씀 
			memcpy(pagebuf, argv[4], strlen(argv[4]));
			memcpy(pagebuf+512, argv[5], strlen(argv[5]));
			dd_write(ppn, pagebuf);
			break;
	// 페이지 읽기: pagebuf를 인자로 사용하여 해당 인터페이스를 호출하여 페이지를 읽어 온 후 여기서 섹터 데이터와
	//                  스페어 데이터를 분리해 낸다
		case 'r' :
			flashfp=fopen(argv[2], "r");
			ppn=atoi(argv[3]);
			dd_read(ppn, pagebuf); //dd_read 함수를 통해 읽음
			memcpy(sectorbuf, pagebuf, SECTOR_SIZE); //sectorbuf에 pagebuf의 섹터 영역을 저장
			memcpy(sparebuf, pagebuf+512, SPARE_SIZE); //sparebuf에 pagebuf에 스페어 영역을 저장

			while(1){
				for(int i=0; i<SECTOR_SIZE; i++){
					if(sectorbuf[i]==(char)0xFF) //sectorbuf의 글자가 0xFF가 나오기 전까지
						break;
					printf("%c",sectorbuf[i]); //sectorbuf 출력
				}
				printf(" ");
				for(int i=0; i<SPARE_SIZE; i++){ //sparebuf의 글자가 0xFF가 나오기 전까지
					if(sparebuf[i]==(char)0xFF) 
						break;
					printf("%c",sparebuf[i]); //sparebuf 출력
				}
				printf("\n");
				break;
			}
			break;
		// 블락 지우기
		case 'e' :
			flashfp=fopen(argv[2],"r+");
			pbn=atoi(argv[3]);
			dd_erase(pbn);
			break;
		default :
			fprintf(stderr, "유효한 옵션이 아닙니다.");
	}
	return 0;
}

int create_flashmemory(char* filename, int blocknum, char* blockbuf ){

	//flashmemory를 만드는 함수

	flashfp=fopen(filename,"w+");

	blockbuf=malloc(BLOCK_SIZE);

	memset(blockbuf,(char)0xFF, BLOCK_SIZE);

	for(int i=0; i < blocknum; i++){
	fwrite(blockbuf, BLOCK_SIZE, 1, flashfp);
	}
}

