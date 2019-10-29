#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <unistd.h>

#define HEADER_SIZE 12
#define FOOTER_SIZE 4 // technically part of the query. type and class of the query.
#define RES_MAX 65536
#define RR_COUNT_LOCATION 6
#define CNAME 5

typedef unsigned int dns_rr_ttl;
typedef unsigned short dns_rr_type;
typedef unsigned short dns_rr_class;
typedef unsigned short dns_rdata_len;
typedef unsigned short dns_rr_count;
typedef unsigned short dns_query_id;
typedef unsigned short dns_flags;

typedef struct
{
	char *name;
	dns_rr_type type;
	dns_rr_class class;
	dns_rr_ttl ttl;
	dns_rdata_len rdata_len;
	unsigned char *rdata;
} dns_rr;

struct dns_answer_entry;
struct dns_answer_entry
{
	char *value;
	struct dns_answer_entry *next;
};
typedef struct dns_answer_entry dns_answer_entry;

void free_answer_entries(dns_answer_entry *ans)
{
	dns_answer_entry *next;
	while (ans != NULL)
	{
		next = ans->next;
		free(ans->value);
		free(ans);
		ans = next;
	}
}

void print_bytes(unsigned char *bytes, int byteslen)
{
	int i, j, byteslen_adjusted;
	unsigned char c;

	if (byteslen % 8)
	{
		byteslen_adjusted = ((byteslen / 8) + 1) * 8;
	}
	else
	{
		byteslen_adjusted = byteslen;
	}
	for (i = 0; i < byteslen_adjusted + 1; i++)
	{
		if (!(i % 8))
		{
			if (i > 0)
			{
				for (j = i - 8; j < i; j++)
				{
					if (j >= byteslen_adjusted)
					{
						printf("  ");
					}
					else if (j >= byteslen)
					{
						printf("  ");
					}
					else if (bytes[j] >= '!' && bytes[j] <= '~')
					{
						printf(" %c", bytes[j]);
					}
					else
					{
						printf(" .");
					}
				}
			}
			if (i < byteslen_adjusted)
			{
				printf("\n%02X: ", i);
			}
		}
		else if (!(i % 4))
		{
			printf(" ");
		}
		if (i >= byteslen_adjusted)
		{
			continue;
		}
		else if (i >= byteslen)
		{
			printf("   ");
		}
		else
		{
			printf("%02X ", bytes[i]);
		}
	}
	printf("\n");
}

void canonicalize_name(char *name)
{
	/*
	 * Canonicalize name in place.  Change all upper-case characters to
	 * lower case and remove the trailing dot if there is any.  If the name
	 * passed is a single dot, "." (representing the root zone), then it
	 * should stay the same.
	 *
	 * INPUT:  name: the domain name that should be canonicalized in place
	 */

	int namelen, i;

	// leave the root zone alone
	if (strcmp(name, ".") == 0)
	{
		return;
	}

	namelen = strlen(name);
	// remove the trailing dot, if any
	if (name[namelen - 1] == '.')
	{
		name[namelen - 1] = '\0';
	}

	// make all upper-case letters lower case
	for (i = 0; i < namelen; i++)
	{
		if (name[i] >= 'A' && name[i] <= 'Z')
		{
			name[i] += 32;
		}
	}
}

// originally defined to returned an int, I modified it.
void name_ascii_to_wire(char *name, unsigned char *wire)
{
	/*
	 * Convert a DNS name from string representation (dot-separated labels)
	 * to DNS wire format, using the provided byte array (wire).  Return
	 * the number of bytes used by the name in wire format.
	 *
	 * INPUT:  name: the string containing the domain name
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *              wire-formatted name should be constructed
	 * OUTPUT: the length of the wire-formatted name.
	 */

	char name_cpy[strlen(name) + 1];
	strcpy(name_cpy, name);
	const char delim[] = {'.'};

	int bytes_copied = 0;
	char *token = strtok(name_cpy, delim);
	while (token)
	{
		int len = strlen(token);
		wire[bytes_copied] = len;
		bytes_copied++;

		memcpy(wire + bytes_copied, token, len);
		bytes_copied += len;
		token = strtok(NULL, delim);
	}
	wire[bytes_copied] = 0; //add the null char
}

