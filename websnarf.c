/**
 * serveur.c
 */

// On importe des fichiers.h nécéssaires à l'application
#include <stdio.h>      // Fichier contenant les en-têtes des fonctions standard d'entrées/sorties 
#include <stdlib.h>     // Fichier contenant les en-têtes de fonctions standard telles que malloc()
#include <string.h>     // Fichier contenant les en-têtes de fonctions standard de gestion de chaînes de caractères 
#include <unistd.h>     // Fichier d'en-têtes de fonctions de la norme POSIX (dont gestion des fichiers : write(), close(), ...)
#include <sys/types.h>      // Fichier d'en-têtes contenant la définition de plusieurs types et de structures primitifs (système)
#include <sys/socket.h>     // Fichier d'en-têtes des fonctions de gestion de sockets
#include <netinet/in.h>     // Fichier contenant différentes macros et constantes facilitant l'utilisation du protocole IP
#include <netdb.h>      // Fichier d'en-têtes contenant la définition de fonctions et de structures permettant d'obtenir des informations sur le réseau (gethostbyname(), struct hostent, ...)
#include <memory.h>     // Contient l'inclusion de string.h (s'il n'est pas déjà inclus) et de features.h
#include <errno.h>      // Fichier d'en-têtes pour la gestion des erreurs (notamment perror())
#include <getopt.h>     // Gestion des paramètres de lancement dans le terminal
#include <time.h>       // Affichage du temps
#include <arpa/inet.h>  //convertion des ip pour affichage
#include <sys/stat.h>  //création de dossiers

#define P 80
#define LOG(X) fputs(X, fp), fflush(fp)
  // On définit les variables nécéssaires
  int newsockfd, sock, opt;
  int debug = 0;
  int alarmtime = 5;
  int maxline = 40;
  char* logfile = "";
  char* savedir = "";
  int apache = 0;
  int fullnames = 0;
  u_short port = P;
  FILE *fp, *fp2;

  char* help = "usage: ./websnarf [options]\n"
        "--timeout <n> or -t <n>    wait at most <n> seconds on a read (default $alarmtime)\n"
        "--log <FILE> or -l <FILE>  append output to FILE (default stdout only)\n"
        "--port <n> or -p <n>       listen on TCP port <n> (default $port/tcp)\n"
        "--max <n> or -m <n>        save at most <n> chars of request (default $maxline chars)\n"
        "--savedir <DIR> or -s <DIR>   save all incoming headers into DIR\n"
        "--debug or -d              turn on a bit of debugging (mainly for developers)\n"
        "--apache or -a             logs are in Apache style\n"
        "--fullnames or -f          display fullname instead of IP adress\n"
        "--help or -h               show this listing\n";

/*
creersock

Il s'agit de la fonction qui va permettre la création d'une socket.
Elle est utilisée dans la fonction main().
Elle prend en paramètre un entier court non signé, qui est le numéro de port,
nécéssaire à l'opération bind().
Cette fonction renvoie un numéro qui permet d'identifier la socket nouvellement créée
(ou la valeur -1 si l'opération a échouée).
*/

