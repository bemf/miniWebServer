//
//  main.cpp
//  myTinyWeb
//
//  Created by wb on 2018/3/19.
//  Copyright © 2018年 wb. All rights reserved.
//

#include <iostream>

#include "common.hpp"

#define MAXLINE 256
#define MAXBUF 256

//处理请求
void doit(int fd);
void read_requesthdrs(rio_t *rp);
int parse_uri(char *uri, char *filename, char *cgiargs);
//处理静态请求
void serve_static(int fd, char *filename, int file_size);
void get_filetype(char *filename, char *filetype);
//处理动态请求
void serve_dynamic(int fd, char *filename, char *cgiargs);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);

int main(int argc, const char * argv[]) {
    std::cout<<"This is a tiny web server"<<std::endl;
    
    int listenfd;
    int connfd;
    
    //char hostname[MAXLINE],port[MAXLINE];
    
    socklen_t client_len;
    sockaddr_storage client_addr;
    
    listenfd=open_listenfd(PORT);
    
    //接受请求， 处理请求
    while (1) {
        client_len=sizeof(client_addr);
        connfd=accept(listenfd, (sockaddr *)&client_addr, &client_len);
        doit(connfd);
        close(connfd);
    }
    
    return 0;
}

void doit(int fd)
{
    int is_static;
    
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    struct stat sbuf;
    char filename[MAXLINE], cgiargs[MAXLINE];
    
    rio_t rio;
    rio_readinitb(&rio, fd);
    
    rio_readlineb(&rio, buf, MAXLINE);
    printf("Request headers:\n");
    printf("%s",buf);
    
    sscanf(buf,"%s %s %s", method, uri, version);
    
    if(strcmp(method,"GET"))
    {
        clienterror(fd, method, "501", "Not implemented",
                    "This tiny web server does not implement this method");
        return ;
    }
    
    read_requesthdrs(&rio);
    
    is_static=parse_uri(uri, filename, cgiargs);
    
    if(stat(filename,&sbuf))
    {
        clienterror(fd, filename, "404", "Not Found",
                    "This tiny web server does not find this file");
        return ;
    }
    
    if(is_static){
        if(!S_ISREG(sbuf.st_mode)||!(S_IRUSR&sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden",
                        "This tiny web server can not read this file");
        }
        serve_static(fd, filename, sbuf.st_size);
    }
    else
    {
        if(!S_ISREG(sbuf.st_mode)||!(S_IRUSR&sbuf.st_mode))
        {
            clienterror(fd, filename, "403", "Forbidden",
                        "This tiny web server can not run this program");
        }
        serve_dynamic(fd, filename, cgiargs);
    }
    
    
}


void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg)
{
    char buf[MAXLINE], body[MAXBUF];
    
    //构造 http回应
    sprintf(body, "<html><title> Tiny Error </title>");
    sprintf(body, "%s<body bgcolor=""ffffff"">\r\n",body);
    sprintf(body, "%s%s: %s\r\n", body, errnum, shortmsg);
    sprintf(body, "%s<p>%s: %s\r\n",body, longmsg, cause);
    sprintf(body, "%s<hr><em>The tiny web server</em>\r\n", body);
    
    //输出回应
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n");
    rio_writen(fd,buf, strlen(buf));
    sprintf(buf, "Content-length: %d\r\n\r\n",(int)strlen(body));
    rio_writen(fd, buf, strlen(buf));
    rio_writen(fd, body, strlen(body));
    
}


void read_requesthdrs(rio_t *rp)
{
    char buf[MAXLINE];
    rio_readlineb(rp, buf, MAXLINE);
    while (strcmp(buf, "\r\n")) {
        rio_readlineb(rp, buf, MAXLINE);
        printf("%s",buf);
    }
    return ;
}

int parse_uri(char *uri, char *filename, char *cgiargs)
{
    char *ptr;
    if(!strstr(uri, "cgi-bin"))
    {
        strcpy(cgiargs,"");
        strcpy(filename, ".");
        strcat(filename,uri);
        if(uri[strlen(uri)-1]=='/')
        {
            strcat(filename,"home.html");
        }
        return 1;
    }
    else{
        ptr=index(uri,'?');
        if(ptr)
        {
            strcpy(cgiargs,ptr+1);
            *ptr='\0';
        }
        else{
            strcpy(cgiargs,"");
        }
        strcpy(filename, ".");
        strcat(filename,uri);
        
    }
    
    return 0;
}

void serve_static(int fd, char *filename, int filesize)
{
    int srcfd;
    char *srcp, filetype[MAXLINE], buf[MAXBUF];
    
    get_filetype(filename, filetype);
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    sprintf(buf, "%sServer:Tiny Web Server\r\n", buf);
    sprintf(buf,"%sConnection: close\r\n", buf);
    sprintf(buf, "%sContent-length: %d\r\n", buf,filesize);
    sprintf(buf, "%sContent-type: %s\r\n\r\n", buf,filetype);
    rio_writen(fd, buf, strlen(buf));
    printf("%s",buf);
    
    srcfd=open(filename,O_RDONLY, 0);
    srcp=(char *)mmap(0, filesize, PROT_READ, MAP_PRIVATE, srcfd, 0);
    close(srcfd);
    rio_writen(fd, srcp, filesize);
    
    munmap(srcp, filesize);
}

void get_filetype(char *filename, char *filetype)
{
    if(strstr(filename,".html"))
    {
        strcpy(filetype,"text/html");
    }
}

void serve_dynamic(int fd, char *filename, char *cgiargs)
{
    char buf[MAXLINE],*emptylist[]={NULL};
    sprintf(buf,"HTTP/1.0 200 OK\r\n");
    rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Server: Tiny Web Server\r\n");
    rio_writen(fd, buf, strlen(buf));
    if(fork()==0)
    {
        setenv("QUERY_STRING",cgiargs, 1);
        dup2(fd, STDOUT_FILENO);
        execve(filename, emptylist, NULL);
    }
    wait(NULL);
}







