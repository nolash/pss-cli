#ifndef PSSCLI_SERVER_H_
#define PSSCLI_SERVER_H_

#define PSSCLI_SERVER_SOCK_MAX 8
#define PSSCLI_SERVER_SOCK_BUFFERSIZE 8096

#define PSSCLI_SERVER_STATUS_IDLE 0
#define PSSCLI_SERVER_STATUS_RUNNING 1

/***
 * \brief start the socket server
 *
 * starts the main run loop and the reply processing thread
 *
 * \return 0 on successful exit
 * \todo find better way of preventing overwrite of cursor passed to socket connection handler
 */
int psscli_server_start();

/***
 * \brief stop the socket server
 *
 * stops all threads and the main run loop
 *
 */
void psscli_server_stop();

/***
 * \brief check the running status of the socket server
 *
 * \return 0 if idle, 1 if running
 */
int psscli_server_status();

#endif // PSSCLI_SERVER_H_