int creersock( u_short port) {

  // On crée deux variables entières
  int sock, retour;

  // On crée une variable adresse selon la structure sockaddr_in (la structure est décrite dans sys/socket.h)
  struct sockaddr_in adresse;
  
  /*
  La ligne suivante décrit la création de la socket en tant que telle.
  La fonction socket prend 3 paramètres : 
  - la famille du socket : la plupart du temps, les développeurs utilisent AF_INET pour l'Internet (TCP/IP, adresses IP sur 4 octets)
    Il existe aussi la famille AF_UNIX, dans ce mode, on ne passe pas des numéros de port mais des noms de fichiers.
  - le protocole de niveau 4 (couche transport) utilisé : SOCK_STREAM pour TCP, SOCK_DGRAM pour UDP, ou enfin SOCK_RAW pour générer
    des trames directement gérées par les couches inférieures.
  - un numéro désignant le protocole qui fournit le service désiré. Dans le cas de socket TCP/IP, on place toujours ce paramètre a 0 si on utilise le protocole par défaut.
  */


  sock = socket(AF_INET, SOCK_STREAM, 0);

  // Si le code retourné n'est pas un identifiant valide (la création s'est mal passée), on affiche un message sur la sortie d'erreur, et on renvoie -1
  if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) < 0) {
    perror ("ERREUR OUVERTURE");
    return(-1);
  }

  //On initialise le timeout
  /*struct timeval timeout;      
    timeout.tv_sec = alarmtime;
    timeout.tv_usec = 0;

    if (setsockopt (sock, SOL_SOCKET, SO_RCVTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
      perror("setsockopt failed\n");
      return(-1);
    }

    if (setsockopt (sock, SOL_SOCKET, SO_SNDTIMEO, (char *)&timeout, sizeof(timeout)) < 0){
      perror("setsockopt failed\n");
      return(-1);
    }*/
  
  // On complète les champs de la structure sockaddr_in : 
  // La famille du socket, AF_INET, comme cité précédement
  adresse.sin_family = AF_INET;

  /* Le port auquel va se lier la socket afin d'attendre les connexions clientes. La fonction htonl()  
  convertit  un  entier  long  "htons" signifie "host to network long" conversion depuis  l'ordre des bits de l'hôte vers celui du réseau.
  */
  adresse.sin_port = htons(port);

  /* Ce champ désigne l'adresse locale auquel un client pourra se connecter. Dans le cas d'une socket utilisée 
  par un serveur, ce champ est initialisé avec la valeur INADDR_ANY. La constante INADDR_ANY utilisée comme 
  adresse pour le serveur a pour valeur 0.0.0.0 ce qui correspond à une écoute sur toutes les interfaces locales disponibles.
    
  */
  adresse.sin_addr.s_addr=INADDR_ANY;
  
  /*
  bind est utilisé pour lier la socket : on va attacher la socket crée au début avec les informations rentrées dans
  la structure sockaddr_in (donc une adresse et un numéro de port).
   Ce bind affecte une identité à la socket, la socket représentée par le descripteur passé en premier argument est associée 
    = l'adresse passée en seconde position. Le dernier argument représente la longueur de l'adresse. 
    Ce qui a pour but de  rendre la socket accessible de l'extérieur (par getsockbyaddr)
  */
  retour = bind (sock,(struct sockaddr *)&adresse,sizeof(adresse));
  
  // En cas d'erreur lors du bind, on affiche un message d'erreur et on renvoie -1
  if (retour<0) {
    perror ("IMPOSSIBLE DE NOMMER LA SOCKET");
    return(-1);
  }

  // Au final, on renvoie sock, qui contient l'identifiant à la socket crée et attachée.
  return (sock);
}

char* formatRequete(char* request){
  char* arr[maxline];
  char delim[] = "\n";
	char *ptr = strtok(request, delim);
  int i = 0;

  while(ptr != NULL){
    arr[i++] = ptr; 
		ptr = strtok(NULL, delim);
	}
  return *arr;
}

char* remplaceString(const char* s, const char* oldS, const char* newS){ 
    char* result; 
    int i, cnt = 0; 
    int newSlen = strlen(newS); 
    int oldSlen = strlen(oldS); 
  
    for (i = 0; s[i] != '\0'; i++) { 
        if (strstr(&s[i], oldS) == &s[i]) { 
            cnt++; 
  
            i += oldSlen - 1; 
        } 
    } 
  
    result = (char*)malloc(i + cnt * (newSlen - oldSlen) + 1); 
  
    i = 0; 
    while (*s) { 
        if (strstr(s, oldS) == s) { 
            strcpy(&result[i], newS); 
            i += newSlen; 
            s += oldSlen; 
        } 
        else
            result[i++] = *s++; 
    } 
  
    result[i] = '\0'; 
    return result; 
} 

