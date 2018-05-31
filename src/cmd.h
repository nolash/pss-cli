#ifndef PSSCLI_CMD_H_
#define PSSCLI_CMD_H_

#define PSSCLI_CMD_RESPONSE_MAX 4096+256

#define PSSCLI_CMD_QUEUE_MAX 64
#define PSSCLI_RESPONSE_QUEUE_MAX 64

#define PSSCLI_RESPONSE_STATUS_UNSENT 0
#define PSSCLI_RESPONSE_STATUS_PENDING 1
#define PSSCLI_RESPONSE_STATUS_RECEIVED 2
#define PSSCLI_RESPONSE_STATUS_PARSED 3

#define PSSCLI_CMD_STATUS_PENDING 0
#define PSSCLI_CMD_STATUS_SENT 1

enum psscli_cmd_code {
	PSSCLI_CMD_NONE,
	PSSCLI_CMD_BASEADDR,
	PSSCLI_CMD_GETPUBLICKEY,
	PSSCLI_CMD_SETPEERPUBLICKEY
};

typedef struct psscli_cmd_ {
	int id;
	char status;
	enum psscli_cmd_code code;
	char **values;
	unsigned char valuecount;
} psscli_cmd;

psscli_cmd psscli_cmd_current;

typedef struct psscli_response_ {
	char id;
	char status;
	char content[PSSCLI_CMD_RESPONSE_MAX]; 
	int length;
} psscli_response;

/***
 * \brief set up command queues
 *
 * must be called before any cmd queue operations
 *
 * \param cmdsize queue size of commands
 * \param responsesize queue size of responses
 * \return 0 on success, PSS_EMEM if queue allocations fail
 */
int psscli_queue_start(short cmdsize, short responsesize);

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
 * Not thread safe
 *
 * \param cmd pointer to command to add
 * \return 0 on success, -1 if element couldn't be added
 */
int psscli_cmd_queue_add(psscli_cmd *cmd);

/***
 * \brief get next command in queue
 *
 * Not thread safe
 *
 * \return pointer to cmd, NULL if there are none
 */
psscli_cmd *psscli_cmd_queue_peek();

/***
 * \brief advance the cmd queue pointer
 *
 * \return 0 on success. Will return PSSCLI_EBUSY if the current command is pending, or PSSCLI_ENONE if there is nothing to process in the queue
 */
psscli_cmd *psscli_cmd_queue_next();

int psscli_response_queue_add(psscli_response *response);
psscli_response *psscli_response_queue_peek();
psscli_response *psscli_response_queue_next();

#endif // PSSCLI_CMD_H_
