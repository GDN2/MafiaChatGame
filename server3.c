//chat_server.c

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/wait.h>
#include <signal.h>
#include <time.h>
#define BUF_SIZE 100
#define NAME_SIZE 30
#define MAX_CLNT 256
#define MAX_ROOM 10
#define STAGE 5
#define DEAD 0
#define CIVIL 1
#define MAFIA 3
#define POLICE 4
#define DOCTOR 5
#define UNDERTAKER 6

void *handle_clnt(void *arg);
void send_msg(char *msg, int len);
void error_handling(char *msg);
void send_const_msg(const char *msg, int len);
void send_room_msg(char *msg, int len, int game_state); //해당 room에만 char 메세지를 전달한다.
void send_room_const_msg(const char *msg, int len, int game_state);// 해당 room에만 const char 메세지를 전달한다
//void stage(int sig); signal함수를 쓰면 서버가  클라이언트가  중복해서 접속한 걸로 생각하므로 안쓸예정

int clnt_cnt=0;
int clnt_socks[MAX_CLNT];
int game_states[MAX_CLNT]={0}; //해당 클라이언트의 room 번호라고 생각
int start_states[MAX_CLNT]={0}; //해당 클라이언트가 게임을 시작했는지 판단
int game_room[MAX_ROOM]={0}; //해당 room에 몇 명이 있는지를 셈
int room_starts[MAX_ROOM]={0}; //해당 room이 게임을 시작했는지 판단
char clnt_name[NAME_SIZE];
char clnt_names[MAX_CLNT][NAME_SIZE];
int stages[MAX_ROOM][STAGE]={0}; //game 진행 상황
int dead_states[MAX_CLNT]={0};
int civil_states[MAX_CLNT]={0};
int mafia_states[MAX_CLNT]={0};
int doctor_states[MAX_CLNT]={0};
int undertaker_states[MAX_CLNT]={0};
int dead_num[MAX_ROOM];
int mafia_num[MAX_ROOM];
int police_num[MAX_ROOM];
int doctor_num[MAX_ROOM];
int undertaker_num[MAX_ROOM];
int civil_num[MAX_ROOM];
int alive_clnt[MAX_ROOM]={0};
int just_onetime[MAX_ROOM][STAGE]={0};
int clnt_jobs[MAX_ROOM][MAX_CLNT]={0};
int jobs[MAX_CLNT]={0};
int private_jobs[MAX_CLNT]={0};
int jobindex[MAX_ROOM]={0};
int jobcount[MAX_ROOM]={0};
int conversation_count[MAX_ROOM]={0};
int votedpeople[MAX_CLNT]={0};
int votecount[MAX_ROOM]={0};
int just_onetime2[MAX_ROOM][STAGE]={0};
int vote_states[MAX_CLNT];
int killedpeople[MAX_CLNT];
int killcount[MAX_ROOM];
int kill_states[MAX_CLNT];

pthread_mutex_t mutx;

int main(int argc, char *argv[])
{
	int serv_sock, clnt_sock, i;
	struct sockaddr_in serv_adr, clnt_adr;
	int clnt_adr_sz;
	int state;
	pthread_t t_id;
	
	char buf[BUF_SIZE];
/*	struct sigaction act; //서버가 signal함수를 호출하면 클라이언트가 다시 접속한 걸로 되고 작동도 다시 안됨
	act.sa_handler=stage;
	sigemptyset(&act.sa_mask);
	act.sa_flags=0;
	sigaction(SIGALRM, &act, 0);*/

	if(argc!=2){
		printf("포트 번호를 입력해주세요 : %s <port>\n",argv[0]);
		exit(1);
	}

	pthread_mutex_init(&mutx, NULL);//mutx의 주소를 받고 mutex생성
	serv_sock=socket(PF_INET, SOCK_STREAM, 0);

	memset(&serv_adr, 0, sizeof(serv_adr));
	serv_adr.sin_family=AF_INET;
	serv_adr.sin_addr.s_addr=htonl(INADDR_ANY);
	serv_adr.sin_port=htons(atoi(argv[1]));

	if(bind(serv_sock, (struct sockaddr*) &serv_adr, sizeof(serv_adr))==-1)
		error_handling("bind() error!");
	if(listen(serv_sock, 5)==-1)
		error_handling("listen() error!");


	while(1)
	{
		clnt_adr_sz=sizeof(clnt_adr);
		clnt_sock=accept(serv_sock, (struct sockaddr*)&clnt_adr, &clnt_adr_sz);

		pthread_mutex_lock(&mutx); //client소켓 생성시 자물쇠 적용
		
		clnt_socks[clnt_cnt]=clnt_sock;
		read(clnt_sock,clnt_name,NAME_SIZE);
		game_states[clnt_cnt]=0;
		sprintf(clnt_names[clnt_cnt++],"%s",clnt_name);
		for(i=0;i<clnt_cnt;i++){
			printf("%s\n",clnt_names[i]);
		}
		pthread_mutex_unlock(&mutx);

		pthread_create(&t_id, NULL, handle_clnt, (void*)&clnt_sock);//t_id의 주소를 받아서 기본적인 형태의 hadle_clnt라는 메인함수를 갖는 thread생성하며 클라이언트의 주소값을 인자로 받고 있다.
		pthread_detach(t_id);//thread타입의 값을 받아서 제거하고 있다.
		printf("Connected client IP: %s \n", inet_ntoa(clnt_adr.sin_addr));//network에서 주소정보를 받아오고 있으니 ntoa이며 clnt_adr의 주소는 clnt_adr.sin_addd이다.
	}
	close(serv_sock);
	return 0;
}