// char *name_ascii_from_wire(unsigned char *wire, int *indexp)
char *name_ascii_from_wire(unsigned char *wire, int index)
{
	/*
	 * Extract the wire-formatted DNS name at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return its string
	 * representation (dot-separated labels) in a char array allocated for
	 * that purpose.  Update the value pointed to by indexp to the next
	 * value beyond the name.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp, a pointer to the index in the wire where the
	 *              wire-formatted name begins
	 * OUTPUT: a string containing the string representation of the name,
	 *              allocated on the heap.
	 */
	char *name = malloc(RES_MAX);
	int bytes_copied = 0;

	while (wire[index]) //not null char
	{
		if (wire[index] >= 0xC0)
		{
			index = wire[index + 1];
			continue;
		}
		unsigned char size_label = wire[index];
		memcpy(name + bytes_copied, wire + index, size_label + 1);
		name[bytes_copied] = '.';
		bytes_copied += size_label + 1;
		index += size_label + 1;
	}
	name[bytes_copied] = 0;
	char *return_me = malloc(strlen(name)); //lose the first size_label
	strcpy(return_me, name + 1);
	free(name);

	canonicalize_name(return_me);
	return return_me; //get rid of the beginning size label
}

dns_rr rr_from_wire(unsigned char *wire, int *indexp, int query_only)
{
	/*
	 * Extract the wire-formatted resource record at the offset specified by
	 * *indexp in the array of bytes provided (wire) and return a
	 * dns_rr (struct) populated with its contents. Update the value
	 * pointed to by indexp to the next value beyond the resource record.
	 *
	 * INPUT:  wire: a pointer to an array of bytes
	 * INPUT:  indexp: a pointer to the index in the wire where the
	 *              wire-formatted resource record begins
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are extracting a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the resource record (struct)
	 */
}

int rr_to_wire(dns_rr rr, unsigned char *wire, int query_only)
{
	/*
	 * Convert a DNS resource record struct to DNS wire format, using the
	 * provided byte array (wire).  Return the number of bytes used by the
	 * name in wire format.
	 *
	 * INPUT:  rr: the dns_rr struct containing the rr record
	 * INPUT:  wire: a pointer to the array of bytes where the
	 *             wire-formatted resource record should be constructed
	 * INPUT:  query_only: a boolean value (1 or 0) which indicates whether
	 *              we are constructing a full resource record or only a
	 *              query (i.e., in the question section of the DNS
	 *              message).  In the case of the latter, the ttl,
	 *              rdata_len, and rdata are skipped.
	 * OUTPUT: the length of the wire-formatted resource record.
	 *
	 */
}

// void create_dns_query(char *qname, dns_rr_type qtype, unsigned char *wire) // original definition
void create_dns_query(char *qname, int wire_size, unsigned char *wire)
{
	/*
	 * Create a wire-formatted DNS (query) message using the provided byte
	 * array (wire).  Create the header and question sections, including
	 * the qname and qtype.
	 *
	 * INPUT:  qname: the string containing the name to be queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes where the DNS wire
	 *               message should be constructed
	 * OUTPUT: the length of the DNS wire message
	 */
	int qsize = strlen(qname) + 2;
	char dest[qsize];
	name_ascii_to_wire(qname, dest);

	unsigned char rand1 = random() % 256;
	unsigned char rand2 = random() % 256;

	unsigned char header[] = {
		rand1, rand2, 0x01, 0x00, //
		0x00, 0x01, 0x00, 0x00,   //
		0x00, 0x00, 0x00, 0x00,   //
	};
	unsigned char footer[] = {
		0x00, 0x01, 0x00, 0x01, //
	};
	memcpy(wire, header, HEADER_SIZE);
	memcpy(wire + HEADER_SIZE, dest, qsize);
	memcpy(wire + HEADER_SIZE + qsize, footer, FOOTER_SIZE);
}

// int send_recv_message(unsigned char *request, int requestlen, unsigned char *response, char *server, unsigned short port)
int send_recv_message(unsigned char *request, int requestlen, unsigned char *response, char *server, char *port)
{
	/*
	 * Send a message (request) over UDP to a server (server) and port
	 * (port) and wait for a response, which is placed in another byte
	 * array (response).  Create a socket, "connect()" it to the
	 * appropriate destination, and then use send() and recv();
	 *
	 * INPUT:  request: a pointer to an array of bytes that should be sent
	 * INPUT:  requestlen: the length of request, in bytes.
	 * INPUT:  response: a pointer to an array of bytes in which the
	 *             response should be received
	 * OUTPUT: the size (bytes) of the response received
	 */

	//coppied from hw5
	int sfd;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_INET;		/* Allow IPv4 */
	hints.ai_socktype = SOCK_DGRAM; /* Datagram socket */
	hints.ai_flags = 0;
	hints.ai_protocol = 0; /* Any protocol */

	int s = getaddrinfo(server, port, &hints, &result); // TODO: is this right?
	if (s != 0)
	{
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(s));
		exit(EXIT_FAILURE);
	}
	for (rp = result; rp != NULL; rp = rp->ai_next)
	{
		sfd = socket(rp->ai_family, rp->ai_socktype,
					 rp->ai_protocol);
		if (sfd == -1)
			continue;

		if (connect(sfd, rp->ai_addr, rp->ai_addrlen) != -1)
			break; /* Success */

		close(sfd);
	}
	if (rp == NULL)
	{ /* No address succeeded */
		fprintf(stderr, "Could not connect\n");
		exit(EXIT_FAILURE);
	}
	freeaddrinfo(result); /* No longer needed */

	int bytes_written = send(sfd, request, requestlen, 0);
	int bytes_received = recv(sfd, response, RES_MAX, 0);
	// print_bytes(response, bytes_received);
	return bytes_received;
}

