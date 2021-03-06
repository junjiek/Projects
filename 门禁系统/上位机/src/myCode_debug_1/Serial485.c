
/**
与485串口通信。
上位机主动发出的请求：
		1.查询门的状态。（下位机有回复）
		2.设置开锁。（下位机有回复）
被动接收的请求（处理后可能要回复）
		1.下位机请求密码验证（上位机要回复）
		2.下位机传送指纹号（上位机要回复）
		3.下位机告知有验证失败的情况（上位机无回复）
		4.指纹验证通过，下位机告知上位机。（上位机要回复）
		5.下位机传来用户号和新密码，要求修改密码。（上位机回复修改结果）
*/

#include "myTty.h"
#include "Serial485.h"
#include "doParse.h"
#include "pthread.h"

#define BytesPerFrame  11  //一帧有11字节

#define DECODE_ERROR_HEAD_UNEXPECT 1			//b[0] != 0xBB
#define DECODE_ERROR_SECONDHEAD_UNEXPECT 2	//b[1] != 0xFF
#define DECODE_ERROR_LENGTH_ERROR 3				//b[2] != 0x07
#define DECODE_ERROR_NOT_HOST_TARGET 4			//b[3] != 0X03
#define DECODE_ERROR_TAIL_UNEXPECT 5			//b[10] != 0XFF

/*查询门的状态*/
void getStateOfDoor()
{

#ifdef DEBUG
printf("Host want to get the status of door.\nNow send command\n");
#endif

	char getStateCmd[11];
	
	getStateCmd[0]  = 0xAA;
	getStateCmd[1]  = 0xFF;
	getStateCmd[2]  = 0x07;
	getStateCmd[3]  = 0x03;
	getStateCmd[4]  = 0xA7;
	getStateCmd[5]  = 0x00;
	getStateCmd[6]  = 0x00;
	getStateCmd[7]  = 0x00;
	getStateCmd[8]  = 0x00;
	getStateCmd[9]  = 0x00;
	getStateCmd[10] = 0xFF;
	
	tty_write(getStateCmd,11);
}

/*将锁设为打开*/
int setLockOpen()
{
#ifdef DEBUG
printf("Host want to set the lock open.\n");
#endif

	char openLockCmd[11];
	
	openLockCmd[0]  = 0xAA;
	openLockCmd[1]  = 0xFF;
	openLockCmd[2]  = 0x07;
	openLockCmd[3]  = 0x03;
	openLockCmd[4]  = 0xB7;
	openLockCmd[5]  = 0x00;
	openLockCmd[6]  = 0x00;
	openLockCmd[7]  = 0x00;
	openLockCmd[8]  = 0x00;
	openLockCmd[9]  = 0x00;
	openLockCmd[10] = 0xFF;
	
	tty_write(openLockCmd,11);
}

/*
	解析这一帧
	协议见自己定义的协议文档《协议-final.doc》
*/

int parseFrame(unsigned char *buffer)
{
	unsigned char cmd,param;
	int userID,password;
	unsigned char FgNum;
	
#ifdef DEBUG
	int i=0;
	printf("+++Now Parsing Frame:\n");
	for(i=0;i<11;i++)
		printf("[%d]=0x%x",i,buffer[i]);
	printf("\n");
#endif

	if((unsigned char)buffer[0] != 0xBB)
		return DECODE_ERROR_HEAD_UNEXPECT;
	if((unsigned char)buffer[1] != 0XFF)
		return DECODE_ERROR_SECONDHEAD_UNEXPECT;
	if((unsigned char)buffer[2] != 0X07)
		return DECODE_ERROR_LENGTH_ERROR;
	if((unsigned char)buffer[3] != 0x00)//0x00表示是发给主控的。（主控的机器号为00）
		return DECODE_ERROR_NOT_HOST_TARGET;
	if((unsigned char)buffer[10] != 0xFF)
		return DECODE_ERROR_TAIL_UNEXPECT;
	
	//here the head is right & the tail is OK! --- get cmd 
	cmd = buffer[4];

#ifdef DEBUG
	printf("Parse cmd=0x%x\n",cmd);
#endif

	switch(cmd)
	{
	/*	返回门的状态
		BB FF 07 00 A7 00 00 00 00 00 FF(开)
	   BB FF 07 00 A7 01 00 00 00 00 FF(关)*/
	case 0xA7:
		if(buffer[5] == 0x00 )
		{
			showDoorStateOpened();//通知界面 门的状态是开着的。			
		}
		else if(buffer[5] == 0x01 )
		{
			showDoorStateClosed();//通知界面 门的状态是关着的。
		}
		else
		{
			showDoorStateUnknown();//通知界面 门的状态是未知的。（异常情况）
		}
		break;

	case 0xB7:
		/*	返回开锁的结果
			BB FF 07 00 B7 00 00 00 00 00 FF  (开锁成功)
			BB FF 07 00 B7 01 00 00 00 00 FF （开锁失败）*/
		if(buffer[5] == 0x00 )
		{
			showLockOpenSuccess();//通知界面开锁成功			
		}
		else if(buffer[5] == 0x01 )
		{
			showLockOpenFail();//通知界面开锁失败
		}
		else
		{
			showLockOpenUnknown();//通知界面开锁结果未知的。（异常情况）
		}
		break;

	case 0xCC:
		/*	密码验证请求：
			BB FF 07 00 CC   00 80    06 25 37  FF(5、6用户名，7、8、9密码)*/
		userID = MAKE_USER_ID(buffer[5] , buffer[6]);
		password = MAKE_USER_PASSWORD(buffer[7] , buffer[8] , buffer[9]);
		//RepalyPswdAuth(checkPassword(userID,password));
		RepalyPswdAuth(userID,password);
		break;

/**********************  ??????  ***********************************/
	/*
		指纹修改，传送指纹号：
		BB FF 07 00 CE 00 23 09 00 00 FF（5、6为用户名，7为指纹序号）
		返回：
		AA FF 07 03 CE 00 00 00 00 00 FF（5表示成功）
		AA FF 07 03 CE 01 00 00 00 00 FF（5表示失败）
	*/
	case 0xCE:
	
		userID = MAKE_USER_ID(buffer[5] , buffer[6] );
		FgNum = buffer[7];
		ModifyFgNum(userID,FgNum);
		break;
/*******************************************************************/
	/*	指纹验证失败多次，下位机将此事件告知上位机，让上位机处理。
		BB FF 07 00 CF 00 00 00 00 00 FF*/
	case 0xCF:
		AlertFGFail();
		break;

	/*	指纹验证通过，下位机告知上位机。（用于保存记录等）
		BB FF 07 00 DA 08 00 00 00 00 FF      ( 5表示指纹号)
		AA FF 07 03 DA 00 23 00 00 00 FF     （5、6 用户号） */
	case 0xDA:
		FgNum = buffer[5];
		ReplyFGAuthed(FgNum);//回应有指纹验证通过事件。
		break;
	
	/*	密码修改：
		BB FF 07 00 DB 00 23 12 34 56 FF（0023：用户号 ，  12 34 56：新密码）
		AA FF 07 03 DB 00 00 00 00 FF（成功）
		AA FF 07 03 DB 01 00 00 00 FF（失败）  */
	case 0xDB:
		userID = MAKE_USER_ID(buffer[5] , buffer[6] );
		password = MAKE_USER_PASSWORD(buffer[5] , buffer[6] , buffer[7]);
		ModifyPasswd(userID , password);
		break;
	default:
		#ifdef DEBUG
			printf("Unknown Command cmd = %d\n",cmd);
		#endif
	/*
		Do nothing .
		Alert to Qt.
	*/
	break;
	}

	return 0;

}


