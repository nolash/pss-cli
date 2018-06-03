#ifndef PSSCLI_CMD_H_
#define PSSCLI_CMD_H_

#define PSSCLI_CMD_RESPONSE_MAX 4096+256

#define	PSSCLI_QUEUE_OUT 0
#define	PSSCLI_QUEUE_X 1
#define	PSSCLI_QUEUE_IN 2

#define PSSCLI_QUEUE_MAX 64

// bitwise
#define PSSCLI_STATUS_LOCAL 1 // if message originated locally
#define PSSCLI_STATUS_VALID 2 // if message has been parsed successfully
#define PSSCLI_STATUS_TX 4 // if message has been transmitted
#define PSSCLI_STATUS_DONE 128 // if message can be garbage collected


enum psscli_cmd_code {
	PSSCLI_CMD_NONE,
	PSSCLI_CMD_MSG, // incoming message
	PSSCLI_CMD_BASEADDR,
	PSSCLI_CMD_GETPUBLICKEY,
	PSSCLI_CMD_SETPEERPUBLICKEY
};

typedef struct psscli_cmd_ {
	long int id;
	char status;
	enum psscli_cmd_code code;
	int sd;
	int *sdptr;
	char **values;
	unsigned char valuecount;
} psscli_cmd;

psscli_cmd psscli_cmd_current;
//
//typedef struct psscli_response_ {
//	long int id;
//	int sd;
//	int *sdptr;
//	char status;
//	char content[PSSCLI_CMD_RESPONSE_MAX]; 
//	int length;
//} psscli_response;
//
//psscli_response psscli_response_current;
//
/***
 * \brief set up command queues
 *
 * must be called before any cmd queue operations
 *
 * \param cmdsize queue size of commands
 * \param responsesize queue size of responses
 * \return 0 on success, PSS_EMEM if queue allocations fail
 */
int psscli_queue_start(short size);

/***
 * \brief release queue allocations. This must be called on shutdown if psscli_cmd_start() has been called
 *
 * \todo iterate all populated queue slots and free entries
 */
void psscli_queue_stop();

/***
 * \brief allocate resources to new command
 *
 * \param cmd command object to allocate to
 * \param valuecount number of string value fields to allocate.
 * \return pointer to allocated command object, or NULL if allocation failed.
 */
psscli_cmd* psscli_cmd_alloc(psscli_cmd *cmd, int valuecount);

/***
 * \brief release all resources held by command
 *
 * Will free all allocations on individual pointers in the values array, from 0 to valuecount. If a pointer is NULL, free will not be attempted.
 *
 * \param cmd command to release
 */
void psscli_cmd_free(psscli_cmd *cmd);

/***
 * \brief copy content from command
 *
 * Will copy all content from *from to *to. If function fails data in *to may be destroyed.
 *
 * \param to destination object
 * \param from source object
 * \return 0 on success, PSSCLI_EMEM if new object couldn't be allocated
 */
int psscli_cmd_copy(psscli_cmd *to, psscli_cmd *from);

/***
 * \brief add a command to the queue
 *
 * Sets cmd id to serial number of queue
 *
 * Not thread safe
 *
 * \param cmd pointer to command to add
 * \return 0 on success, -1 if element couldn't be added
 */
int psscli_cmd_queue_add(char qid, psscli_cmd *cmd);

/***
 * \brief get next command in queue
 *
 * Not thread safe
 *
 * \return pointer to cmd, NULL if there are none
 */
psscli_cmd *psscli_cmd_queue_peek(char qid);

/***
 * \brief advance the cmd queue pointer
 *
 * \return 0 on success. Will return PSSCLI_EBUSY if the current command is pending, or PSSCLI_ENONE if there is nothing to process in the queue
 */
psscli_cmd *psscli_cmd_queue_next(char qid);

#endif // PSSCLI_CMD_H_