void *handle_clnt(void *arg) // thread의 main 함수 변수로 accept한 클라이언트의  주소값을 받아오고처리한다
{
	int clnt_sock=*((int*)arg);
	int str_len=0, i,j,k,n;
	int empty;
	int temp=0;
	int set=0;
	int noclient=0;
	int just[STAGE]={0};
	int *ptemp=NULL;
	char msg[BUF_SIZE];
	char tmsg[BUF_SIZE];//client에게 보내는 메세지
	char cmsg[BUF_SIZE];
	char tname[NAME_SIZE];
	int room_change = 0;
	int after_msg = 0;
	int clnt_stage[STAGE]={0};
	int clnt_job=-1;

	const char game_msg[BUF_SIZE]={"/game\n"}; //사용할 const msg저장 이렇게 저장안하고 사용하면 서버에서 strcmp함수가 작동 안함
	const char mafia_msg[BUF_SIZE]={"/mafia\n"};
	const char vote_msg[BUF_SIZE]={"/vote\n"};//vote뒤에 _msg안 붙이면 비교가 안됨 그리고 !strcmp도 지금 상태에서 어디서든 작동을 안함 게임 진입부터  strstr사용
	const char const_msg[BUF_SIZE]={"[Message from Server]\n"};
	const char mafia_fail_msg[BUF_SIZE]={"[Should more than 3!]\n"};
	const char mafia_start_msg[BUF_SIZE]={"[mafia game start!]\n"};
	const char mafia_game_started[BUF_SIZE]={"[Not enter the room. Mafia game aleady is started!]\n"};
	const char zeroroom[BUF_SIZE]={"[Not Started Mafia game room 0!]\n"};
	const char mafiaWIN[BUF_SIZE]={"[MAFIA WIN!!!]\n"};
	const char civilWIN[BUF_SIZE]={"[CIVIL WIN!!!]\n"};
	const char pressEnter[BUF_SIZE]={"[Press Enter...]\n"};
	const char noset[BUF_SIZE]={"[No other clients is set!]\n"};
	const char clnt_set[BUF_SIZE]={"[All client is set! Next Stage!]\n"};
	const char private_msg[BUF_SIZE]={"[Private message!]\n"};
	const char civil_msg[BUF_SIZE]={"<<<[You are Civilian!]>>>\n"};
	const char mafia_msg2[BUF_SIZE]={"<<<[You are Mafia!]>>>\n"};
	const char police_msg[BUF_SIZE]={"<<<[You are Police!]>>>\n"};
	const char doctor_msg[BUF_SIZE]={"<<<[You are Doctor!]>>>\n"};
	const char undertaker_msg[BUF_SIZE]={"<<<[You are undertaker!]>>>\n"};
	const char conversation_msg[BUF_SIZE]={"[Start! Conversation for judgement!]\n"};
	const char lessconversation[BUF_SIZE]={"Should be more than total 10 conversation!\n"};
	const char votepeople_msg[BUF_SIZE]={"Write the mafia\n"};
	const char noclient_msg[BUF_SIZE]={"[NO client]\n"};
	const char OK_msg[BUF_SIZE]={"[OK]\n"};
	const char justonetime_msg[BUF_SIZE]={"[Just one time!]\n"};
	const char dead_msg[BUF_SIZE]={"[You are DEAD!]\n"};
	const char isdead_msg[BUF_SIZE]={" is dead !\n"};
	const char alreadydead_msg[BUF_SIZE]={"[The people already dead!]\n"};
	const char kill_msg[BUF_SIZE]={"/kill\n"};
	const char notproper_msg[BUF_SIZE]={"[You are not proper person for this!]\n"};
	while((str_len=read(clnt_sock, msg, sizeof(msg)))!=0){
		pthread_mutex_lock(&mutx); //시작하자마자 소켓 위치 찾아두기
		for(j=0;j<clnt_cnt;j++){
			if(clnt_sock==clnt_socks[j]){
				break;
			}
		}
		pthread_mutex_unlock(&mutx);
		if(after_msg==1){ //mafia게임이 실행중인 방으로 들어가려고 하면 메세지를 띄워주는데 이렇게 따로 분리해두지 않으면 바로 메세지가 발송되지 않음
			after_msg=0;
			printf("Message: j=[%d] ",j); //클라이언트 소켓을 표현하고 있음 여기서도 printf로 함수를 감싸주어야 동기화에서 풀림
			send_room_const_msg(mafia_game_started,strlen(mafia_game_started),game_states[j]);
			for(i=0; i<clnt_cnt; i++){
				printf(" %d ", game_states[i]); //클라이언트 room번호
				printf(" [%d] ",clnt_socks[i]); //클라이언트 소켓 번호
			}
			printf("~message\n"); //동기화에서 풀리게 하기 위함
		}
		if(!strcmp(msg,game_msg)){ //const char형으로 만들어서 비교하는건  필수
			pthread_mutex_lock(&mutx);
			printf("[client game connected]");
			pthread_mutex_unlock(&mutx);
			if(room_change>=1){ //방을 바꾼 적이 있으면 해당 방 인원수 감소
				--game_room[game_states[j]];
			}
			read(clnt_sock, &game_states[j],sizeof(int));//이렇게 &배열을 사용해서 해당 배열의 주소에 정확히 집어 넣는 것이 가능하다.
														//client로부터 room번호를 받아서 room 위치 배열에 해당 클라이언트 위치(j)에 저장
			if(room_starts[game_states[j]]==1){ //마피아 게임이 실행중이면 튕겨 나감
				if(room_change>=1){
					room_change=0;	
				}
				game_states[j]=-1; //마피아 게임방 접속 실패 -1
				++after_msg;//가끔씩 안 먹힐 때가 있을 때에는 이렇게 해줌
				puts("인원 초과");
				putc('\n',stdin);//동기화를 깨뜨림
				pthread_mutex_unlock(&mutx);
				continue;
			}
			for(i=0;i<MAX_ROOM;i++){
				printf(" [%d] ",room_starts[i]);
			}
			++game_room[game_states[j]]; //게임방 인원수 증가
			++room_change; //방을 바꾸면 체크 해둠
			for(i=0;i<MAX_ROOM;i++){
				printf(" %d ",game_room[i]); //게임 room 인원 수 print
			}
			for(i=0;i<clnt_cnt;i++){
				printf(" {%d} ", game_states[i]); //클라이언트가 몇 번 room에 있는지 확인
			}
			printf("\t%d번방 [%d]client, client_cnt: %d\n",game_states[j],clnt_sock,clnt_cnt); //server에서 문장 printf 두개 만들면 끊어져서 출력되며 \n이 있는 곳을 문장의 마지막으로 본다. 두 번째 줄에 \n이 있을 경우에 첫번째 줄과 두번 째 줄이 출력되며 그 이후에 두 번 입력했을 시에 출력 안된 세번 째 줄부터 첫번 째 두번째 줄 순으로 출력 요약하자면 \n은 하나만 있어야 하고 마지막 줄에 있어야 할 것
//printf("/n")의 효과로 동기화에서 풀어버린다 즉 printf로 앞 뒤로 감싸야 동기화가 풀린다.
			set=1;	
			pthread_mutex_unlock(&mutx);
		}
		else if(!strcmp(msg,mafia_msg))
		{
			printf("[client want to play Mafia] ");
			//j두번 찾지 말기
			sprintf(msg,"[%d번방]",game_states[j]);
			sprintf(tmsg,"[%d명]",game_room[game_states[j]]);
			if(game_states[j]==0){
				send_room_const_msg(zeroroom,strlen(zeroroom),game_states[j]);
			}
			else if(game_room[game_states[j]]<3){ //인원 수가 3명 미만이면 안됨
				printf("현재 인원 수 : %d ",game_room[game_states[j]]);
				send_room_msg(msg,strlen(msg),game_states[j]);
				send_room_msg(tmsg,strlen(tmsg),game_states[j]); //tmsg는 배열하나 더 만들려고 했던 것 sprintf사용 하면 숫자가 문자열로 바뀜
				send_room_const_msg(mafia_fail_msg,strlen(mafia_fail_msg),game_states[j]);
			}
			else{
				printf("현재 인원 수 : %d ", game_room[game_states[j]]); //게임 시작 시 서버에 인원 수 표현
				start_states[j]=game_states[j];
				for(k=0;k<clnt_cnt;k++)
				{
					if(game_states[k]==game_states[j]){
						start_states[k]=start_states[j]; //해당 방의 모든 클라이언트의 게임 실행 상태를 0에서 동일하게 방 번호로 바꿈
					}
				}
				room_starts[game_states[j]]=1; //room을 게임 시작 상태로 바꿈
				send_room_msg(msg,strlen(msg),game_states[j]); //게임 시작 메세지 출력
				send_room_msg(tmsg,strlen(tmsg),game_states[j]);
				send_room_const_msg(mafia_start_msg,strlen(mafia_start_msg),game_states[j]);
			}
			send_room_const_msg(const_msg,strlen(const_msg),game_states[j]); //const msg가 잘 가는지 실험
			printf("Mafia game set\n"); //마지막에 /n문자로 동기화 풀어줌
		}
		else if (room_starts[game_states[j]]!=1){
			send_room_msg(msg, str_len,game_states[j]); //아무 것도 아닐 경우 해당 방에만 메세지 출력
			continue;
		}

		else if(room_starts[game_states[j]]==1){
			printf("message:");
//			pthread_mutex_lock(&mutx);//인원수 세팅 마피아 3명당 1명 경찰 4명당 1명 의사 5명당 1명 장의사 6명당 한명
			if(clnt_stage[0]==0)
			{

			mafia_num[game_states[j]]=game_room[game_states[j]]/MAFIA;
			police_num[game_states[j]]=game_room[game_states[j]]/POLICE;
			doctor_num[game_states[j]]=game_room[game_states[j]]/DOCTOR;
			undertaker_num[game_states[j]]=game_room[game_states[j]]/UNDERTAKER;
			civil_num[game_states[j]]=game_room[game_states[j]]-mafia_num[game_states[j]]-police_num[game_states[j]]-doctor_num[game_states[j]]-undertaker_num[game_states[j]];
			alive_clnt[game_states[j]]=game_room[game_states[j]];
			++stages[game_states[j]][0];
			clnt_stage[0]=1;
			write(clnt_socks[j],pressEnter,strlen(pressEnter));
			}
			else if(stages[game_states[j]][0]!=game_room[game_states[j]]){
			send_room_const_msg(noset,strlen(noset),game_states[j]);
			}
			else if(just_onetime[game_states[j]][0]!=1)
			{
			
			sprintf(cmsg,"civil: %d ,mafia: %d,police: %d, doctor: %d, undertaker: %d",civil_num[game_states[j]],mafia_num[game_states[j]],police_num[game_states[j]],doctor_num[game_states[j]],undertaker_num[game_states[j]]);
			send_room_const_msg(cmsg,strlen(cmsg),game_states[j]);
			send_room_const_msg(clnt_set,strlen(clnt_set),game_states[j]);
				printf("\t...Game Stages[0]=%d clear!",stages[game_states[j]][0]);
			for(i=0;i<STAGE;i++){
				printf("<%d>",stages[game_states[j]][i]);
			}
				printf("%s",cmsg);
				just_onetime[game_states[j]][0]=1;
			}
			else{//end of stage 0
//stage 1	
			if(clnt_stage[1]!=1&&stages[game_states[j]][0]==game_room[game_states[j]]){
			if(just_onetime[game_states[j]][1]==0){
			++just_onetime[game_states[j]][1];
			for(i=0;i<civil_num[game_states[j]];i++){
				clnt_jobs[game_states[j]][jobindex[game_states[j]]++]=1;
			}
			for(i=0;i<mafia_num[game_states[j]];i++){
				clnt_jobs[game_states[j]][jobindex[game_states[j]]++]=3;
			}
			for(i=0;i<police_num[game_states[j]];i++){
				clnt_jobs[game_states[j]][jobindex[game_states[j]]++]=4;
			}
			for(i=0;i<doctor_num[game_states[j]];i++){
				clnt_jobs[game_states[j]][jobindex[game_states[j]]++]=5;
			}
			for(i=0;i<undertaker_num[game_states[j]];i++){
				clnt_jobs[game_states[j]][jobindex[game_states[j]]++]=6;
			}	
			}else{}
			++clnt_stage[1];
			++stages[game_states[j]][1];
			write(clnt_socks[j],pressEnter,strlen(pressEnter));
			}
			else if(stages[game_states[j]][1]!=game_room[game_states[j]]){
				send_room_const_msg(noset,strlen(noset),game_states[j]);
			}
			else if(just[1]!=1){
                i=(unsigned)time(NULL)%(game_room[game_states[j]]-jobcount[game_states[j]]);
                if(i==game_room[game_states[j]]-jobcount[game_states[j]]-1){
                    private_jobs[j]=clnt_jobs[game_states[j]][i];
					clnt_job=private_jobs[j];
                ++jobcount[game_states[j]];  
				}
                else{
                        private_jobs[j]=clnt_jobs[game_states[j]][i];
                        ptemp=&clnt_jobs[game_states[j]][i];
                        clnt_jobs[game_states[j]][i]=clnt_jobs[game_states[j]][game_room[game_states[j]]-jobcount[game_states[j]]-1];
                        clnt_jobs[game_states[j]][game_room[game_states[j]]-jobcount[game_states[j]]-1]=*ptemp;
  	                    ++jobcount[game_states[j]];
       			        send_room_const_msg(pressEnter,strlen(pressEnter),game_states[j]);
				}
				sprintf(tmsg,"client:%s 직업{%d}",clnt_names[j], private_jobs[j]);
				if(private_jobs[j]==1){
					write(clnt_socks[j],civil_msg,BUF_SIZE);
				}
				else if(private_jobs[j]==3){
					write(clnt_socks[j],mafia_msg2,BUF_SIZE);
				}
				else if(private_jobs[j]==4){
					write(clnt_socks[j],police_msg,BUF_SIZE);
				}
				else if(private_jobs[j]==5){
					write(clnt_socks[j],doctor_msg,BUF_SIZE);
				}
				else if(private_jobs[j]==6){
					write(clnt_socks[j],undertaker_msg,BUF_SIZE);
				}
				else{
					write(clnt_socks[j],"I don't Know my job!\n",BUF_SIZE);				   }
				printf("%s",tmsg);
				for(i=0;i<STAGE;i++){
					printf(" <%d> ",stages[game_states[j]][i]);
				}
				just[1]=1;
			}
			else{ //stage 2
			if(clnt_stage[2]==0&&stages[game_states[j]][1]==game_room[game_states[j]]){
				write(clnt_socks[j],pressEnter,BUF_SIZE);
				++clnt_stage[2];
				++stages[game_states[j]][2];
			}
			else if(just_onetime[game_states[j]][2]!=1){
				++just_onetime[game_states[j]][2];
				send_room_const_msg(conversation_msg,BUF_SIZE,game_states[j]);
				for(i=0;i<STAGE;i++){
					printf(" <%d> ",stages[game_states[j]][i]);
				}
				just_onetime2[game_states[j]][2]=0;
			}
			else if((strstr(msg,vote_msg))&&conversation_count[game_states[j]]<10){
				read(clnt_socks[j],tmsg,BUF_SIZE);//읽어 주긴 함	
				send_room_const_msg(lessconversation,BUF_SIZE,game_states[j]);
			}
			else if((strstr(msg,vote_msg))&&conversation_count[game_states[j]]>=10){
				pthread_mutex_lock(&mutx);
				read(clnt_socks[j],tmsg,BUF_SIZE);//바로 메세지 읽어들일 준비
				for(k=0 ; k < clnt_cnt ; k++){
					printf("%s",tmsg);//출력 확인 용도
					if(!(strcmp(tmsg,clnt_names[k]))){
						noclient=0;			
						n=k;
						k=clnt_cnt; //break사용하면 안 좋은 것 같아서 k값 수정
					}
					else if(k == clnt_cnt - 1){
						noclient=1;
					}
				}
				if(noclient==1){
					if(vote_states[j]==1){
					write(clnt_socks[j],justonetime_msg,BUF_SIZE);
					pthread_mutex_unlock(&mutx);
					continue;
					}
					write(clnt_sock,noclient_msg,BUF_SIZE);
					pthread_mutex_unlock(&mutx);
				}
				else if(noclient==0){
					if(vote_states[j]==1){
					write(clnt_socks[j],justonetime_msg,BUF_SIZE);
					pthread_mutex_unlock(&mutx);
					continue;
					}
					if(dead_states[n]==1){
					write(clnt_socks[j],alreadydead_msg,BUF_SIZE);
					pthread_mutex_unlock(&mutx);
					continue;
					}
					write(clnt_socks[j],OK_msg,BUF_SIZE);
					++votedpeople[n];
					++votecount[game_states[j]];
					vote_states[j]=1;
				}
				pthread_mutex_unlock(&mutx);
			}
            else if((strstr(msg,kill_msg))&&conversation_count[game_states[j]]<10){
                read(clnt_socks[j],tmsg,BUF_SIZE);//읽어 주긴 함    
                send_room_const_msg(lessconversation,BUF_SIZE,game_states[j]);
            }
            else if((strstr(msg,kill_msg))&&conversation_count[game_states[j]]>=10){
                pthread_mutex_lock(&mutx);
                read(clnt_socks[j],tmsg,BUF_SIZE);//바로 메세지 읽어들일 준비
				if(private_jobs[j]!=3){
				write(clnt_socks[j],notproper_msg,BUF_SIZE);
				pthread_mutex_unlock(&mutx);
					continue;
				}
                for(k=0 ; k < clnt_cnt ; k++){
                    printf("%s",tmsg);//출력 확인 용도
                    if(!(strcmp(tmsg,clnt_names[k]))){
                        noclient=0;
                        n=k;
                        k=clnt_cnt; //break사용하면 안 좋은 것 같아서 k값 수정
                    }
                    else if(k == clnt_cnt - 1){
                        noclient=1;
                    }
                }
                if(noclient==1){
                    if(kill_states[j]==1){
                    write(clnt_socks[j],justonetime_msg,BUF_SIZE);
                    pthread_mutex_unlock(&mutx);
                    continue;
                    }
                    write(clnt_sock,noclient_msg,BUF_SIZE);
                    pthread_mutex_unlock(&mutx);
                }
                else if(noclient==0){
                    if(kill_states[j]==1){
                    write(clnt_socks[j],justonetime_msg,BUF_SIZE);
                    pthread_mutex_unlock(&mutx);
                    continue;
                    }
                    if(dead_states[n]==1){
                    write(clnt_socks[j],alreadydead_msg,BUF_SIZE);
                    pthread_mutex_unlock(&mutx);
                    continue;
                    }
                    write(clnt_socks[j],OK_msg,BUF_SIZE);
                    ++killedpeople[n];
                    ++killcount[game_states[j]];
                    kill_states[j]=1;
                }
                pthread_mutex_unlock(&mutx);
            }

			else{
				++conversation_count[game_states[j]];
			if(votecount[game_states[j]]!=alive_clnt[game_states[j]]){
				send_room_msg(msg, str_len,game_states[j]);
			}
			else if(votecount[game_states[j]]==alive_clnt[game_states[j]]){
				for(i=0; i<clnt_cnt; i++){
					if(game_states[i]=game_states[j]){
						vote_states[i]=0;
					}
				}
				if(just_onetime2[game_states[j]][2]==0){	
				printf("투표 완료\n");
				for(k=0;k<clnt_cnt;k++){
					if(game_states[k]==game_states[k]){
						if(temp<votedpeople[k]){
							temp=votedpeople[k];
							n=k;
							votedpeople[k]=0;
						}
						else{
							votedpeople[k]=0;
						}
					}
				}
				write(clnt_socks[n],"[You are DEAD!]\n",BUF_SIZE);//전달시는 글자 그대로
				putc('\n',stdin);
				sprintf(cmsg,"%s",clnt_names[n]);
				send_room_const_msg(cmsg,BUF_SIZE,game_states[j]);
				send_room_const_msg(isdead_msg,strlen(isdead_msg),game_states[j]);
				--alive_clnt[game_states[j]];
				just_onetime[game_states[j]][2]=0;
				votecount[game_states[j]]=0;
				++dead_num[game_states[j]];
				++dead_states[n];
				++just_onetime2[game_states[j]][2];
				conversation_count[game_states[j]]=0;

				if(private_jobs[n]==1){
					--civil_num[game_states[j]];
					printf("--clinte num");
				}
				else if(private_jobs[n]==3){
					--mafia_num[game_states[j]];
					printf("--mafia num");
				}
				else if(private_jobs[n]==4){
					--police_num[game_states[j]];
					printf("--police num");
				}
				else if(private_jobs[n]==5){
					--doctor_num[game_states[j]];
					printf("--doctor num");
				}
				else if(private_jobs[n]==6){
					--undertaker_num[game_states[j]];
					printf("--undertaker num");
				}
            if(mafia_num[game_states[j]]==0){//mafia game종료 조건
                send_room_const_msg(civilWIN,strlen(civilWIN),game_states[j]);
            }
            else if(mafia_num[game_states[j]]>=police_num[game_states[j]]+doctor_num[game_states[j]]+undertaker_num[game_states[j]]+civil_num[game_states[j]]){
                send_room_const_msg(mafiaWIN,strlen(mafiaWIN),game_states[j]);
            }
            else{
                printf("...Continue MAFIA GAME! ");
            }
		
			}//죽을 클라이언트 처리
				send_room_msg(msg,str_len,game_states[j]);
			}
			}//end of stage2 투표 진행
			}//end of stage1 직업 랜덤 배정
			}//end of stage0 직업 수 설정
			
			printf("~message\n");
		};
	}//end of while
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++) //remove disconnected client
	{	
		if(clnt_sock==clnt_socks[i])
		{
			if((game_states[i]!=-1&&room_change!=0)){
				--game_room[game_states[i]];
				room_starts[game_states[i]]=0;
			}
			while(i<clnt_cnt-1){
				clnt_socks[i]=clnt_socks[i+1];
				game_states[i]=game_states[i+1];
				start_states[i]=start_states[i+1];
				strcpy(clnt_names[i],clnt_names[i+1]);
				++i;//이렇게 바꿈으로써 이제 정상으로 되었음 소켓 먼저 나가도 됨
			}
			break;
		}
	}
	clnt_cnt--;

	pthread_mutex_unlock(&mutx);
	close(clnt_sock);
	return NULL;
}
void send_msg(char *msg, int len) //send to all
{
	int i;
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++){
		write(clnt_socks[i], msg, len);
	}
	pthread_mutex_unlock(&mutx);
	
}

void send_const_msg(const char *msg,int len) //함수로 호출하면 어째서인지 단독으로 보내지 못하고 처음 접속한 클라이언트에게만 가게된다.
{
	int j;
	pthread_mutex_lock(&mutx);
	for(j=0; j<clnt_cnt; j++){
		write(clnt_socks[j],msg,len);
	}
	pthread_mutex_unlock(&mutx);

}

void send_room_msg(char *msg,int len, int game_state)
{
	int i;
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++){
		if(game_states[i]==game_state){
			write(clnt_socks[i],msg,len);
		}
	}
	pthread_mutex_unlock(&mutx);
}

void send_room_const_msg(const char *msg, int len, int game_state)
{
	int i;
	pthread_mutex_lock(&mutx);
	for(i=0; i<clnt_cnt; i++){
		if(game_states[i]==game_state){
			write(clnt_socks[i],msg,len);
		}
	}
	pthread_mutex_unlock(&mutx);
}

void error_handling(char *msg)
{
	fputs(msg,stderr);
	fputc('\n',stderr);
	exit(1);
}

/*void stage(int sig)
{
	if(sig==SIGALRM)
	{
		puts("3초 경과");
	}
	alarm(3);
}*/

//현재 버그 없음

























































