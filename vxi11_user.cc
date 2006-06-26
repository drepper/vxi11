#include "vxi11_user.h"

int	vxi11_open_device(char *ip, CLIENT **client, VXI11_LINK **link) {

Create_LinkParms link_parms;

	*client = clnt_create(ip, DEVICE_CORE, DEVICE_CORE_VERSION, "tcp");

	if (*client == NULL) {
		clnt_pcreateerror(ip);
		return -1;
		}

	/* Set link parameters */
	link_parms.clientId	= (long) *client;
	link_parms.lockDevice	= 0;
	link_parms.lock_timeout	= VXI11_DEFAULT_TIMEOUT;
	link_parms.device	= "inst0";

	*link = create_link_1(&link_parms, *client);
	if (*link == NULL) {
		clnt_perror(*client, ip);
		return -2;
		}
	return 0;
	}

int	vxi11_close_device(char *ip, CLIENT *client, VXI11_LINK *link) {
Device_Error *dev_error;

	dev_error = destroy_link_1(&link->lid, client);

	if (dev_error == NULL) {
		clnt_perror(client,ip);
		return -1;
		}

	clnt_destroy(client);

	return 0;
	}

/* A _lot_ of the time we are sending text strings, and can safely rely on
 * strlen(cmd). */
int	vxi11_send(CLIENT *client, VXI11_LINK *link, char *cmd) {
	return vxi11_send(client, link, cmd, strlen(cmd));
	}
/* We still need the version of the function where the length is set explicitly
 * though, for when we are sending fixed length data blocks. */
int	vxi11_send(CLIENT *client, VXI11_LINK *link, char *cmd, unsigned long len) {
Device_WriteParms write_parms;
Device_WriteResp *write_resp;

	write_parms.lid			= link->lid;
	write_parms.io_timeout		= VXI11_DEFAULT_TIMEOUT;
	write_parms.lock_timeout	= VXI11_DEFAULT_TIMEOUT;
	write_parms.flags		= 8;
	write_parms.data.data_len	= len;
	write_parms.data.data_val	= cmd;

	write_resp = device_write_1(&write_parms, client);

	if (write_resp->error != 0) {
		printf("vxi11_user: write error: %d\n",write_resp->error);
		return -1;
		}
	return 0;
	}

/* wrapper, for default timeout */
long	vxi11_receive(CLIENT *client, VXI11_LINK *link, char *buffer, unsigned long len) {
	return vxi11_receive(client, link, buffer, len, VXI11_READ_TIMEOUT);
	}

long	vxi11_receive(CLIENT *client, VXI11_LINK *link, char *buffer, unsigned long len, unsigned long timeout) {
Device_ReadParms read_parms;
Device_ReadResp *read_resp;
long	reason;
long	curr_pos = 0;

	read_parms.lid			= link->lid;
	read_parms.requestSize		= len;
	read_parms.io_timeout		= timeout;	/* in ms */
	read_parms.lock_timeout		= timeout;	/* in ms */
	read_parms.flags		= 0;
	read_parms.termChar		= 0;

	do {
		read_resp = device_read_1(&read_parms, client);
		if (read_resp->error != 0) {
			printf("vxi11_user: read error: %d\n",read_resp->error);
			return -(read_resp->error);
			}
		if((curr_pos + read_resp->data.data_len) <= len) {
			memcpy(buffer+curr_pos, read_resp->data.data_val, read_resp->data.data_len);
			curr_pos+=read_resp->data.data_len;
			}
		else {
			printf("xvi11_user: read error: buffer too small. need %ld bytes\n",(curr_pos + read_resp->data.data_len));
			return -100;
			}
		reason=read_resp->reason;
		free(read_resp->data.data_val);
		} while (reason < 4);
	return (curr_pos + read_resp->data.data_len); /*actual number of bytes received*/

	}

int	vxi11_send_data_block(CLIENT *client, VXI11_LINK *link, char *cmd, char *buffer, unsigned long len) {
char	*out_buffer;
int	cmd_len=strlen(cmd);

	out_buffer=new char[cmd_len+10+len];
	sprintf(out_buffer,"%s#8%08lu",cmd,len);
	memcpy(out_buffer+cmd_len+10,buffer,(unsigned long) len);
	vxi11_send(client, link, out_buffer, (unsigned long) (cmd_len+10+len));
	delete[] out_buffer;
	}
	

