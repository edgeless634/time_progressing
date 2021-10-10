#include <ESP8266WiFi.h>

#define TCP_SERVER_ADDR "bemfa.com" //巴法云服务器地址默认即可
#define TCP_SERVER_PORT "8344" //服务器端口，tcp创客云端口8344··
typedef void (*func)(String);

//最大字节数
#define MAX_PACKETSIZE 512
//设置心跳值30s
#define KEEPALIVEATIME 30*1000

//处理传入的消息（回调函数）
void defaultHandleMessage(String message);

class MyBemfa{
  public:
    //相关函数初始化

    //初始化函数
    MyBemfa(String ssid, String passwd, String uid, String topic);

    //总控制函数
    void tick();
    //连接WIFI
    void doWiFiTick();
    void startSTA();

    //TCP初始化连接
    void doTCPClientTick();
    void startTCPClient();
    void sendtoTCPServer(String p);

    //自定义回调函数
    void setCallbackFunction(func f);

  private:
    //tcp客户端相关初始化，默认即可
    WiFiClient TCPclient;
    String TcpClient_Buff = "";//初始化字符串，用于接收服务器发来的数据
    String STA_SSID;
    String STA_PASSWD;
    String UID;
    String TOPIC;
    unsigned int TcpClient_BuffIndex = 0;
    unsigned long TcpClient_preTick = 0;
    unsigned long preHeartTick = 0;//心跳
    unsigned long preTCPStartTick = 0;//连接
    bool preTCPConnected = false;
    func handleMessage = defaultHandleMessage;
};


MyBemfa::MyBemfa(String ssid, String passwd, String uid, String topic) {
  STA_SSID = ssid;
  STA_PASSWD = passwd;
  UID = uid;
  TOPIC = topic;
  startTCPClient();
}

/*  
  *发送数据到TCP服务器
 */
void MyBemfa::sendtoTCPServer(String p){
  if (!TCPclient.connected()) {
    Serial.println("Client is not readly");
    return;
  }
  TCPclient.print(p);
}


/*
  *初始化和服务器建立连接
*/
void MyBemfa::startTCPClient() {
  if(TCPclient.connect(TCP_SERVER_ADDR, atoi(TCP_SERVER_PORT))) {
    Serial.print("\nConnected to server:");
    Serial.printf("%s:%d\r\n",TCP_SERVER_ADDR,atoi(TCP_SERVER_PORT));
    
    String tcpTemp="";  //初始化字符串
    tcpTemp = "cmd=3&uid="+UID+"&topic="+TOPIC+"\r\n"; //构建订阅指令
    sendtoTCPServer(tcpTemp); //发送订阅指令
    tcpTemp="";//清空
    /*
     //如果需要订阅多个主题，可再次发送订阅指令
      tcpTemp = "cmd=1&uid="+UID+"&topic="+主题2+"\r\n"; //构建订阅指令
      sendtoTCPServer(tcpTemp); //发送订阅指令
      tcpTemp="";//清空
     */
    
    preTCPConnected = true;
    preHeartTick = millis();
    TCPclient.setNoDelay(true);
  }else{
    Serial.print("Failed connected to server:");
    Serial.println(TCP_SERVER_ADDR);
    TCPclient.stop();
    preTCPConnected = false;
  }
  preTCPStartTick = millis();
}


