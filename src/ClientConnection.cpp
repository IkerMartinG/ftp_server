//****************************************************************************
//                         REDES Y SISTEMAS DISTRIBUIDOS
//                      
//                     2º de grado de Ingeniería Informática
//                       
//              This class processes an FTP transaction.
// 
//****************************************************************************



#include <cstring>
#include <cstdarg>
#include <cstdio>
#include <cerrno>
#include <netdb.h>

#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pwd.h>
#include <grp.h>
#include <time.h>
#include <locale.h>
#include <langinfo.h>
#include <fcntl.h>
#include <unistd.h>

#include <sys/stat.h> 
#include <iostream>
#include <dirent.h>

#include "common.h"

#include "ClientConnection.h"
#include "FTPServer.h"




ClientConnection::ClientConnection(int s) {
    int sock = (int)(s);
  
    char buffer[MAX_BUFF];

    control_socket = s;
    // Check the Linux man pages to know what fdopen does.
    fd = fdopen(s, "a+");
    if (fd == NULL){
	std::cout << "Connection closed" << std::endl;

	fclose(fd);
	close(control_socket);
	ok = false;
	return ;
    }
    
    ok = true;
    data_socket = -1;
    parar = false;
   
  
  
};


ClientConnection::~ClientConnection() {
 	fclose(fd);
	close(control_socket); 
  
}


int connect_TCP( uint32_t address,  uint16_t  port) {
    struct sockaddr_in sin;
    struct hostent *hent;
    int s;

    memset(&sin, 0, sizeof(sin));
    sin.sin_family = AF_INET;
    sin.sin_port = htons(port);

    memcpy(&sin.sin_addr, &address, sizeof(address));

    s = socket(AF_INET, SOCK_STREAM, 0);

    if(s < 0)
       errexit("No se puede crear el socket: %s\n", strerror(errno));
    if(connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0)
       errexit("No se puede conectar con %s: %s\n", address, strerror(errno));

    return s;

}






void ClientConnection::stop() {
    close(data_socket);
    close(control_socket);
    parar = true;
  
}





    
#define COMMAND(cmd) strcmp(command, cmd)==0

// This method processes the requests.
// Here you should implement the actions related to the FTP commands.
// See the example for the USER command.
// If you think that you have to add other commands feel free to do so. You 
// are allowed to add auxiliary methods if necessary.

void ClientConnection::WaitForRequests() {
    if (!ok) {
	 return;
    }
    
    fprintf(fd, "220 Service ready\n");
  
    while(!parar) {

      fscanf(fd, "%s", command);
      if (COMMAND("USER")) {
	    fscanf(fd, "%s", arg);
	    fprintf(fd, "331 User name ok, need password\n");
      }
      else if (COMMAND("PWD")) {
	   
      }
      else if (COMMAND("PASS")) {
        fscanf(fd, "%s", arg);
        if(strcmp(arg,"1234") == 0){
            fprintf(fd, "230 User logged in\n");
        }
        else{
            fprintf(fd, "530 Not logged in.\n");
            parar = true;
        }
	   
      }
      else if (COMMAND("PORT")) {
        int a1, a2, a3, a4;
        int p1, p2;

        std::cout << "PRE READ PORT INFO\n";
        fscanf(this->fd, "%d,%d,%d,%d,%d,%d", &a1, &a2, &a3, &a4, &p1, &p2);
        std::cout << "address: " << a1 << "." << a2 << "." << a3 << "." << a4 << std::endl;
        
        uint32_t address = (a1 << 24) | (a2 << 16) | (a3 << 8) | a4;
        uint16_t port = (p1 << 8) | p2;
        
        std::cout << "PRE CONNECT_TCP\n";
        std::cout << "Address ->" << address << " Port -> " << port << "\n";
        this->data_socket = connect_TCP(address, port);
       
        fprintf(this->fd, "200 OK\n");
        fflush(this->fd);
      }
      else if (COMMAND("PASV")) {
        
        struct sockaddr_in fsin;
        socklen_t slen = sizeof(fsin);
        int s = define_socket_TCP(0); // Se define un socket en un puerto aleatorio libre en el sistema
        getsockname(s, (sockaddr*)&fsin, &slen);
        int port = ntohs(fsin.sin_port); // obtenemos el puerto
        int p1 = port / 256;
        int p2 = port % 256;
        fprintf(fd, "227 Entering Passive Mode (127,0,0,1,%d,%d)\n", p1, p2); // enviamos ip y puerto en el formato adecuado
        fflush(this->fd);
        data_socket = accept(s, (struct sockaddr*)&fsin, &slen);
  
      }else if (COMMAND("STOR") ) {
        fscanf(fd, "%s", arg);

        fprintf(fd, "150 FIle status okay; about to open data connection. \n");
        fflush(fd);
        FILE *f = fopen(arg, "wb+");

        if(f == NULL){
          fprintf(fd, "425 Can't open data connection. \n");
          fflush(fd);
          close(data_socket);
          break;
        }

        fprintf(fd, "125 Data connection already open; transfer starting. \n");
        char *buffer[MAX_BUFF];

        fflush(fd);

        while(1){
          int b = recv(data_socket, buffer, MAX_BUFF, 0);
          fwrite(buffer, 1, b, f);
          if(b == 0){
            break;
          }
        }

        fprintf(fd, "226 Closing data connection. \n");
        fflush(fd);
        fclose(f);
        close(data_socket);
      }
      else if (COMMAND("RETR")) {
        fscanf(fd, "%s", arg);

        FILE *f = fopen(arg, "rb"); // LECTURA NORMAL O BINARIO

        if(f != NULL){
          fprintf(fd, "125 Data connection already open; transfer starting. \n");
          fflush(fd);
          char *buffer[MAX_BUFF];

          while(1){
            int b = fread(buffer, 1, MAX_BUFF, f);

            send(data_socket, buffer, b, 0);
            if(b < MAX_BUFF){
              break;
            }
          }
          fprintf(fd, "226 Closing data connection. \n");
          fflush(fd);

          fclose(f);
          close(data_socket);
        } else {
          fprintf(fd, "425 Can't open data connection. \n");
          fflush(fd);
          close(data_socket);
        }
      }
      else if (COMMAND("LIST")) {
        DIR* d = opendir(".");
        fprintf(fd, "125 List started OK.\n");
        fflush(fd);
        struct dirent* entry;
        FILE* data_file = fdopen(this->data_socket, "wb");
        while ((entry = readdir(d)) != NULL) { // Mientras obtengamos un nombre de archivo/directorio
          std::string dir_name(entry->d_name);
          if (!(dir_name == "." || dir_name == "..")) {
            dir_name += "\x0d\x0a";
            fwrite(dir_name.c_str(), 1, dir_name.length(), data_file);      
          }
        }
        fflush(data_file);
        close(data_socket);
        closedir(d);
        fprintf(fd, "250 List completed succesfully.\n");
        fflush(fd);
      }
      else if (COMMAND("SYST")) {
           fprintf(fd, "215 UNIX Type: L8.\n");   
      }

      else if (COMMAND("TYPE")) {
	    fscanf(fd, "%s", arg);
	    fprintf(fd, "200 OK\n");   
      }
     
      else if (COMMAND("QUIT")) {
        fprintf(fd, "221 Service closing control connection. Logged out if appropriate.\n");
        close(data_socket);	
        parar=true;
        break;
      }
  
      else  {
	    fprintf(fd, "502 Command not implemented.\n"); fflush(fd);
	    printf("Comando : %s %s\n", command, arg);
	    printf("Error interno del servidor\n");
	
      }
      
    }
    
    fclose(fd);

    
    return;
  
};
