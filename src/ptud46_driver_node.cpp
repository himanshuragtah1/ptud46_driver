/* ptud32 driver node
 ROS connection -complete
 command line arguments 
 -reset         :resets the device
 -com /dev/ttyUSB1 : changes the com port
Prerequisits: PTU powered and initialized(optional)

for first run call: rosrun ptud46_driver ptud46_driver_node -reset -com /dev/ttyUSB0
after : rosrun ptud46_driver ptud46_driver_node -com /dev/ttyUSB0


TODO : use standard tf_pose_command for pose_cmd : optional
Increase Baud rate :done
flush serial buffers :done
allow to send the reset option, port, through command line:done
Implement timeout for the while loops : done
TODO : maximize speed - too slow now! - maximize baud, maximize speed, minimize payload, 
*/


#include "ros/ros.h"
#include "std_msgs/String.h"
#include "geometry_msgs/Vector3.h"
#include <tf/transform_broadcaster.h>

#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <termios.h>
#include <unistd.h>
#include <ctime>

#define BAUDRATE B9600    //powerup Baud
#define BAUDRATE2 B38400  //max Baud
#define MODEMDEVICE "/dev/ttyUSB0"

int fd1=0;
char *buffer,*bufptr;
unsigned char buff;
int wr,rd,nbytes,tries;
speed_t baud = BAUDRATE;
speed_t baud2 = BAUDRATE2;
double pan_ang;
double tilt_ang;
double pan_cmd = 0;
double tilt_cmd = 100;
bool new_cmd=true;
bool timeout=false;
bool reset=false;
bool comchange=false;
std::string user_com;
time_t start,now;

void openSerial(void);
void resetPtu(void);
void configPtu(void);
void waitForPtu(void);
int read_line(unsigned char buff_line[],int size);
void cmdpose_callback(const geometry_msgs::Vector3 cmdpose_msg);