/* This function reads a response in the form of a definite-length block, such
 * as when you ask for waveform data. The data is returned in the following
 * format:
 *   #800001000<1000 bytes of data>
 *   ||\______/
 *   ||    |
 *   ||    \---- number of bytes of data
 *   |\--------- number of digits that follow (in this case 8, with leading 0's)
 *   \---------- always starts with #
 */
long	vxi11_receive_data_block(CLIENT *client, VXI11_LINK *link, char *buffer, unsigned long len, unsigned long timeout) {
/* I'm not sure what the maximum length of this header is, I'll assume it's 
 * 11 (#9 + 9 digits) */
unsigned long	necessary_buffer_size;
char		*in_buffer;
int		ret;
int		ndigits;
unsigned long	returned_bytes;
int		l;
char		scan_cmd[20];
	necessary_buffer_size=len+12;
	in_buffer=new char[necessary_buffer_size];
	ret=vxi11_receive(client, link, in_buffer, necessary_buffer_size, timeout);
	if (ret < 0) return ret;
	if (in_buffer[0] != '#') {
		printf("vxi11_user: data block error: data block does not begin with '#'\n");
		return -3;
		}

	/* first find out how many digits */
	sscanf(in_buffer,"#%1d",&ndigits);
	/* now that we know, we can convert the next <ndigits> bytes into an unsigned long */
	sprintf(scan_cmd,"#%%1d%%%dlu",ndigits);
	//printf("The sscanf command is: %s\n",scan_cmd);
	sscanf(in_buffer,scan_cmd,&ndigits,&returned_bytes);
	//printf("using sscanf, ndigits=%d, returned_bytes=%lu\n",ndigits,returned_bytes);

	memcpy(buffer, in_buffer+(ndigits+2), returned_bytes);
	delete[] in_buffer;
	return (long) returned_bytes;
	}

/* This is mainly a useful function for the overloaded vxi11_obtain_value() fn's */
long	vxi11_send_and_receive(CLIENT *client, VXI11_LINK *link, char *cmd, char *buf, unsigned long buf_len, unsigned long timeout) {
int	ret;
long	bytes_returned;
	ret = vxi11_send(client, link, cmd);
	if (ret != 0) {
		printf("Error: vxi11_send_and_receive: could not send cmd.\n");
		printf("       The function vxi11_send returned %d. ",ret);
		return -1;
		}
	bytes_returned = vxi11_receive(client, link, buf, buf_len, timeout);
	if (bytes_returned <= 0) {
		printf("Error: vxi11_send_and_receive: problem reading reply.\n");
		printf("       The function vxi11_receive returned %ld. ",bytes_returned);
		return -2;
		}
	return 0;
	}

long	vxi11_obtain_long_value(CLIENT *client, VXI11_LINK *link, char *cmd, unsigned long timeout) {
char	buf[50]; /* 50=arbitrary length... more than enough for one number in ascii */
	memset(buf, 0, 50);
	if (vxi11_send_and_receive(client, link, cmd, buf, 50, timeout) != 0) {
		printf("Returning 0\n");
		return 0;
		}
	return strtol(buf, (char **)NULL, 10);
	}

double	vxi11_obtain_double_value(CLIENT *client, VXI11_LINK *link, char *cmd, unsigned long timeout) {
char	buf[50]; /* 50=arbitrary length... more than enough for one number in ascii */
double	val;
	memset(buf, 0, 50);
	if (vxi11_send_and_receive(client, link, cmd, buf, 50, timeout) != 0) {
		printf("Returning 0.0\n");
		return 0.0;
		}
	val = strtod(buf, (char **)NULL);
	//cout<<"cmd="<<cmd<<" val="<<val<<endl;
	return val;
	}

long	vxi11_obtain_long_value(CLIENT *client, VXI11_LINK *link, char *cmd) {
	return vxi11_obtain_long_value(client, link, cmd, VXI11_READ_TIMEOUT);
	}

double	vxi11_obtain_double_value(CLIENT *client, VXI11_LINK *link, char *cmd) {
	return vxi11_obtain_double_value(client, link, cmd, VXI11_READ_TIMEOUT);
	}

