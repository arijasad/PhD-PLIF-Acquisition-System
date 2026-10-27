#include <arpa/inet.h> // inet_addr()
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h> // bzero()
#include <sys/socket.h>
#include <unistd.h> // read(), write(), close()

//#include <stdio.h>
//#include <string.h>
//#include <unistd.h>
//#include <stdio.h>
#define MAXSTR 100
#define MAX 80
#define PORT 8080
#define SA struct sockaddr

static int write_gpio_file(const char *path, const char *value)
{
        FILE *fp = fopen(path, "w");
        if (fp == NULL) {
                perror(path);
                return -1;
        }
        if (fprintf(fp, "%s", value) < 0) {
                perror(path);
                fclose(fp);
                return -1;
        }
        if (fclose(fp) != 0) {
                perror(path);
                return -1;
        }
        return 0;
}


static int write_gpio_stream(FILE *stream, const char *path, const char *value)
{
        if (fseek(stream, 0, SEEK_SET) != 0) {
                perror(path);
                return -1;
        }
        if (fprintf(stream, "%s", value) < 0) {
                perror(path);
                return -1;
        }
        if (fflush(stream) != 0) {
                perror(path);
                return -1;
        }
        return 0;
}

static int export_gpio(const char *gpio_path, const char *pin)
{
        char path[MAXSTR];
        snprintf(path, sizeof(path), "%s/gpio%s", gpio_path, pin);
        if (access(path, F_OK) == 0) {
                return 0;
        }
        snprintf(path, sizeof(path), "%s/export", gpio_path);
        return write_gpio_file(path, pin);
}
void func(int sockfd)
{
        FILE *fp;
        FILE *fq;
        FILE *fr;
        FILE *fs;
        char pin_modulation[] = "78";
        char pin_interlock[] = "72"; //72
        char pin_fault[] = "8"; //Also need to change inside the code 8
        char pin_sys[] = "38";
        char GPIOPATH[] = "/sys/class/gpio";
        char path[MAXSTR] = "";
        char path1[MAXSTR] = "";
        char path2[MAXSTR] = "";
        char path3[MAXSTR] = "";

        if (export_gpio(GPIOPATH, pin_modulation) != 0 ||
            export_gpio(GPIOPATH, pin_interlock) != 0 ||
            export_gpio(GPIOPATH, pin_fault) != 0 ||
            export_gpio(GPIOPATH, pin_sys) != 0) {
                return;
        }

        snprintf(path, MAXSTR, "%s/gpio%s/direction", GPIOPATH, pin_modulation);
        if (write_gpio_file(path, "out") != 0) return;
        snprintf(path1, MAXSTR, "%s/gpio%s/direction", GPIOPATH, pin_interlock);
        if (write_gpio_file(path1, "out") != 0) return;
        snprintf(path3, MAXSTR, "%s/gpio%s/direction", GPIOPATH, pin_sys);
        if (write_gpio_file(path3, "out") != 0) return;
        snprintf(path2, MAXSTR, "%s/gpio%s/direction", GPIOPATH, pin_fault);
        if (write_gpio_file(path2, "in") != 0) return;

        //Sends the output signal to the modulation input, interlock and system status
        int state = 0;
        char fault_value_path[MAXSTR];
        snprintf(path, MAXSTR, "%s/gpio%s/value", GPIOPATH, pin_modulation);
        fp = fopen(path, "w");
        if (fp == NULL) { perror(path); return; }
        snprintf(path1, MAXSTR, "%s/gpio%s/value", GPIOPATH, pin_interlock);
        fq = fopen(path1, "w");
        if (fq == NULL) { perror(path1); fclose(fp); return; }
        snprintf(path3, MAXSTR, "%s/gpio%s/value", GPIOPATH, pin_sys);
        fs = fopen(path3, "w");
        if (fs == NULL) { perror(path3); fclose(fp); fclose(fq); return; }
        snprintf(fault_value_path, sizeof(fault_value_path), "%s/gpio%s/value", GPIOPATH, pin_fault);
        fr = fopen(fault_value_path, "r");
        if (fr == NULL) { perror(fault_value_path); fclose(fp); fclose(fq); fclose(fs); return; }

        int val = 0;
        while(1){
                if (fscanf(fr, "%d", &val) != 1) {
                        fclose(fr); fclose(fp); fclose(fq); fclose(fs);
                        return;
                }
                if (fclose(fr) != 0) {
                        perror(fault_value_path);
                        fclose(fp); fclose(fq); fclose(fs);
                        return;
                }
                fr = fopen(fault_value_path, "r");
                if (fr == NULL) {
                        perror(fault_value_path);
                        fclose(fp); fclose(fq); fclose(fs);
                        return;
                }
                if (write_gpio_stream(fq, path1, "1") != 0 ||
                    write_gpio_stream(fs, path3, "1") != 0) {
                        fclose(fr); fclose(fp); fclose(fq); fclose(fs);
                        return;
                }
                if (state){
                        if (write_gpio_stream(fp, path, "1") != 0) {
                                fclose(fr); fclose(fp); fclose(fq); fclose(fs);
                                return;
                        }
                        usleep(5000000);//Time between signal
              }else{
                        if (write_gpio_stream(fp, path, "0") != 0) {
                                fclose(fr); fclose(fp); fclose(fq); fclose(fs);
                                return;
                        }
                        //Request storage of the latest completed camera frame
                        char buff[MAX];
                        int n;
                        bzero(buff, 4);
                     //   printf("Laser modulation time ");
                        char buff1[] = "store";
                        n = 0;
//                      while ((buff[n++] = getchar()) != '\n');
                        ssize_t sent = write(sockfd, buff1, sizeof(buff1));
                        if (sent != (ssize_t)sizeof(buff1)) {
                                perror("store request");
                                fclose(fr); fclose(fp); fclose(fq); fclose(fs);
                                return;
                        }
                        usleep(1000000);//Time the laser is on for
                }
                state = ~state; }
}


int main()
{
    int sockfd, connfd;
    struct sockaddr_in servaddr, cli;
 
    // socket create and verification
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd == -1) {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));
 
    // assign IP, PORT
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = inet_addr("169.254.90.110");
    servaddr.sin_port = htons(PORT);
 
    // connect the client socket to server socket
    if (connect(sockfd, (SA*)&servaddr, sizeof(servaddr))
        != 0) {
        printf("connection with the server failed...\n");
        exit(0);
    }
    else
        printf("connected to the server..\n");
 
    // function for chat
    func(sockfd);
 
    // close the socket
    close(sockfd);
}