int main(int argc, char **argv){
      
	for(int i=1; i<argc;i++){
	        //std::cout<<argv[i]<<std::endl;
	        std::string arg = argv[i];
		if (arg=="-reset"){
		reset=true;}
		if (arg=="-com"){
		user_com=argv[i+1];
		comchange=true;}
	}

	// Initialize ROS
	ros::init(argc, argv, "ptud46_driver_node");
	ros::NodeHandle n;
	tf::Transform transform;
  	tf::Quaternion q;
	ros::Rate loop_rate(500);
	static tf::TransformBroadcaster br;
	ros::Subscriber cmdpose_sub = n.subscribe("cmd_pose", 1, cmdpose_callback);
	ros::Publisher ready_pub = n.advertise<std_msgs::String>("ptu_ready", 100, false);

	// Initialize System
	openSerial();	//Serial port init
	
	configPtu();    //config PTU connection
	if(reset) resetPtu();   //PTU reset
	waitForPtu();
	rd=read(fd1, &buff, 1);
	rd=read(fd1, &buff, 1);
	rd=read(fd1, &buff, 1);
	
	ROS_INFO("start");

	// Loop	
	pan_ang = 0;
	tilt_ang = 0;
	std::string cmd_string;
	std::stringstream cmd_string_stream;
	std_msgs::String msg2;
	int count = 0;
	unsigned char buff_line[10];
	bool fwdp=true;
	bool fwdt=true;
	while (ros::ok())
	{   //1. Publish angle of PTU to /tf
	
	    tcflush(fd1, TCIOFLUSH);	
	    //1.1 Get Pan angle
	    wr=write(fd1,"PP\n",3); 		  	//request pan angle from ptu
            if(wr <0)  ROS_INFO("write failure"); 	//report errors
            rd=read(fd1, &buff, 1);			//read *
    	    rd=read_line(buff_line,10);   // read line from buffer
	    if(rd <0)  {ROS_INFO("read failure");}
	    else{
		      //ROS_INFO("%s", &buff_line[0]);
		      pan_ang=atol(reinterpret_cast<const char*>(buff_line));
		      ROS_INFO("%f",pan_ang);
	    }
	    //rd=read(fd1, &buff, 1);  //check this (better do a sync process)
	    
	    //1.2 Get Tilt angle
	    wr=write(fd1,"TP\n",3); 		  	//request tilt angle from ptu
            if(wr <0)  ROS_INFO("write failure"); 	//report errors
            rd=read(fd1, &buff, 1);			//read *
    	    rd=read_line(buff_line,10);   // read line from buffer
	    if(rd <0)  {ROS_INFO("read failure");}
	    else{
		      //ROS_INFO("%s", &buff_line[0]);
		      tilt_ang=atol(reinterpret_cast<const char*>(buff_line));
		      ROS_INFO("%f",tilt_ang);
	    }
	    //rd=read(fd1, &buff, 1);
    	
	    
	    //1.3 Create /tf massege
	    transform.setOrigin( tf::Vector3(0.0, 0.0, 0.0) );
  	    q.setRPY(0, 0, pan_ang*0.00089759762);
  	    transform.setRotation(q);
 	    br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "world", "panmotor"));
	    
	    transform.setOrigin( tf::Vector3(0.0, 0.0, 0.067437) );
  	    q.setRPY(0, -tilt_ang*0.00089759762, 0);
  	    transform.setRotation(q);
 	    br.sendTransform(tf::StampedTransform(transform, ros::Time::now(), "panmotor", "tiltmotor"));
	    
	        
	    //2.send angle commands to PTU
            //2.1 check new angle commands
            
            //2.2 send commands 
                //pan_cmd=1000;
                //tilt_cmd=100;
            if (new_cmd){
                msg2.data = "!";
                ready_pub.publish(msg2);
                cmd_string_stream.str("");
                cmd_string_stream<<"PP"<< int(pan_cmd) <<std::endl;
                cmd_string=cmd_string_stream.str();
                //std::cout<<cmd_string.c_str()<<std::endl;
	    	wr=write(fd1,cmd_string.c_str(),cmd_string.length());
    		rd=read(fd1, &buff, 1);
    		ROS_INFO("%s", &buff);
    		if (buff!='*'){ROS_INFO("command fail");}
    		waitForPtu();
    		rd=read(fd1, &buff, 1);
    		rd=read(fd1, &buff, 1);
    		
    		cmd_string_stream.str("");
                cmd_string_stream<<"TP"<< int(tilt_cmd) <<std::endl;
                cmd_string=cmd_string_stream.str();
                //std::cout<<cmd_string.c_str()<<std::endl;
	    	wr=write(fd1,cmd_string.c_str(),cmd_string.length());
    		rd=read(fd1, &buff, 1);
    		ROS_INFO("%s", &buff);
    		if (buff!='*'){ROS_INFO("command fail");}
    		waitForPtu();
    		rd=read(fd1, &buff, 1);
    		rd=read(fd1, &buff, 1);
    		
    		new_cmd=false;
	      }
	    
	 
         msg2.data = "*";
         ready_pub.publish(msg2);  
	      

	    //scan commands for testing
	    if(0){
	    if (pan_cmd<2000 && fwdp){
	    	pan_cmd=pan_cmd+100;
	    }
	    else{
	        fwdp=false;
	    }
	    
	    if (pan_cmd>-2000 && !fwdp){
	    	pan_cmd=pan_cmd-100;
	    }
	    else{
	        fwdp=true;
	    }
	    }
	    
	    if(0){
	    if (tilt_cmd<550 && fwdt){
	    	tilt_cmd=tilt_cmd+5;
	    }
	    else{

	        fwdt=false;
	    }
	    
	    if (tilt_cmd>-850 && !fwdt){
	    	tilt_cmd=tilt_cmd-5;
	    }
	    else{
	        fwdt=true;
	    }
	    }
	    

	    ros::spinOnce();
	    loop_rate.sleep();
	}
	close(fd1);
	return 0;
}

void cmdpose_callback(const geometry_msgs::Vector3 cmdpose_msg){
	double old_pan_cmd = pan_cmd;
	double old_tilt_cmd = tilt_cmd;
	pan_cmd=cmdpose_msg.x;
	tilt_cmd=cmdpose_msg.y;
	//std::cout<<pan_ang<<std::endl;
	if(pan_cmd==old_pan_cmd && tilt_cmd==old_tilt_cmd){
		new_cmd=false;
	}
	else{
		new_cmd=true;
	}
}


