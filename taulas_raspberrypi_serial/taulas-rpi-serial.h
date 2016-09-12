/**
 * Taulas RPI Serial interface
 *
 * Makefile used to build the software
 *
 * Copyright 2016 Nicolas Mora <mail@babelouest.org>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public License
 * as published by the Free Software Foundation;
 * version 2.1 of the License.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU GENERAL PUBLIC LICENSE for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this library.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __TAULAS_RPI_SERIAL_H_
#define __TAULAS_RPI_SERIAL_H_

#include <string.h>
#include <jansson.h>
#include <signal.h>
#include <pthread.h>
#include <getopt.h>

#include <orcania.h>
#include <yder.h>
#include <ulfius.h>

#include "arduino-serial-lib.h"

// applicaation status
int global_handler_variable;

#define RUNNING  0
#define STOP     1
#define ERROR    2

// Default config values
#define PORT_DEFAULT           8585
#define PREFIX_DEFAULT         "taulas"
#define SERIAL_PATTERN_DEFAULT "/dev/ttyACM"
#define SERIAL_BAUD_DEFAULT    9600
#define SERIAL_TIMEOUT_DEFAULT 3000

// Communication constants
#define COMMAND_PREFIX "<"
#define COMMAND_SUFFIX ">"
#define READ_UNTIL     '>'
#define ALERT_PREFIX   "<{\"alert\":"

// Configuration structure
struct _taulas_config {
  // Config data
  int    port;
  char * prefix;
  char * serial_pattern;
  int    baud;
  int    timeout;
  int    log_mode;
  int    log_level;
  char * log_file;
  
  // working data
  char *          serial_path;
  int             serial_fd;
  char *          device_name;
  char *          alert_url;
  pthread_mutex_t lock;
};

// main functions
void exit_handler(int signal);
int build_config_from_args(int argc, char ** argv, struct _taulas_config * taulas_config);
void clean_config(struct _taulas_config * taulas_config);
void print_help(const char * app_name);

// Serial communication functions
int detect_device_arduino(struct _taulas_config * taulas_config);
int connect_device_arduino(struct _taulas_config * taulas_config);
char * get_name_arduino(struct _taulas_config * taulas_config);
json_t * send_command_arduino(struct _taulas_config * taulas_config, const char * command);
void handle_alert_arduino(struct _taulas_config * taulas_config);

// Callback functions
int callback_send_command (const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_get_alert_url (const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_default (const struct _u_request * request, struct _u_response * response, void * user_data);
int callback_root (const struct _u_request * request, struct _u_response * response, void * user_data);

#endif
