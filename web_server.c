#include "web_server.h"
#define MAX_REQUEST_SIZE 70000
#define MAX_RESPONSE_BUF 1024

//get the MIME type of files for Content-Type
const char * get_MIME_type(const char* path) {
    const char *last_dot = strrchr(path, '.');		//搜尋最後出現的char，找不到返回NULL
	
	//已查看副檔名的方式來決定(僅支援部分型態)	
    if (last_dot) {
        if (strcmp(last_dot, ".html") == 0) return "text/html";
        if (strcmp(last_dot, ".css") == 0) return "text/css";
		if (strcmp(last_dot, ".js") == 0) return "application/javascript";
		if (strcmp(last_dot, ".ico") == 0) return "image/x-icon";
        if (strcmp(last_dot, ".jpeg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".jpg") == 0) return "image/jpeg";
        if (strcmp(last_dot, ".png") == 0) return "image/png";
        if (strcmp(last_dot, ".gif") == 0) return "image/gif";
        if (strcmp(last_dot, ".txt") == 0) return "text/plain";
    }

    return "application/octet-stream";
}

int create_socket(const char* host, const char *port) {
    printf("Configuring local address...\n");
    struct addrinfo hints;	
    memset(&hints, 0, sizeof(hints));	//將hints初始化為0
    hints.ai_family = AF_INET;			//AF_INET表示我們用IPv4 address
	hints.ai_socktype = SOCK_STREAM;	//SOCK_STREAM表示我們用TCP
    hints.ai_flags = AI_PASSIVE;		//設定這個會告訴getaddrinfo，我們要bind wildcard address

    struct addrinfo *bind_address;
    getaddrinfo(host, port, &hints, &bind_address);		//我們用getaddrinfo來設定addrinfo。host可填網址(域名)或IP，在這裡我們沒有要指定網卡，所以填0(NULL)
														//port是我們要從哪個port監聽連線，可填協定(如htpp)或port號，hints用來指定一些基本項目(見上)
														//bind_address會指向設定好的結果，總之這個addrinfo是提供給bind()的資訊
    printf("Creating socket...\n");
    int socketFd;								
    socketFd = socket(bind_address->ai_family,					//bind_address的內容是getaddrinfo()根據我們提供的資訊，自行設定的。
            bind_address->ai_socktype, bind_address->ai_protocol); 	//另外，雖然這邊是用bind_address的內部資料，但其實它和bind_address本身沒關係
																	//它就是要取那個資料而已，直接填AF_INET也是一樣的
    
	if (socketFd<0) {									
        fprintf(stderr, "socket() failed. (%d)\n", errno);		
        exit(1);
    }

    printf("Binding socket to local address...\n");
    if (bind(socketFd,
                bind_address->ai_addr, bind_address->ai_addrlen)) {		//bind成功回傳0，否則回傳非0
        fprintf(stderr, "bind() failed. (%d)\n", errno);
        exit(1);
    }
	
    freeaddrinfo(bind_address);		//綁好後就可以將addrinfo釋放記憶體了

    printf("Listening...\n");
    if (listen(socketFd, 10) < 0) {		//這裡的10是指最多能queue住10個connections，再多的就拒絕掉
        fprintf(stderr, "listen() failed. (%d)\n", errno);
        exit(1);
    }

    return socketFd;
}

//紀錄每個client的資訊
struct client_info {
    socklen_t addr_len;
    struct sockaddr_storage addr;			//sockaddr_storage結構，保證夠大的空間，來存各種socket address(不只是IPv4)
    int socketFd;
    char request[MAX_REQUEST_SIZE + 1];         //用來存client端送到server端的HTTP request
    int received;								//request中已存多少bytes
    FILE *f;									//用來上傳圖片
	char addr_IP[128];
};

//當server不懂client的request，即回傳400 erorr
void send_400(struct client_info *client) {
    const char *error400 = "HTTP/1.1 400 Bad Request\r\n"
        "Connection: close\r\n"
        "Content-Length: 11\r\n\r\nBad Request";	//連同body一起送
    send(client->socketFd, error400, strlen(error400), 0);
    close(client->socketFd);
}

//當server找不到clients所求的資料，即回傳404 eorro
void send_404(struct client_info *client) {
    const char *error404 = "HTTP/1.1 404 Not Found\r\n"
        "Connection: close\r\n"
        "Content-Length: 9\r\n\r\nNot Found";		//連同body一起送
    send(client->socketFd, error404, strlen(error404), 0);
    close(client->socketFd);
}

//得到client的IP address(字串形式)
void get_client_address(struct client_info *client) {
    getnameinfo((struct sockaddr*) &(client->addr),			//輸入已知的client address和address len
            client->addr_len,								//會將socket address轉成對應的host名稱(我想這應該和big or little endian有關
            client->addr_IP, sizeof(client->addr_IP), 0, 0,	//hostname會存在address_buffer，因為我們不在乎service name，所以後兩項填0
            NI_NUMERICHOST);								//NI_NUMERICHOST指我們想以IP_address的方式看hostname
}


//用來送出檔案到對應的client
//第二參數是所求資料的路徑(在http request中)
void serve_resource(struct client_info *client, const char *path) {

    printf("serve_resource %s %s\n", client->addr_IP, path);

	//如果路徑是/，則回傳index.html
    if (strcmp(path, "/") == 0) path = "/index.html";

	//為了之後能用一個固定大小的buffer存路徑，我們這邊設定一個path的長度上限
    if (strlen(path) > 127) {
        send_400(client);
        return;
    }
	
	//我們不允許clients查找web資料夾以外的東西
    if (strstr(path, "..")) {
        send_404(client);
        return;
    }

    char full_path[128];
	//web資料夾內存了所有可提供給client的資料
	//我們將web接上path，然後存進full_path中
    sprintf(full_path, "web%s", path);
    
	//我們用fopen來查看client所求的資料存不存在
    FILE *fp = fopen(full_path, "rb");

	//不存在的話，回傳404
    if (!fp) {
        send_404(client);
        return;
    }

	//下面的一串操作是在獲得檔案大小，以在之後填到Content-Length中
	//用fseek來設定fp指向檔案中的哪個位置，因為SEEK_END，且偏移量為0，所以fp現在是在檔案末尾
    fseek(fp, 0L, SEEK_END);
    size_t fileSize = ftell(fp);	//ftell用來告知fp當前位置和檔案開頭位置的byte偏移量
    rewind(fp);				//將fp設回檔案開頭處

	//得知file的type(MIME type)，以在之後填到Content-Type中
	//常數字串會自動被配置空間，所以能直接讓pointer指向它
    const char *mime_type = get_MIME_type(full_path);

    char buffer[MAX_RESPONSE_BUF];		//存HTTP reponse的buffer

    sprintf(buffer, "HTTP/1.1 200 OK\r\n");
    send(client->socketFd, buffer, strlen(buffer), 0);		

    sprintf(buffer, "Connection: close\r\n");
    send(client->socketFd, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Length: %u\r\n", fileSize);
    send(client->socketFd, buffer, strlen(buffer), 0);

    sprintf(buffer, "Content-Type: %s\r\n", mime_type);
    send(client->socketFd, buffer, strlen(buffer), 0);

    sprintf(buffer, "\r\n");								//HTTP response header以空白行為結尾，後面就是HTTP body
    send(client->socketFd, buffer, strlen(buffer), 0);		//send()的最後一個參數是flag，並沒有要用到，順帶一提，send()的回傳值是送出多少bytes
    

    int r = fread(buffer, 1, MAX_RESPONSE_BUF, fp);		//我們用fread()將fp每次讀BSIZE到buffer中，fread()回傳它讀了多少bytes(因為我們設size為1)
    while (r) {	
        send(client->socketFd, buffer, r, 0);			//送出去
        r = fread(buffer, 1, MAX_RESPONSE_BUF, fp);		//繼續讀，值到讀完
    }

    fclose(fp);										//關閉檔案
    close(client->socketFd);						//關閉檔案
}



void handle_POST(struct client_info *client, char *body){
	int i=0;
    char *conLen = strstr(client->request, "Content-Length");
    conLen = conLen + strlen("Content-Length: ");
    char *conLenEnd = strstr(conLen,"\r\n");
    char tmpLen[10];
    while(conLen!=conLenEnd){
        tmpLen[i] = *conLen;
        i++;
        conLen++;
	}
    tmpLen[i] = 0;
    int bodyLen = atoi(tmpLen);
							
    char *b = strstr(client->request, "boundary")+9;
	char *b_end = strstr(b, "\r\n");
							
	char boundary[128];
	i=2;
    boundary[0] = '-';
    boundary[1] = '-';
	while(b!=b_end){
		boundary[i] = *b;
		b++;
        i++;
	}
	boundary[i] = '\0';

    char filename[128];
                            
    char *con1 = strstr(body,"\r\n\r\n")+4;
    
    char *conEnd = strstr(con1,boundary);
    conEnd -= 2;
    
    char *con2 = strstr(conEnd,"\r\n\r\n")+4;
                            
    int j=0;
    int end = bodyLen-2-strlen(boundary)-2-2;
    i=0;

    while(i<bodyLen){
        if(body<con1){
            body++;
            i++;
        }
        else if(body<conEnd){
            filename[j] = *body;
            body++;
            i++;
            j++;
        }
        else if(body<con2){
            if(body==conEnd){
                filename[j] = 0;
                client->f = fopen(filename,"wb");
            }
            body++;
            i++;
        }
        else if(i<end){
            fwrite(body,1,1,client->f);
            body++;
            i++;
        }else{
            //if(*body!='\r')
                //printf("e#%d:%c#e\n",i,*body);
            body++;
            i++;
        }
        
    }
    fclose(client->f); //記得close,否則不會寫檔寫進去

    serve_resource(client,"/done.html");
							
}

int main(int argc, char* argv[]) {
    
    int socketListen;
    struct client_info client;
    pid_t pid;

    if(argc>1){
        socketListen=create_socket(0,argv[1]);
    }
    else{
        socketListen=create_socket(0,"80");
    }

    while(1) {
	    client.socketFd = accept(socketListen,							
                    (struct sockaddr*) &(client.addr),
                    &(client.addr_len));

		
		signal(SIGCHLD,SIG_IGN);
		pid = fork();
		
		if(pid==0){
			//child process
            
            if(client.socketFd<0){
			    fprintf(stderr, "accept() failed. (%d)\n", errno);		
			    exit(1);
		    }
			close(socketListen);
			get_client_address(&client);
            printf("New connection from %s.\n",	client.addr_IP);					//印出是誰來連線
                    	
			//接受資料
            int r;
            r = recv(client.socketFd,								//用recv()來接收client端傳給server端的資料(HTTP request)
                        client.request,				   //不用read()的理由是，它只能用在Unix平台
                        MAX_REQUEST_SIZE, 0);    //request是buffer，加上已經收到多少byte，也就是一直往後放,第三參數是緩衝區還有多大，第四參數是flag，不管它


                	
											
			//如果讀到的bytes數少於1

            if (r < 1) {
                    printf("Unexpected disconnect from %s.\n", client.addr_IP);
                    close(client.socketFd);		//於是我們把該client斷線，並釋放資源
			}
			else{
                    client.received += r;						//讀多少就加上去
                    client.request[client.received] = 0;		//補上\0 (因為我們得到的資料不保證一定會有\0，要是最後收到的那筆沒有\0，到時候讀取時就不知終止處了
				    //printf("%s\n",client.request);

                    char *header = strstr(client.request, "\r\n\r\n");		//是否讀完header            
                    char *body = header + 4;
					
					if (header) {
                        *header = 0;

                        if (strncmp("GET /", client.request, 5)==0){
                            char *path = client.request + 4;				//也就是跳過GET和空格，到檔名處
                            char *end_path = strstr(path, " ");				//直到下個空格，就是檔名結束處，比如GET /form.html HTTP/1.1
							
							//找不到空格就報錯
                            if (!end_path) {
                                send_400(&client);
                            } else {
                                *end_path = 0;	//把末尾設為\0
                                serve_resource(&client, path);		//送出資料給client
                            }
                        
						}else if(strncmp("POST /", client.request, 6)==0){
							handle_POST(&client,body);
						}
					}
				}
        exit(1);   //重要,一定要加
		}
		else{
			close(client.socketFd);
		}		
	}	
    close(socketListen);	
    return 0;
}

