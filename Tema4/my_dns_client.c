#include "dns_message.h"
#include <sys/time.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <fcntl.h>

#define BUFLEN 1024

/* ----------------------------------------------------- */

struct in_addr res;
my_dns_rr reply_rr;
int preference;

unsigned char* dns_to_host(unsigned char* ptr, unsigned char* buffer, int *count)
{
	unsigned char *name = (unsigned char*) malloc(256 * sizeof(unsigned char));

	unsigned int poz = 0, offset;
	int jumped = 0;
	int i, j;

	*count = 1;
	name[0] = '\0';

	while (*ptr != 0) {
		if (*ptr >= 192) {
			offset = (*ptr) * (1<<8) + *(ptr + 1)
					- (((1<<16) - 1) - ((1<<14) -1));
			ptr = buffer + offset - 1;
			jumped = 1;
		} else {
			name[poz++] = *ptr;
		}
		ptr += 1;
		if ( !jumped ) {
			(*count)++;
		}
	}

	name[poz] = '\0';
	if (jumped) {
		(*count)++;
	}

	for (i = 0; i < (int)strlen((const char*)name); ++i) {
		poz = name[i];
		for (j = 0; j < (int)poz; ++j){
			name[i] = name[i + 1];
			++i;
		}
		name[i] = '.';
	}
	name[i] = '\0';

	return name;
}

/* ----------------------------------------------------- */

void name_to_dns(char* name, char* dns_name){

	int i = 0, j, last = 0;
	
	while(i < strlen(name)) {
		if(name[i] == '.') {
			dns_name[last] = i - last;
			for(j = 1 + last; j < 1 + i; j++){
				dns_name[j] = name[j - 1];
			}
			last = i + 1;
		}
		i++;
	}
	dns_name[last] = i - last;
	for(j = 1 + last; j < i + 1; j++) {
		dns_name[j] = name[j - 1];
	}
	
	dns_name[strlen(name) + 1] = 0;
	dns_name[strlen(name) + 2] = '\0';
}

/* ----------------------------------------------------- */

char* class_to_string(int class) {
	char* str_class = malloc( 32 * sizeof(char) );
	if(class == 1) {
		strcpy(str_class, "IN");
		return str_class;
	}
	return "";
}

/* ----------------------------------------------------- */

char* type_to_string(int type) {
	char* str_type = malloc( 32 * sizeof(char) );
	strcpy(str_type, "");
	if(type == A) strcpy(str_type,"A");
	if(type == MX) strcpy(str_type,"MX");
	if(type == NS) strcpy(str_type,"NS");
	if(type == CNAME) strcpy(str_type,"CNAME");
	if(type == SOA) strcpy(str_type,"SOA");
	if(type == TXT) strcpy(str_type,"TXT");
	return str_type;
}

/* ----------------------------------------------------- */

int get_type(char* type) {
	if( strcmp("A", type) == 0 ) return A;
	if( strcmp("NS", type) == 0 ) return NS;
	if( strcmp("CNAME", type) == 0 ) return CNAME;
	if( strcmp("MX", type) == 0 ) return MX;
	if( strcmp("SOA", type) == 0 ) return SOA;
	if( strcmp("TXT", type) == 0 ) return TXT;
	return -1;
}

/* ----------------------------------------------------- */