void openSerial(){
	if(comchange){
	fd1=open(user_com.c_str(), O_RDWR | O_NOCTTY | O_NDELAY);
	}
	else{
	fd1=open(MODEMDEVICE, O_RDWR | O_NOCTTY | O_NDELAY);
	}
	if (fd1 == -1 )
	{
		perror("open_port: Unable to open /dev/ttyUSB1 –");
		exit (EXIT_FAILURE);
	}

	else
	{
		fcntl(fd1, F_SETFL,0);
		printf("Serial port open\n");
	}

	/* set the other settings */
	struct termios settings;
	tcgetattr(fd1, &settings);

	cfsetospeed(&settings, baud); 		/* baud rate */
        cfsetispeed(&settings, baud); 		/* baud rate */
	settings.c_cflag &= ~PARENB; 		/* no parity */
	settings.c_cflag &= ~CSTOPB; 		/* 1 stop bit */
	settings.c_cflag &= ~CSIZE;
	settings.c_cflag |= CS8 | CLOCAL; 	/* 8 bits and local mode*/
	settings.c_lflag &= ~(ICANON | ECHO | ECHOE | ISIG); /* raw input */
	settings.c_oflag &= ~OPOST; 		/* raw output */
        //settings.c_cflag &= ~CNEW_RTSCTS;     /* no hardware flow control*/
	settings.c_cc[VINTR] = 1;     		/* Ctrl-c */
	settings.c_cc[VMIN]  = 0;		/* minimum read bytes */
	settings.c_cc[VTIME] = 5;		/* time out 1=0.1s*/
	tcsetattr(fd1, TCSANOW, &settings); 	/* apply the settings */
	sleep(2); //required to make flush work, for some reason
  	tcflush(fd1,TCIOFLUSH);

	tcgetattr(fd1, &settings);
	speed_t ispeed = cfgetispeed(&settings);
	speed_t ospeed = cfgetospeed(&settings);
	printf("baud rate in: 0%o\n", ispeed);
	printf("baud rate out: 0%o\n", ospeed);

}

void resetPtu(void){
 /* Reseting the PTU is performed by sending the command 'R\n' through serial
    Would take around 10-20 s for the task and returns '*' when the process is complete
    Includes a timeout to stop hanging of the code */
    std::cout<<"Initializing PTU..."<<std::endl;
    wr=write(fd1,"R\n",2); 
    if(wr <0)  {ROS_INFO("write failure");
    		exit (EXIT_FAILURE);}
    bool init_flag= false;
    time(&start);
    int count=0;
    while (!init_flag){
    	rd=read(fd1, &buff, 1);
    	if (rd>0){
    		std::cout<<buff;
    		if (buff=='*'){
    			std::cout<<"PTU Ready"<<std::endl;
    			init_flag=true;
    		}
    	}
    	time(&now);
    	if(now > start + 90)
    	{
        	std::cout<<"Connection timeout init!\n"<<std::endl;
        	timeout=true;
        	exit(EXIT_FAILURE);  
        	break;
    	}
   } 
}