#ifdef DEBUG
int k=0,j=0,f=0;
#endif

/*
	监听485接口的数据
	所有数据，都从这里接收。
	应作为一个监听线程
*/
void* ListenSerial485(void* data)
{

	tty_init();

	unsigned char  frameBuffer[12];	
	unsigned char  tempBuffer[12];
	unsigned char  receiveFrame = 0;	//是否收到一帧
	unsigned char  count = 0;			//已收到当前帧的几个字节
	unsigned char  parseRes = -1;
	while(1){
		printf("Try reading:\n");
		while(tty_read( &frameBuffer[count] , 1 ) < 0)
		{
			#ifdef DEBUG
			
			#endif
		}

		#ifdef DEBUG
			printf("\nGet 0x%x from tty ,Now count=%d\n",frameBuffer[count],count+1);
		#endif

		count++;

		if(count >= BytesPerFrame)
		{
			#ifdef DEBUG
			printf("< Frame_%d >Receive A Frame!!\n",k++);
			#endif
			//已收到一帧。
			count = 0;
			receiveFrame = 1;
			parseRes = parseFrame(frameBuffer);
			printf("> > > > > parse return : %d\n",parseRes);
			//是否要睡眠一下,以等待解析完成。疑问。为防止缓冲区覆盖，可能要复制到另一缓冲区。
			receiveFrame = 0;
		}
	}
	
	tty_end();
	return NULL;

}




void* hostDoto485(void* data)
{
	int cmd;
	int inputCnt=0;
	int sid,pswd;

	while(1){

		printf("[Cmd]Please select one command:\n");
		printf("<1>Get the state of Door.\n");
		printf("<2>Unlock the lock.\n");	
		printf("<3>Add new user.\n");	
		printf("<4>Delete user.\n");	
		printf("<5>Show All users.\n");	
		printf("<6>Print Parse result table.\n");	
		
		scanf("%d",&cmd);

	#ifdef DEBUG
		printf("Your input cmd is %d\n",cmd);
	#endif

		switch(cmd)
		{	
			case 1:
				getStateOfDoor();	
			break;
			
			case 2:
				setLockOpen();
			break;
			
			case 3:
				scanf("%d",&sid);
				scanf("%d",&pswd);
				addUserInfo(sid,pswd);
			break;
	
			case 4:
				scanf("%d",&sid);
				deleteUser(sid);
			break;

			case 5:
				showAllUser();
			break;

			case 6:
				printf("Parse result table :\n");
				printf("  0 - parse OK !\n");
				printf("  1 - unexpected head(0xBB).\n");
				printf("  2 - unexpected second head(0xFF).\n");
				printf("  3 - length error(0x07 , Not fetal).\n");
				printf("  4 - not from target(0x03 , door expected).\n");
				printf("  5 - unexpected tail(not 0xFF).\n");
			break;
		}
	}
	return NULL;
}




int main(int argc,char** argv)
{

  	pthread_t listen485Thread, keyThread;
	void * retval;

#ifdef DEBUG
printf("Threads create ...\n");
#endif

 	pthread_create(&listen485Thread, NULL, ListenSerial485, 0);
  	pthread_create(&keyThread, NULL, hostDoto485, 0);
  
  	pthread_join(listen485Thread, &retval);
  	pthread_join(keyThread, &retval);

	return 0;
}







