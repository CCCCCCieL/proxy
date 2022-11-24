#include <stdio.h>
#include "csapp.h"
#include "sbuf.h"
#include "cache.h"


/* Recommended max cache and object sizes */
#define MAX_CACHE_SIZE 1049000
#define MAX_OBJECT_SIZE 102400

/* Decide the number of pthread */
#define PTHREAD_NUMBER 8
#define sbuf_SIZE 32

static const char *user_agent_hdr = "User-Agent: Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/105.0.0.0 Safari/537.36\r\n";


int parse_uri(char *uri, char *hostname, char *port,char *path, char *version);
void doit(int fd);
void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg);
void *thread(void *vargp);
void sbuf_insert(sbuf_t *sp, int item);
void *load_and_cache(int fd, char *hostname, char *port, char *path, char *filename, char *method);


sbuf_t sp;
cache_list_t cache;
char eviction[];

int main(int argc, char **argv){
    // nessary variables
    int listenfd, connfd;
    char hostname[MAXLINE], port[6];
    socklen_t clientlen;
    struct sockaddr_storage clientaddr;

    /* Check command line args */
    if (argc != 2&&argc != 3) {
        fprintf(stderr, "usage: %s <port> or <port> <eviction policies>\n", argv[0]);
        exit(1);
    }

    if (argc == 2) {
        strcpy(eviction, "NAN");
    }

    if (argc == 3) {
        if (!strcmp(argv[2],"LRU"))
        {
            strcpy(eviction, "LRU");
        } else if (!strcmp(argv[2],"LFU"))
        {
            strcpy(eviction, "LFU");
        } else {
            fprintf(stderr, "usage: %s <port> <eviction policies>\n", argv[0]);
            exit(1);
        }
        
    }

    // start listening 
    listenfd = Open_listenfd(argv[1]);

    // initial 
    sbuf_init(&sp, sbuf_SIZE);
    // create threads
    pthread_t tid;
    for (size_t i=0;i<PTHREAD_NUMBER;i++) {
        Pthread_create(&tid, NULL, thread, NULL);
    }

    // initial the space to record the list of cache
    create_cache_space(&cache);

    while (1) {
	    clientlen = sizeof(clientaddr);
	    connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen); 
        Getnameinfo((SA *) &clientaddr, clientlen, hostname, MAXLINE, port, MAXLINE, 0);
        printf("Accepted connection from (%s, %s)\n", hostname, port);
        // add to buffer for waiting
        sbuf_insert(&sp, connfd);
    }

    return 0;
}

/* Thread routine */
void *thread(void *vargp) 
{  
    // divide out
    Pthread_detach(pthread_self());  
    while (1) {
        int fd = sbuf_remove(&sp);
        doit(fd);
        Close(fd);
    }    
}


void doit(int fd) 
{
    int have_uri;
    rio_t rio;
    char buf[MAXLINE], method[MAXLINE], uri[MAXLINE], version[MAXLINE];
    char filename[MAXLINE], hostname[MAXLINE], port[MAXLINE], path[MAXLINE];


    /* Read request line and headers */
    Rio_readinitb(&rio, fd);
    if (!Rio_readlineb(&rio, buf, MAXLINE))  
        return;

    printf("%s", buf);
    sscanf(buf, "%s %s %s", method, uri, version);
    if (strcasecmp(method, "GET")) {   
        clienterror(fd, method, "501", "Not Implemented", "Proxy does not implement this method");
        return;
    }                           
    
    /* Parse URI from GET request */
    printf("read URI\n");
    strcpy(filename,uri);
    have_uri = parse_uri(uri, hostname, port, path,version); 

    if (have_uri < 0) {
        clienterror(fd, method, "ERROR", "URI not valid", "No page found :(");
        return;
    };

    single_cache_t *evercached = cache_search(&cache,hostname,port,path);
    if (!evercached)
    {
        // perform as a client
        load_and_cache(fd,hostname,port,path,filename,method);
    } else {
        // perform as a server
        Rio_writen(fd, evercached->cache, evercached->size);
        if (!strcmp(eviction,"LRU")) {
            // make it be the first, then last one is the LRU one
            pick_cache(&cache, evercached);
        }
        evercached->used += 1;
    }
              
}