void configPtu(void){
 /* Check Ptu response */
    std::cout<<"Checking PTU connection..."<<std::endl;
    waitForPtu();
    
    if(timeout){
    //try changing to  38400
    std::cout<<"Connection timeout! trying 38400 baud\n"<<std::endl;
    struct termios settings;
    tcgetattr(fd1, &settings);
    cfsetospeed(&settings, baud2); 		/* baud rate */
    cfsetispeed(&settings, baud2); 		/* baud rate */
    tcsetattr(fd1, TCSANOW, &settings); 
    tcflush(fd1, TCOFLUSH);
    	tcgetattr(fd1, &settings);
	speed_t ispeed = cfgetispeed(&settings);
	speed_t ospeed = cfgetospeed(&settings);
	printf("baud rate in: 0%o\n", ispeed);
	printf("baud rate out: 0%o\n", ospeed);
        
    timeout=false;
    waitForPtu();
    if(timeout){
      std::cout<<"Connection timeout @ 38400 baud... exit\n"<<std::endl; 
      exit(EXIT_FAILURE);  
    }
    }
    
    
    std::cout<<"Writing configuration..."<<std::endl;
    //Disable Echo
    wr=write(fd1,"ED\n",3);
    bool init_flag= false;
    
    
    time(&start);		
    while (!init_flag){
    	rd=read(fd1, &buff, 1);
    	if (rd>0){
    		//std::cout<<buff;
    		if (buff=='*'){
    			//std::cout<<"PTU Ready"<<std::endl;
    			init_flag=true;
    		}
    	}
    	time(&now);
    	if(now > start + 5)
    	{
        	std::cout<<"Connection timeout!\n"<<std::endl;
        	timeout=true;
        	break;
    	}
    } 
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    
    
    //Set ASCII feedback
    wr=write(fd1,"FT\n",3);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){std::cout<<"Config Fail"<<std::endl;exit(EXIT_FAILURE);}
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);

    //Set Pan speed
    wr=write(fd1,"PS2900\n",7);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){std::cout<<"Config Fail"<<std::endl;exit(EXIT_FAILURE);}
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    //Set Tilt speed
    wr=write(fd1,"TS2900\n",7);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){std::cout<<"Config Fail"<<std::endl;exit(EXIT_FAILURE);}
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    //Increase baud rate
    //if(!timeout){
	    wr=write(fd1,"@(38400,0,F)\n",13);
	    rd=read(fd1, &buff, 1);
	    ROS_INFO("%s", &buff);
	    if (buff!='*'){std::cout<<"Config Fail"<<std::endl;exit(EXIT_FAILURE);}
	    rd=read(fd1, &buff, 1);
	    rd=read(fd1, &buff, 1);
	    
	    struct termios settings;
	    tcgetattr(fd1, &settings);
	    cfsetospeed(&settings, baud2); 		/* baud rate */
	    cfsetispeed(&settings, baud2); 		/* baud rate */
	    tcsetattr(fd1, TCSANOW, &settings); 
	    tcflush(fd1, TCOFLUSH);
    //}
    
    if(0){
    //test
    wr=write(fd1,"PP1000\n",7);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){exit(EXIT_FAILURE);}
    waitForPtu();
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    //test
    wr=write(fd1,"PP0\n",4);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){exit(EXIT_FAILURE);}
    waitForPtu();
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    //test
    wr=write(fd1,"TP-900\n",7);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){exit(EXIT_FAILURE);}
    waitForPtu();
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    
    //test
    wr=write(fd1,"TP0\n",4);
    rd=read(fd1, &buff, 1);
    ROS_INFO("%s", &buff);
    if (buff!='*'){exit(EXIT_FAILURE);}
    waitForPtu();
    rd=read(fd1, &buff, 1);
    rd=read(fd1, &buff, 1);
    }
    std::cout<<"Configuration complete..."<<std::endl;
}

void waitForPtu(void){
    /* Wait for Ptu command completion */
    wr=write(fd1,"A\n",2); //request angle from ptu
    if(wr <0)  {ROS_INFO("write failure");
    		exit (EXIT_FAILURE);}
    bool init_flag= false;
    time(&start);		
    while (!init_flag){
    	rd=read(fd1, &buff, 1);
    	if (rd>0){
    		std::cout<<buff;
    		if (buff=='*'){
    			//std::cout<<"PTU Ready"<<std::endl;
    			init_flag=true;
    		}
    		if(buff=='!'){
    		       wr=write(fd1,"A\n",2); //retry
  		       if(wr <0)  {ROS_INFO("write failure");
    		                   exit (EXIT_FAILURE);}
      		}
    	}
    	time(&now);
    	//std::cout<<now<<std::endl;
    	if(now > start + 5)
    	{
        	std::cout<<"Connection timeout!\n"<<std::endl;
        	timeout=true;
        	break;
    	}
   } 
}

int read_line(unsigned char buff_line[],int size){
	    bool eol=false;     //end of line flag
	    int i=0;		//line size 
	    memset (buff_line,' ',size);
	    while(!eol && i<99){
            	rd=read(fd1, &buff, 1);   //read charachter
	    	if (rd<1) {
	    		//ROS_INFO("Read failure or timeout"); 
	    		eol=true;
	    		i=-1;
	    	}
	    	else{
	    		if (buff=='\n') {
	    			eol=true;
	    		}
	    		else{
	    			buff_line[i]=buff;
	    			i++;
			}
		}
		if (i==size-1){
			ROS_INFO("Serial sw_buffer overflow");
			i=-1;
		}
	}
	//rd=read(fd1, &buff, 1);
        //rd=read(fd1, &buff, 1);
	//ROS_INFO("hi");
	//ROS_INFO("%s", &buff_line[0]);
     return i;
}