unsigned short swap_bytes_in_short(unsigned short s)
{
	// printf("Before: %#x\n", s);
	unsigned short copy = s;
	s = s << 8;
	copy = copy >> 8;
	s = s | copy;

	// printf("After: %#x\n", s);
	return s;
}

char *get_answer_address(unsigned char *wire, int index)
{
	/*
	 * Extract the IPv4 address from the answer section, following any
	 * aliases that might be found, and return the string representation of
	 * the IP address.  If no address is found, then return NULL.
	 *
	 * INPUT:  qname: the string containing the name that was queried
	 * INPUT:  qtype: the integer representation of type of the query (type A == 1)
	 * INPUT:  wire: the pointer to the array of bytes representing the DNS wire message
	 * OUTPUT: a linked list of dns_answer_entrys the value member of each
	 * reflecting either the name or IP address.  If
	 */

	char *ip_address = malloc(16); //255.255.255.255\0 is the largest string

	sprintf(ip_address, "%d.%d.%d.%d", wire[index], wire[index + 1], wire[index + 2], wire[index + 3]);
	return ip_address;
}

dns_answer_entry *resolve(char *qname, char *server, char *port)
{
	int size = HEADER_SIZE + strlen(qname) + 2 /*first size label and null char*/ + FOOTER_SIZE;
	char wire[size];
	create_dns_query(qname, size, wire);
	// print_bytes(wire, size);

	unsigned char response[RES_MAX];
	int bytes_received = send_recv_message(wire, size, response, server, port);

	dns_answer_entry *current_ans = malloc(sizeof(struct dns_answer_entry));
	dns_answer_entry *head = current_ans;
	dns_answer_entry *prev;
	unsigned short totalRRs = response[RR_COUNT_LOCATION];
	totalRRs <<= 8; //becuase big/little endian mismatch, I can't simply grab the data out of the array with pointer type casting.
	totalRRs += response[RR_COUNT_LOCATION + 1];

	// unsigned char *RR = response + size; //begining of response section
	int index = size;
	for (int i = 0; i < totalRRs; i++)
	{
		current_ans->next = malloc(sizeof(struct dns_answer_entry));

		int name_len;
		if ((response + index)[0] >= 0xC0)
		{
			name_len = 2;
		}
		else
		{
			name_len = strlen((response + index)) + 1;
		}
		index += name_len;
		index += 10; //useless info in the response record
		if ((response + index - 10)[1] == CNAME)
		{
			//alias
			current_ans->value = name_ascii_from_wire(response, index); // TODO: remember to calculate the correct index
		}
		else
		{
			// IP address
			current_ans->value = get_answer_address(response, index);
		}
		prev = current_ans;
		current_ans = current_ans->next;
		unsigned short Rdata_len = response[index - 2];
		Rdata_len <<= 8;
		Rdata_len += response[index - 1];
		// printf("Rdata_len = %#x\n", Rdata_len);
		index += Rdata_len; // RDATA length;
							// index += name_len;
	}
	if (totalRRs == 0)
	{
		return NULL;
	}
	prev->next = NULL;
	free(current_ans);
	return head;
}

// struct dns_answer_entry
// {
// 	char *value;
// 	struct dns_answer_entry *next;
// };

int main(int argc, char *argv[])
{
	char *port;
	dns_answer_entry *ans_list, *ans;
	if (argc < 3)
	{
		fprintf(stderr, "Usage: %s <domain name> <server> [ <port> ]\n", argv[0]);
		exit(1);
	}
	if (argc > 3)
	{
		port = argv[3];
	}
	else
	{
		port = "53";
	}

	ans = ans_list = resolve(argv[1], argv[2], port);
	while (ans != NULL)
	{
		printf("%s\n", ans->value);
		ans = ans->next;
	}
	if (ans_list != NULL)
	{
		free_answer_entries(ans_list);
	}
}