int main(int argc, char* argv[]) {
	char domain[256], interrogation[32];
	char buffer[BUFLEN];
	char ip_addr[10][256];

	strcpy(domain, argv[1]);
	strcpy(interrogation, argv[2]);

	/* Citire din fisier adrese servere DNS */
	
	int i = 0;
	FILE *fd;
	fd = fopen("dns_servers.conf", "r");
	while(fgets(buffer, BUFLEN, fd) != 0) {
		if(buffer[0] != '#' && buffer[0] != '\0'){
			strcpy(ip_addr[i], buffer);
			int x = strlen(ip_addr[i]);
			ip_addr[i][x - 1] = '\0';
			i++;
		}
	}
	fclose(fd);
	
	/* ------------------------------------ */

	struct sockaddr_in serv_addr;
	int sockfd;
	
	/* Conectare la socket */
	
	sockfd = socket(AF_INET, SOCK_DGRAM, 0);
	while (sockfd < 0){
  		sockfd = socket(AF_INET, SOCK_DGRAM, 0);
  	}
  	
	/* ------------------- */




	/* Deschidere logfile */
	
	FILE *g = fopen("logfile", "a");
		
	/* -------------- */

	serv_addr.sin_family = AF_INET;
	serv_addr.sin_port = htons(53);
	
	int good = 0;
	i = 0;
	
	while(!good) {
		inet_aton(ip_addr[i], &serv_addr.sin_addr.s_addr);

		/* Creere mesaj interogare */
		
		dns_header_t *my_header;
		dns_question_t *my_question;
		char* dns_name;
	
		memset(buffer, 0, sizeof(buffer));
	
		my_header = (dns_header_t*) buffer;
		memset(my_header, 0, sizeof(dns_header_t));
		my_header->id = (unsigned short) htons(getpid());
		my_header->qdcount = htons(1);
		my_header->rd = 1;
		
		dns_name = (char*) &buffer[sizeof(dns_header_t)];
		name_to_dns(domain, dns_name);
	
		my_question = (dns_question_t*) &buffer[ sizeof(dns_header_t) + strlen(domain) + 2 ];
			
		my_question->qclass = htons(1);
		my_question->qtype = htons(get_type(interrogation));
	
		/* Trimitere interogare la serverul DNS */
		
		if( sendto( sockfd, 
				  		buffer, 
				  		sizeof(dns_header_t) + strlen(domain) + 2 + sizeof(dns_question_t), 
				  		0, 
				  		(struct sockaddr*)&serv_addr , sizeof(serv_addr)) < 0)
		{
			perror("ERROR sending on socket");
			i++;
			continue;				 	
		}

		
		/* ---------------------------------------------------------------------------- */
	
		fd_set tempfd;
		FD_ZERO(&tempfd);
		FD_SET(sockfd, &tempfd);
	
		/* Asteptare raspuns */
	
		struct timeval response_time;
		response_time.tv_sec = 3;
		response_time.tv_usec = 0;
		memset(buffer, 0, BUFLEN);
		int r = select(sockfd + 1, &tempfd, NULL, NULL, &response_time);

		if(r < 0) {
			perror("select ERROR");
			i++;
			continue;
		} else if(r == 0) {
			perror("select TIMEOUT");
			i++;
			continue;
		} else {
			int size;
			memset(buffer, 0, sizeof(buffer));
			if( recvfrom(sockfd, buffer, sizeof(buffer), 0, (struct sockaddr*) &serv_addr, (socklen_t*) &size) < 0){
				perror("recv ERROR");
				i++;
				continue;
			}		

			/* Prelucrare raspuns */			
			
			my_header = (dns_header_t*) buffer;
			int no_answers = ntohs(my_header->ancount);
			int no_answers1 = ntohs(my_header->nscount);
			int no_answers2 = ntohs(my_header->arcount);
			 
			char* answer_pointer = &buffer[sizeof(dns_header_t) + strlen(domain) + 2 + sizeof(dns_question_t)];
			
			if(no_answers > 0) {
				fprintf(g, "; %s - %s %s\n", ip_addr[i], domain, interrogation);
				fprintf(g, "\n;; ANSWER SECTION:\n");
			}
			
			/* Prelucrare raspunsuri din sectiunea ANSWER */
			
			unsigned short j;
			
			for (j = 0; j < no_answers; j++){
				int count = 0;
				
				char* qname = dns_to_host(answer_pointer, buffer, &count);
				
				answer_pointer += count;
								
				dns_rr_t *answer = (dns_rr_t*) answer_pointer;
				answer_pointer += sizeof(dns_rr_t);
												
				unsigned short type = ntohs(answer->type);
				unsigned short class = ntohs(answer->class);
				
				fprintf(g, "%s %s %s ", qname, class_to_string(class), type_to_string(type));
			
				if(type == A) {
					unsigned short len = ntohs(answer->rdlength);
					reply_rr.rdata = calloc(len, sizeof(char));
				
					answer_pointer -= count;
				
					unsigned short k;
					for(k = 0; k < len; k++){
						if(answer_pointer[k] < 0) {
							fprintf(g, "%d", 256 + answer_pointer[k]);
						} else {
							fprintf(g, "%d", answer_pointer[k]);
						}
						if(k < len - 1) {
							fprintf(g, ".");
						}
					}
					fprintf(g,"\n");
				
					memset(&res, 0, sizeof(res));

					answer_pointer += 2 * count;			
				}
				
				if(type == MX) {
					unsigned short len = ntohs(answer->rdlength);					
					
					answer_pointer -= count;
					
					preference = *(answer_pointer + 1);
									
					char *ptr = &answer_pointer[2];
					int count1;
					char *mname = dns_to_host(ptr, buffer, &count1);
					answer_pointer += count1 + 2;
				
					fprintf(g, " %d  %s\n", preference, mname);
				}
				
				if(type == TXT) {
					unsigned short len = ntohs(answer->rdlength);
					reply_rr.rdata = calloc(len + 1, sizeof(char));
					
					answer_pointer -= count;
					
					unsigned short k;
					for(k = 1 ; k < len; k++) {
						reply_rr.rdata[k - 1] = answer_pointer[k];
					}
					reply_rr.rdata[len] = '\0';
					
					fprintf(g, "%s\n", reply_rr.rdata);
				}
				
				if(type == CNAME) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *canname = calloc(len + 1, sizeof(char));
					 
					int count1;			
					canname = dns_to_host(answer_pointer, buffer, &count1);
					
					answer_pointer += count1;
					
					fprintf(g, "%s\n", canname);
					
				}
				
				if(type == SOA) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *ptr1 = &answer_pointer[0];					
					int count1;
					
					char *aname = dns_to_host(answer_pointer, buffer, &count1);			
					
					answer_pointer += count1;
					
					char *amail = dns_to_host(answer_pointer, buffer, &count1);
					
					answer_pointer += count1;
					
					int serial = 0;
					
					unsigned short k;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						serial = serial * 256 + x;
					}
					
					answer_pointer += 4;
					
					int refresh = 0;
					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						refresh = refresh * 256 + x;
					}
					
					answer_pointer += 4;
					
					int retry = 0;
					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						retry = retry * 256 + x;
					}
					
					answer_pointer += 4;
					
					int expiration = 0;
					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						expiration = expiration * 256 + x;
					}
					
					answer_pointer += 4;
					
					int minimum = 0;
					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						minimum = minimum * 256 + x;
					}
					
					fprintf(g, "%s %s %d %d %d %d %d\n", aname, amail, serial, refresh, retry, expiration, minimum);
					
					answer_pointer += 4;
					
				}
				
				if(type == NS) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					
					char *nsname = calloc(len, sizeof(char));
					
					int count2;
					nsname = dns_to_host(answer_pointer, buffer, &count2);
					
					answer_pointer += count2;
					
					fprintf(g, "%s\n", nsname);
				}
				
			}
			
			if(no_answers1 > 0) {
				fprintf(g, "; %s - %s %s\n", ip_addr[i], domain, interrogation);
				fprintf(g, "\n;; AUTHORITY SECTION:\n");
			}
			
			/* Prelucrare raspunsuri din sectiunea AUTHORITY */
			
			for (j = 0; j < no_answers1; j++){
				int count = 0;
				
				char* qname = dns_to_host(answer_pointer, buffer, &count);				
				answer_pointer += count;
								
				dns_rr_t *answer = (dns_rr_t*) answer_pointer;
				answer_pointer += sizeof(dns_rr_t);
												
				unsigned short type = ntohs(answer->type);
				unsigned short class = ntohs(answer->class);
				
				fprintf(g, "%s %s %s ", qname, class_to_string(class), type_to_string(type));
			
				if(type == A) {
					unsigned short len = ntohs(answer->rdlength);
				
					answer_pointer -= count;
				
					unsigned short k;
					for(k = 0; k < len; k++){
						if(answer_pointer[k] < 0) {
							fprintf(g, "%d", 256 + answer_pointer[k]);
						} else {
							fprintf(g, "%d", answer_pointer[k]);
						}
						if(k < len - 1) {
							fprintf(g, ".");
						}
					}
					fprintf(g,"\n");
				
					memset(&res, 0, sizeof(res));

					answer_pointer += 2 * count;			
				}
				
				if(type == MX) {
					unsigned short len = ntohs(answer->rdlength);					
					
					answer_pointer -= count;
					
					preference = *(answer_pointer + 1);
									
					char *ptr = &answer_pointer[2];
					int count1;
					char *mname = dns_to_host(ptr, buffer, &count1);
					answer_pointer += count1 + 2;
				
					fprintf(g, " %d  %s\n", preference, mname);
				}
				
				if(type == TXT) {
					unsigned short len = ntohs(answer->rdlength);
					reply_rr.rdata = calloc(len + 1, sizeof(char));
					
					answer_pointer -= count;
					
					unsigned short k;
					for(k = 1 ; k < len; k++) {
						reply_rr.rdata[k - 1] = answer_pointer[k];
					}
					reply_rr.rdata[len] = '\0';
					
					fprintf(g, "%s\n", reply_rr.rdata);
				}
				
				if(type == CNAME) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *canname = calloc(len + 1, sizeof(char));					 
					int count1;			
					canname = dns_to_host(answer_pointer, buffer, &count1);
					
					answer_pointer += count1;
					
					fprintf(g, "%s\n", canname);
					
				}
				
				if(type == SOA) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *ptr1 = &answer_pointer[0];					
					int count1;					
					char *aname = dns_to_host(answer_pointer, buffer, &count1);			
					
					answer_pointer += count1;
					
					char *amail = dns_to_host(answer_pointer, buffer, &count1);					
					answer_pointer += count1;
					
					unsigned short k;
					int serial = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						serial = serial * 256 + x;
					}
					
					answer_pointer += 4;
					
					int refresh = 0;					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						refresh = refresh * 256 + x;
					}
					
					answer_pointer += 4;
					
					int retry = 0;					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						retry = retry * 256 + x;
					}
					
					answer_pointer += 4;
					
					int expiration = 0;					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						expiration = expiration * 256 + x;
					}
					
					answer_pointer += 4;
					
					int minimum = 0;					
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						minimum = minimum * 256 + x;
					}
					
					fprintf(g, "%s %s %d %d %d %d %d\n", aname, amail, serial, refresh, retry, expiration, minimum);
					
					answer_pointer += 4;
					
				}
				
				if(type == NS) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *nsname = calloc(len, sizeof(char));					
					int count2;
					nsname = dns_to_host(answer_pointer, buffer, &count2);
					
					answer_pointer += count2;
					
					fprintf(g, "%s\n", nsname);
				}
				
			}
			
			if(no_answers2 > 0) {
				fprintf(g, "; %s - %s %s\n", ip_addr[i], domain, interrogation);
				fprintf(g, "\n;; ADDITIONAL SECTION:\n");
			}
			
			/* Prelucrare raspunsuri din sectiunea ADDITIONAL */
			
			for (j = 0; j < no_answers2; j++){
				int count = 0;
				
				char* qname = dns_to_host(answer_pointer, buffer, &count);				
				answer_pointer += count;
								
				dns_rr_t *answer = (dns_rr_t*) answer_pointer;
				answer_pointer += sizeof(dns_rr_t);
												
				unsigned short type = ntohs(answer->type);
				unsigned short class = ntohs(answer->class);
				
				fprintf(g, "%s %s %s ", qname, class_to_string(class), type_to_string(type));
			
				if(type == A) {
					unsigned short len = ntohs(answer->rdlength);
				
					answer_pointer -= count;
				
					unsigned short k;
					for(k = 0; k < len; k++){
						if(answer_pointer[k] < 0) {
							fprintf(g, "%d", 256 + answer_pointer[k]);
						} else {
							fprintf(g, "%d", answer_pointer[k]);
						}
						if(k < len - 1) {
							fprintf(g, ".");
						}
					}
					fprintf(g,"\n");
				
					memset(&res, 0, sizeof(res));

					answer_pointer += 2 * count;			
				}
				
				if(type == MX) {
					unsigned short len = ntohs(answer->rdlength);					
					
					answer_pointer -= count;
					
					preference = *(answer_pointer + 1);
									
					char *ptr = &answer_pointer[2];
					int count1;
					char *mname = dns_to_host(ptr, buffer, &count1);
					answer_pointer += count1 + 2;
				
					fprintf(g, " %d  %s\n", preference, mname);
				}
				
				if(type == TXT) {
					unsigned short len = ntohs(answer->rdlength);
					reply_rr.rdata = calloc(len + 1, sizeof(char));
					
					answer_pointer -= count;
					
					unsigned short k;
					for(k = 1 ; k < len; k++) {
						reply_rr.rdata[k - 1] = answer_pointer[k];
					}
					reply_rr.rdata[len] = '\0';
					
					fprintf(g, "%s\n", reply_rr.rdata);
				}
				
				if(type == CNAME) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *canname = calloc(len + 1, sizeof(char));					 
					int count1;			
					canname = dns_to_host(answer_pointer, buffer, &count1);
					
					answer_pointer += count1;
					
					fprintf(g, "%s\n", canname);
					
				}
				
				if(type == SOA) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
					
					char *ptr1 = &answer_pointer[0];					
					int count1;
					
					char *aname = dns_to_host(answer_pointer, buffer, &count1);								
					answer_pointer += count1;
					
					char *amail = dns_to_host(answer_pointer, buffer, &count1);					
					answer_pointer += count1;
					
					unsigned short k;
					int serial = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						serial = serial * 256 + x;
					}
					
					answer_pointer += 4;
					
					int refresh = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						refresh = refresh * 256 + x;
					}
					
					answer_pointer += 4;
					
					int retry = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						retry = retry * 256 + x;
					}
					
					answer_pointer += 4;
					
					int expiration = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						expiration = expiration * 256 + x;
					}
					
					answer_pointer += 4;
					
					int minimum = 0;
					for(k = 0 ; k < 4; k++) {
						unsigned char x = *(answer_pointer + k);
						minimum = minimum * 256 + x;
					}
					
					fprintf(g, "%s %s %d %d %d %d %d\n", aname, amail, serial, refresh, retry, expiration, minimum);
					
					answer_pointer += 4;
					
				}
				
				if(type == NS) {
					unsigned short len = ntohs(answer->rdlength);
					
					answer_pointer -= count;
	
					char *nsname = calloc(len, sizeof(char));					
					int count2;
					nsname = dns_to_host(answer_pointer, buffer, &count2);
					
					answer_pointer += count2;
					
					fprintf(g, "%s\n", nsname);
				}
			}
			
			
		}
		good = 1;
	}
	fprintf(g, "\n");
	fclose(g);
	return 0;
}