void *load_and_cache(int fd, char *hostname, char *port, char *path, char *filename, char *method)
{
    // open client
    int clientfd = open_clientfd(hostname, port);
    // if page not found (wrong hostname/path)
    if (clientfd<0) {
        clienterror(fd, method, "ERROR", "Cannot find the corresponding page", "No page found :(");
        return;
    }

    rio_t cli_rio;
    Rio_readinitb(&cli_rio, clientfd);

    char buf[MAXLINE];
    // first line
    sprintf(buf, "%s %s HTTP/1.0\r\n", method, filename);

    // http header
    sprintf(buf,"%sHost: %s\r\n",buf,hostname);
    sprintf(buf,"%sUser-Agent: %s\r\n",buf,user_agent_hdr);
    sprintf(buf,"%sConnection: close\r\n",buf);
    sprintf(buf,"%sProxy-Connection: close\r\n",buf);
    Rio_writen(clientfd,buf,strlen(buf));

    char *cacheAdr_base = Malloc(MAX_OBJECT_SIZE);
    char *cacheAdr = cacheAdr_base;
    ssize_t n=0;
    ssize_t totalNumber = 0;
    while ((n = Rio_readnb(&cli_rio, buf, MAXLINE))) {
        if (n+totalNumber<MAX_OBJECT_SIZE)
        {
            memcpy(cacheAdr,buf,n);
            cacheAdr += n;
        }
        totalNumber += n;
        Rio_writen(fd, buf, n);
    }

    
    if (totalNumber<MAX_OBJECT_SIZE)
    {
        // if can be cashed
        single_cache_t *new = cache_new(hostname,port,path,cacheAdr_base,totalNumber);
        cache_add(&cache, new, eviction);
    }

    Free(cacheAdr_base);

}


int parse_uri(char *uri, char *hostname, char *port,char *path, char *version) 
{
    char *http = strstr(uri, "http://");
    if (http!=NULL) {
        // add the length to the uri address, then uri will be www...
        uri += strlen("http://");
        printf("uri: %s, \n", uri);

        char* adr = index(uri,'/');
        if (adr!=0){
            printf("has path\n");
            strcpy(path,adr);
            *adr = '\0'; // make adr the last char of the string
        } else {
            printf("no path\n");
            strcpy(path,"/");
        }

        // test if port is given
        adr = index(uri,':');
        if (adr) {
            printf("has port\n");
            strcpy(port,adr+1);
            *adr = '\0';
        } else {
            strcpy(port,"80");
        }
        
        // copy the strings to hostname, rest part are all hostname
        strcpy(hostname,uri);

        adr = version+5;
        strcpy(adr,"1.0");
 
        printf("hostname: %s, ", hostname);  
        printf("port: %s, ", port);  
        printf("path: %s, ", path);
        printf("version: %s\n", version);

        return 0;

    }
    return -1;
}

void clienterror(int fd, char *cause, char *errnum, char *shortmsg, char *longmsg) 
{
    char buf[MAXLINE];

    /* Print the HTTP response headers */
    sprintf(buf, "HTTP/1.0 %s %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "Content-type: text/html\r\n\r\n");
    Rio_writen(fd, buf, strlen(buf));

    /* Print the HTTP response body */
    sprintf(buf, "<html><title>Proxy Error</title>");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<body bgcolor=""ffffff"">\r\n");
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "%s: %s\r\n", errnum, shortmsg);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<p>%s: %s\r\n", longmsg, cause);
    Rio_writen(fd, buf, strlen(buf));
    sprintf(buf, "<hr><em>The Proxy server</em>\r\n");
    Rio_writen(fd, buf, strlen(buf));
}
