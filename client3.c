//chat_client.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#define BUF_SIZE 100
#define NAME_SIZE 30
#define MAX_ROOM 10

void *send_msg(void *arg);
void *recv_msg(void *arg);
void error_handling(char *msg);
void ctrlc_handling(int sig);

char name[NAME_SIZE]="[DEFAULT]";
char trueName[NAME_SIZE]="DEFAULT";
char msg[BUF_SIZE]={0};
int game_state = 0;
int game_room_client=0;
int civil=0;
int mafia=0;
int police=0;
int doctor=0;
int undertaker=0;
int dead=0;

pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
	int sock;
	struct sockaddr_in serv_addr;
	pthread_t snd_thread, rcv_thread;
	void *thread_return;
	struct sigaction actc;
	actc.sa_handler=ctrlc_handling;
	sigemptyset(&actc.sa_mask);
	actc.sa_flags=0;
	sigaction(SIGINT, &actc, 0);


	if(argc!=4){
		printf("Usage : %s <IP> <port> <name>\n", argv[0]);
		exit(1);
	}

	sprintf(name, "[%s]", argv[3]);
	sprintf(trueName,"%s",argv[3]);
	sock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_addr, 0, sizeof(serv_addr));
	serv_addr.sin_family=AF_INET;
	serv_addr.sin_addr.s_addr=inet_addr(argv[1]);
	serv_addr.sin_port=htons(atoi(argv[2]));

	if(connect(sock, (struct sockaddr*)&serv_addr, sizeof(serv_addr))==-1)
		error_handling("connnect() error!");
	pthread_mutex_lock(&mutx);
	write(sock, trueName, NAME_SIZE);
	putc('\n',stdin);
	pthread_mutex_unlock(&mutx);
	printf("Welcome! Press Enter... [TIP]/menu");
	pthread_create(&snd_thread, NULL, send_msg, (void *)&sock);
	pthread_create(&rcv_thread, NULL, recv_msg, (void *)&sock);
	pthread_join(snd_thread, &thread_return);
	pthread_join(rcv_thread, &thread_return);
	close(sock);
	return 0;
}

void *send_msg(void *arg) //send thread main
{
	int sock=*((int*)arg);
	int e;
	int i;
	int str_len;
	char tname[NAME_SIZE];
	char name_msg[NAME_SIZE+BUF_SIZE];
	char empty[BUF_SIZE]={""};
	char end_msg[BUF_SIZE]={"Someone is connect end!\n"};
	while(1)
	{
		if(dead!=0){
			printf("I'm not conversation!\n");
			sleep(30);
			continue;
		}
		fgets(msg, BUF_SIZE, stdin);
		if(!strcmp(msg, "q\n")||!strcmp(msg,"Q\n"))//q누르면 종료
		{
			sprintf(name_msg,"%s is connect end\n",name);
			write(sock,name_msg,strlen(name_msg));
			putc('\n',stdin);	//바로 종료하면 문제 생기는 것 같아서 처리 해둠
			sleep(3);			//바로 종료하면 문제 생기는 것 같아서 처리 해둠
			close(sock);
			exit(0);
		}
		else if(!strcmp(msg,"/menu\n"))//메뉴
		{
			printf("\n");
			printf("[MENU]\n");
			printf("1./menu -> Show the menu\n");
			printf("2./game -> Enter the game\n");
			printf("3./mafia -> Mafia game start\n");
			printf("4./vote -> Judgement Mafia!\n");
//			printf("5./kill -> Judgement Civilian!\n");
			printf("/Q or /q -> Close\n");
		}
		else if(!strcmp(msg,"/game\n")) //room변경
		{
			printf("ROOM(0~9): ");
			scanf("%d",&game_state);//게임 룸 입력
			sprintf(msg, "%s",msg);//sprintf필수사용 안전히복사
			write(sock, msg, strlen(name_msg)); //무엇을 하든지간에 우선 다른 것과 마찬가지로 write를 사용해서 동기화를 풀어주고 버퍼에 쌓인 걸 비움
			putc('\n',stdin);//write는 연속해서 못 사용하니까 \n집어넣어줌
			write(sock, &game_state,sizeof(int));//write만 쓰는 건 위험하지만 성공 현재 유일하게 서버에 직접적으로 입력해서 주고 받음
		}
		else if(!strcmp(msg,"/vote\n"))
		{
			write(sock, msg,strlen(name_msg));//먼저 명령보냄
			printf("The Mafia: "); //마피아 입력 요구
			scanf("%s",tname); //깨끗한 메세지를 입력
			write(sock,tname,NAME_SIZE); //메세지 보내기
	
		}
		else if(!strcmp(msg,"/kill\n"))
		{
			write(sock, msg, strlen(name_msg));
			printf("The target: ");
			scanf("%s",tname);
			write(sock,tname,NAME_SIZE);
		}
		else if(!strcmp(msg,"/mafia\n")) //마피아 게임 시작
		{
			printf("Mafia Game START!\n");
			sprintf(msg, "%s", msg);
			write(sock, msg, strlen(name_msg));
			putc('\n',stdin);
		}//else if("\n")공백 입력시 예외로 두면 오류생김
		else{
		putc('\n',stdin);
		sprintf(name_msg, "\r[%d번방]%s %s",game_state, name, msg); //몇 번 방에 있는지를 맨 앞에 넣어 줌 메세지 프로토콜을 정할 때도 사용가능
		write(sock, name_msg, strlen(name_msg)); //별 일 없으면 write
		//write는 한 번만 입력됨 다만 남아있는 건 저장되어서 다음에 날라가지만 putc('\n')로 조절
		}
	}
	return NULL;
}
void *recv_msg(void *arg) //read thread main 아직까지 recv_msg를 응용하지는 않음
{
	int sock=*((int*)arg);
	char name_msg[BUF_SIZE];
	char dead_msg[BUF_SIZE] = {"[You are DEAD!]\n"};//전달 받을 때는 write에서 글자 그대로 받음

	int str_len;
	while(1)
	{
		str_len=read(sock, name_msg, BUF_SIZE);
		if(str_len==-1)
			return (void *)-1;
		else if(!strcmp(name_msg,dead_msg)){
			printf("I'm dead!\n");
			write(sock,"\n",BUF_SIZE);
			++dead;
		}
		else{
			name_msg[str_len]=0;
			fputs(name_msg,stdout);
		}

	}
	return NULL;
}

void ctrlc_handling(int sig) //ctrl+c로 종료하는 것과 ctrl+z로 종료하는 것이 있는데 특히 ctrl+c로 종료하면 서버에서 처리를 하지 못함 ctrl+z는 처리가능
{
	if(sig==SIGINT){
		printf(" Press Q to quit!\n");
	}
}

void error_handling(char *msg)
{
	fputs(msg, stderr);
	fputc('\n', stderr);
	exit(1);
}





























