//Création d'un nouveau processus pour chaque client
void* serverfork(void* newsockfd1){
  char msg [BUFSIZ];
  char display[8300];
  int newsockfd = *(int *) newsockfd1;
  int hours, minutes, seconds, day, month, year;
  

  struct timeval timeout;
  timeout.tv_sec = alarmtime;
  timeout.tv_usec = 0;

  fd_set read_fds;
  FD_ZERO(&read_fds);

  struct sockaddr_in client_addr, host_addr, serv_addr;
  int clnt_sock_err, clnt_peer_err;
  socklen_t serv_len = sizeof(serv_addr);

  clnt_peer_err = getpeername(newsockfd, (struct sockaddr *)&client_addr, &serv_len);
  clnt_sock_err = getsockname(newsockfd, (struct sockaddr *)&host_addr, &serv_len);

  if (clnt_peer_err || clnt_sock_err){
    perror("Erreur : ne peut pas récupérer les infos de la connexion");
    exit(EXIT_FAILURE);
  }


  char clientName[30];
  char hostName[30];

  

  if(fork() == 0){
    
    // On créé le timeout avec la fonction select
    FD_SET(newsockfd, &read_fds);
    switch (select(newsockfd+1, &read_fds, NULL, NULL, &timeout)) {
    case -1:
      perror("ERREUR SELECT");
      close(newsockfd);
      exit(EXIT_FAILURE);
    case 0:
        strcpy(msg, "[timeout]");
    }

    if(fullnames){
      if (getnameinfo((struct sockaddr*)&client_addr, sizeof(client_addr), clientName, sizeof(clientName),0,0,0) !=0 
        || gethostname(hostName, sizeof(hostName)) !=0) {
        
        perror("Erreur IP client vers nom");
      }
    }
    else{
      strcpy(clientName, inet_ntoa(client_addr.sin_addr));
      strcpy(hostName, inet_ntoa(host_addr.sin_addr));
    }
    

    time_t now;
    time(&now);
    struct tm *local = localtime(&now);
    hours = local->tm_hour;
    minutes = local->tm_min;
    seconds = local->tm_sec;
    day = local->tm_mday;
    month = local->tm_mon + 1;
    year = local->tm_year + 1900; 

    //taille du message reçu;
    size_t s = 0;

    // On lit le message envoyé par la socket de communication s'il existe. 
    //  msg contiendra la chaine de caractères envoyée par le réseau,
    // s le code d'erreur de la fonction recv. -1 si pb et sinon c'est le nombre de caractères lus

    if(FD_ISSET(newsockfd, &read_fds)){
      
      if(debug) printf("client ready to read, now reading\n");
      
      s = recv(newsockfd, msg, 1024, 0);

      if(s == -1){
        perror("Erreur de lecture du socket");
        close(newsockfd);
        exit(EXIT_FAILURE);
      }
      else{

        if(debug) {
          printf("  got read of [%zu]\n", s);
          char* newline;
          newline = remplaceString(msg, "\r", "<CR>");
          newline = remplaceString(newline, "\n", "<LF>");
          printf("  [%s]\n", newline);
          printf("  Finished read loop: request = %zu bytes\n", s);
        }
        
        if (s == 0){ // message vide
          strcpy(msg, "[empty]");
        }
        else{
          // Si le code d'erreur est bon, on affiche le message.
          
          //si on a définit un dossier de sauvegarde
          if (savedir && savedir[0]){
            char filepath[256];

            sprintf(filepath, "%s/%s-%s", savedir, clientName, hostName);
            fp2 = fopen(filepath, "w");
            fputs(msg, fp2); fflush(fp2);
          }

          msg[s] = 0;
          strcpy(msg, &formatRequete(msg)[0]);
          msg[strcspn(msg, "\r\n")] = 0;
        }
      }
    }

    if(apache){
      char date[30];
      strftime(date, sizeof(date), "%d/%b/%G:%T %z", local);
      sprintf(display, "%s - - [%s] \"%s\" 404 %zu\n", inet_ntoa(client_addr.sin_addr), date, msg, s);
    }
    else{
      sprintf(display, "%02d/%02d/%d %02d:%02d:%02d\t %s:%d\t -> %s:%d : %s\n", 
        day, month, year, hours, minutes, seconds, 
        clientName, ntohs(client_addr.sin_port),
        hostName, ntohs(host_addr.sin_port),
        msg);
    }

    printf("%s\n", display);

    if (logfile && logfile[0])
        LOG(display);
  }

  // On referme la socket de communication
  close(newsockfd);
  return 0;
}