/*
  *检查数据，发送心跳
*/
void MyBemfa::doTCPClientTick(){
 //检查是否断开，断开后重连
  if(WiFi.status() != WL_CONNECTED) {
    return;
  }
  if (!TCPclient.connected()) {//断开重连
    if(preTCPConnected == true) {
      preTCPConnected = false;
      preTCPStartTick = millis();
      Serial.println();
      Serial.println("TCP Client disconnected.");
      TCPclient.stop();
    }
    else if(millis() - preTCPStartTick > 1*1000) {//重新连接
      startTCPClient();
    }
  }else{
    if (TCPclient.available()) {//收数据
      char c =TCPclient.read();
      TcpClient_Buff +=c;
      TcpClient_BuffIndex++;
      TcpClient_preTick = millis();
      
      if(TcpClient_BuffIndex>=MAX_PACKETSIZE - 1) {
        TcpClient_BuffIndex = MAX_PACKETSIZE-2;
        TcpClient_preTick = TcpClient_preTick - 200;
      }
      preHeartTick = millis();
    }
    if(millis() - preHeartTick >= KEEPALIVEATIME){//保持心跳
      preHeartTick = millis();
      Serial.println("--Keep alive:");
      sendtoTCPServer("ping\r\n"); //发送心跳，指令需\r\n结尾，详见接入文档介绍
    }
  }
  if((TcpClient_Buff.length() >= 1) && (millis() - TcpClient_preTick>=200)) {
    TCPclient.flush();
    Serial.print("Rev string: ");
    TcpClient_Buff.trim(); //去掉首位空格
    Serial.println(TcpClient_Buff); //打印接收到的消息
    String getTopic = "";
    String getMsg = "";
    if(TcpClient_Buff.length() > 15) {//注意TcpClient_Buff只是个字符串，在上面开头做了初始化 String TcpClient_Buff = "";
      //此时会收到推送的指令，指令大概为 cmd=2&uid=xxx&topic=light002&msg=off
      int topicIndex = TcpClient_Buff.indexOf("&topic=")+7; //c语言字符串查找，查找&topic=位置，并移动7位，不懂的可百度c语言字符串查找
      int msgIndex = TcpClient_Buff.indexOf("&msg=");//c语言字符串查找，查找&msg=位置
      getTopic = TcpClient_Buff.substring(topicIndex,msgIndex);//c语言字符串截取，截取到topic,不懂的可百度c语言字符串截取
      getMsg = TcpClient_Buff.substring(msgIndex+5);//c语言字符串截取，截取到消息
      Serial.print("topic:------");
      Serial.println(getTopic); //打印截取到的主题值
      Serial.print("msg:--------");
      Serial.println(getMsg);   //打印截取到的消息值
    }
    handleMessage(getMsg);

    TcpClient_Buff="";
    TcpClient_BuffIndex = 0;
  }
}


/*
  *初始化wifi连接
*/
void MyBemfa::startSTA() {
  WiFi.disconnect();
  WiFi.mode(WIFI_STA);
  WiFi.begin(STA_SSID, STA_PASSWD);
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.println("WIFI not Connect");
  }
  Serial.println("Connected to AP");
  Serial.print("IP address is ");
  Serial.println(WiFi.localIP());
}


/*
  WiFiTick
  检查是否需要初始化WiFi
  检查WiFi是否连接上，若连接成功启动TCP Client
  控制指示灯
*/
void MyBemfa::doWiFiTick(){
  static bool startSTAFlag = false;
  static bool taskStarted = false;
  static uint32_t lastWiFiCheckTick = 0;

  if (!startSTAFlag) {
    startSTAFlag = true;
    startSTA();
  }

  //未连接1s重连
  if ( WiFi.status() != WL_CONNECTED ) {
    if (millis() - lastWiFiCheckTick > 1000) {
      lastWiFiCheckTick = millis();
    }
  }
  //连接成功建立
  else {
    if (taskStarted == false) {
      taskStarted = true;
      Serial.print("\r\nGet IP Address: ");
      Serial.println(WiFi.localIP());
      startTCPClient();
    }
  }
}

void MyBemfa::setCallbackFunction(func f) {
  handleMessage = f;
}

void defaultHandleMessage(String message) {
  if(message  == "on"){       //如果是消息==打开
    Serial.println("Turn ON");
    digitalWrite(D5,LOW);
  }else if(message == "off"){ //如果是消息==关闭
    Serial.println("Turn OFF");
    digitalWrite(D5,HIGH);
  }
}

void MyBemfa::tick() {
  doWiFiTick();
  doTCPClientTick();
}