int main (int argc, char *argv[]) {

  static struct option long_options[] = {
    {"help", no_argument, 0, 'h' },
    {"apache", no_argument, 0, 'a' },
    {"debug", no_argument, 0, 'd' },
    {"fullnames", no_argument, 0, 'f' },
    {"port", required_argument, 0, 'p' },
    {"timeout", required_argument, 0, 't' },
    {"log", required_argument, 0, 'l' },
    {"max", required_argument, 0, 'm' },
    {"savedir", required_argument, 0, 's' },
    {NULL, 0, NULL, 0}
  };

  while ((opt = getopt_long(argc, argv, "hadfp:t:l:m:s:", long_options ,NULL)) != -1) {
    switch (opt) {
      case 'h':
        fprintf(stderr, help, argv[0]);
        exit(EXIT_FAILURE);
        break;
      case 'a':
        apache = 1;
        break;
      case 'd':
        debug = 1;
        break;  
      case 'f':
        fullnames = 1;
        break;  
      case 'p':
        port = atoi(optarg);
        break;
      case 't':
        alarmtime = atoi(optarg);
        break;
      case 'l':
        logfile = optarg;
        fp = fopen(logfile, "w");
        fprintf(fp, "# Now listening on port %d\n", port);
        fflush(fp);
        break;
      case 'm':
        maxline = atoi(optarg);
        break;
      case 's':
        savedir = optarg;
        struct stat st = {0};
        if (stat(savedir, &st) == -1)
          mkdir(savedir, 0700);
        break;
      default: /* '?' */
        fprintf(stderr, "Arguments invalide, --help ou -h pour afficher l'aide\n");
        exit(EXIT_FAILURE);
    }
  }
  // On crée la socket
  sock = creersock (port);

  /*
  listen
	permet de dimensionner la taille de la file d'attente.
   On passe en paramètre la socket qui va écouter, et un entier qui désigne le nombre de connexions simultanées autorisées (backlog)
  */
  listen (sock,5);

  /* La fonction accept permet d'accepter une connexion à notre socket par un client. On passe en paramètres la socket serveur d'écoute à demi définie.
  newsockfd contiendra l'identifiant de la socket de communication. newsockfd est la valeur de retour de la primitive accept. 
 C'est une socket d'échange de messages : elle est entièrement définie.
 On peut préciser aussi la structure et la taille de sockaddr associée 
  mais ce n'est pas obligatoire et ici on a mis le pointeur NULL
  */

//pour gérer plusieurs clients itérativement on fait une boucle qui va créer un nouveau socket de communication à la fin de la communication avec un client.
  printf("websnarf listening on port %d (timeout=%d secs)\n", port, alarmtime);
  while(1){

    newsockfd = accept (sock, (struct sockaddr *) 0, (unsigned int*) 0);

    // Si l'accept se passe mal, on quitte le programme en affichant un message d'erreur.
    if (newsockfd == -1) {
      perror("Erreur accept");
      return EXIT_FAILURE;
    }
    struct sockaddr_in client_addr;
    int clnt_peer_err;
    socklen_t serv_len = sizeof(client_addr);

    clnt_peer_err = getpeername(newsockfd, (struct sockaddr *)&client_addr, &serv_len);

    if (clnt_peer_err){
      perror("Erreur : ne peut pas récupérer les infos de la connexion");
      return EXIT_FAILURE;
    }

    if(debug)
      printf("--> accepted connection from %s\n", inet_ntoa(client_addr.sin_addr));

    serverfork((void *) &newsockfd);
  }

    // On referme la socket d'écoute et les fichiers ouverts.
    close(sock);

    if (logfile && logfile[0])
      fclose(fp);

    if (savedir && savedir[0])
      fclose(fp2);
    return EXIT_SUCCESS;
  }