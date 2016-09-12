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

#include "taulas-rpi-serial.h"

/**
 * Main function
 * 
 * Creates config structure, start framework, then wait for stop signal
 * 
 */
int main(int argc, char ** argv) {
  struct _taulas_config taulas_config;
  struct _u_instance instance;
  pthread_mutexattr_t mutexattr;
  
  signal (SIGQUIT, exit_handler);
  signal (SIGINT, exit_handler);
  signal (SIGTERM, exit_handler);
  signal (SIGHUP, exit_handler);
  
  taulas_config.port = PORT_DEFAULT;
  taulas_config.prefix = nstrdup(PREFIX_DEFAULT);
  taulas_config.serial_pattern = nstrdup(SERIAL_PATTERN_DEFAULT);
  taulas_config.baud = SERIAL_BAUD_DEFAULT;
  taulas_config.timeout = SERIAL_TIMEOUT_DEFAULT;
#ifdef DEBUG
  taulas_config.log_mode = Y_LOG_MODE_CONSOLE;
  taulas_config.log_level = Y_LOG_LEVEL_DEBUG;
#else
  taulas_config.log_mode = Y_LOG_MODE_SYSLOG;
  taulas_config.log_level = Y_LOG_LEVEL_INFO;
#endif
  taulas_config.log_file = NULL;
  
  if (build_config_from_args(argc, argv, &taulas_config)) {
    y_init_logs("Taulas RPI Serial", taulas_config.log_mode, taulas_config.log_level, taulas_config.log_file, "Starting Taulas RPI Serial interface");
    
    if (detect_device_arduino(&taulas_config)) {
      taulas_config.alert_url = NULL;
      if (ulfius_init_instance(&instance, taulas_config.port, NULL) != U_OK) {
        y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_init_instance, abort");
      } else {
        u_map_put(instance.default_headers, "Access-Control-Allow-Origin", "*");
    
        // Endpoint list declaration
        ulfius_add_endpoint_by_val(&instance, "GET", "/", NULL, NULL, NULL, NULL, &callback_root, &taulas_config);
        ulfius_add_endpoint_by_val(&instance, "GET", taulas_config.prefix, NULL, NULL, NULL, NULL, &callback_send_command, &taulas_config);
        ulfius_add_endpoint_by_val(&instance, "GET", taulas_config.prefix, "/alertCb", NULL, NULL, NULL, &callback_get_alert_url, &taulas_config);

        // default_endpoint declaration
        ulfius_set_default_endpoint(&instance, NULL, NULL, NULL, &callback_default, &taulas_config);
        if (ulfius_start_framework(&instance) != U_OK) {
          y_log_message(Y_LOG_LEVEL_ERROR, "Error ulfius_start_framework, abort");
        } else {
          y_log_message(Y_LOG_LEVEL_INFO, "Program running on port %d, wait for signal to stop", taulas_config.port);
          pthread_mutexattr_init ( &mutexattr );
          pthread_mutexattr_settype( &mutexattr, PTHREAD_MUTEX_RECURSIVE_NP );
          if (pthread_mutex_init(&taulas_config.lock, &mutexattr) != 0) {
            y_log_message(Y_LOG_LEVEL_ERROR, "Impossible to initialize Mutex Lock for MariaDB connection");
          }
          pthread_mutexattr_destroy( &mutexattr );
          taulas_config.serial_fd = connect_device_arduino(&taulas_config);
          global_handler_variable = RUNNING;
          while (global_handler_variable == RUNNING) {
            handle_alert_arduino(&taulas_config);
            sleep(1);
          }
          y_log_message(Y_LOG_LEVEL_INFO, "Exit program");
          close(taulas_config.serial_fd);
          ulfius_stop_framework(&instance);
        }
        ulfius_clean_instance(&instance);
      }
      clean_config(&taulas_config);
      pthread_mutex_destroy(&taulas_config.lock);
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "Can not connect arduino device, abort");
    }
    
    y_close_logs();
  }

  
  return 0;
}

/**
 * Set config values depending on command line arguments
 */
int build_config_from_args(int argc, char ** argv, struct _taulas_config * taulas_config) {
  int next_option;
  char * tmp = NULL, * to_free = NULL, * one_log_mode = NULL;

  const char * short_options = "p::u::s::b::t::l::m::f::h::";
  static const struct option long_options[]= {
    {"port", optional_argument,NULL, 'p'},
    {"url-prefix", optional_argument,NULL, 'u'},
    {"serial-pattern", optional_argument,NULL, 's'},
    {"baud", optional_argument,NULL, 'b'},
    {"timeout", optional_argument,NULL, 't'},
    {"log-level", optional_argument,NULL, 'l'},
    {"log-mode", optional_argument,NULL, 'm'},
    {"log-file", optional_argument,NULL, 'f'},
    {"help", optional_argument,NULL, 'h'},
    {NULL, 0, NULL, 0}
  };
  
  if (taulas_config != NULL) {
    do {
      next_option = getopt_long(argc, argv, short_options, long_options, NULL);
      
      switch (next_option) {
        case 'p':
          if (optarg != NULL) {
            taulas_config->port = strtol(optarg, NULL, 10);
            if (taulas_config->port <= 0 || taulas_config->port > 65535) {
              fprintf(stderr, "Error!\nInvalid TCP Port number\n\tPlease specify an integer value between 1 and 65535");
              print_help(argv[0]);
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no port specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'u':
          if (optarg != NULL) {
            free(taulas_config->prefix);
            taulas_config->prefix = nstrdup(optarg);
            if (taulas_config->prefix == NULL) {
              fprintf(stderr, "Error allocating taulas_config->prefix, exiting\n");
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no URL prefix specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 's':
          if (optarg != NULL) {
            free(taulas_config->serial_pattern);
            taulas_config->serial_pattern = nstrdup(optarg);
            if (taulas_config->serial_pattern == NULL) {
              fprintf(stderr, "Error allocating taulas_config->serial_pattern, exiting\n");
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no URL serial pattern specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'b':
          if (optarg != NULL) {
            taulas_config->baud = strtol(optarg, NULL, 10);
            if (taulas_config->baud <= 0 || taulas_config->baud > 115200) {
              fprintf(stderr, "Error, invalid baud rate\n\tPlease specify an integer value between 1 and 115200");
              print_help(argv[0]);
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no baud rate specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 't':
          if (optarg != NULL) {
            taulas_config->timeout = strtol(optarg, NULL, 10);
            if (taulas_config->timeout <= 0) {
              fprintf(stderr, "Error, invalid timeout number\n\tPlease specify a positive integer value (in seconds)");
              print_help(argv[0]);
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no timeout specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'm':
          if (optarg != NULL) {
            tmp = nstrdup(optarg);
            to_free = to_free;
            if (tmp == NULL) {
              fprintf(stderr, "Error allocating log_mode, exiting\n");
            }
            one_log_mode = strtok(tmp, ",");
            while (one_log_mode != NULL) {
              if (0 == strncmp("console", one_log_mode, strlen("console"))) {
                taulas_config->log_mode |= Y_LOG_MODE_CONSOLE;
              } else if (0 == strncmp("syslog", one_log_mode, strlen("syslog"))) {
                taulas_config->log_mode |= Y_LOG_MODE_SYSLOG;
              } else if (0 == strncmp("file", one_log_mode, strlen("file"))) {
                taulas_config->log_mode |= Y_LOG_MODE_FILE;
              }
              one_log_mode = strtok(NULL, ",");
            }
            free(to_free);
          } else {
            fprintf(stderr, "Error, no mode specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'l':
          if (optarg != NULL) {
            if (0 == strncmp("NONE", optarg, strlen("NONE"))) {
              taulas_config->log_level = Y_LOG_LEVEL_NONE;
            } else if (0 == strncmp("ERROR", optarg, strlen("ERROR"))) {
              taulas_config->log_level = Y_LOG_LEVEL_ERROR;
            } else if (0 == strncmp("WARNING", optarg, strlen("WARNING"))) {
              taulas_config->log_level = Y_LOG_LEVEL_WARNING;
            } else if (0 == strncmp("INFO", optarg, strlen("INFO"))) {
              taulas_config->log_level = Y_LOG_LEVEL_INFO;
            } else if (0 == strncmp("DEBUG", optarg, strlen("DEBUG"))) {
              taulas_config->log_level = Y_LOG_LEVEL_DEBUG;
            }
          } else {
            fprintf(stderr, "Error, no log level specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'f':
          if (optarg != NULL) {
            taulas_config->log_file = nstrdup(optarg);
            if (taulas_config->log_file == NULL) {
              fprintf(stderr, "Error allocating taulas_config->log_file, exiting\n");
              return 0;
            }
          } else {
            fprintf(stderr, "Error, no log file specified\n");
            print_help(argv[0]);
            return 0;
          }
          break;
        case 'h':
          print_help(argv[0]);
          return 0;
          break;
      }
      
    } while (next_option != -1);
    
    return 1;
  } else {
    return 0;
  }
  
}

/**
 * Print help message
 */
void print_help(const char * app_name) {
  printf("\n%s, serial interface with a taulas arduino device\n", app_name);
  printf("Options available:\n");
  printf("-h --help: Print this help message and exit\n");
  printf("-p --port: TCP Port to listen to, default %d\n", PORT_DEFAULT);
  printf("-u --url-prefix: url prefix for the webservice, default '%s'\n", PREFIX_DEFAULT);
  printf("-s --serial-pattern: pattern to the serial file of the arduino, default '%s'\n", SERIAL_PATTERN_DEFAULT);
  printf("-b --baud: baud rate to connect to the Arduino, default %d\n", SERIAL_BAUD_DEFAULT);
  printf("-t --timeout: timeout in seconds for serial reading, default %d\n", SERIAL_TIMEOUT_DEFAULT);
#ifdef DEBUG
  printf("-l --log-level: log level for the application, values are NONE, ERROR, WARNING, INFO, DEBUG, default is 'DEBUG'\n");
  printf("-m --log-mode: log mode for the application, values are console, file or syslog, multiple values must be separated with a comma, default is 'console'\n");
#else
  printf("-l --log-level: log level for the application, values are NONE, ERROR, WARNING, INFO, DEBUG, default is 'INFO'\n");
  printf("-m --log-mode: log mode for the application, values are console, file or syslog, multiple values must be separated with a comma, default is 'syslog'\n");
#endif
  printf("-f --log-file: path to log file if log mode is file\n\n");
}

/**
 * Clean config structure before exit
 */
void clean_config(struct _taulas_config * taulas_config) {
  if (taulas_config != NULL) {
    free(taulas_config->prefix);
    free(taulas_config->serial_pattern);
    free(taulas_config->log_file);
    free(taulas_config->serial_path);
    free(taulas_config->device_name);
    free(taulas_config->alert_url);
  }
}

/**
 * handles signal catch to exit properly when ^C is used for example
 */
void exit_handler(int signal) {
  y_log_message(Y_LOG_LEVEL_INFO, "Caught a stop or kill signal (%d), exiting", signal);
  global_handler_variable = STOP;
}

/**
 * Read serial, if an alert is triggered, send alert message
 */
void handle_alert_arduino(struct _taulas_config * taulas_config) {
  char buffer[1025] = {0};
  json_t * tmp;
  struct _u_request req;
  int res;
  
  if (taulas_config != NULL && taulas_config->alert_url != NULL) {
    if (pthread_mutex_lock(&taulas_config->lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error getting mutex");
    } else {
      serialport_read_until(taulas_config->serial_fd, buffer, READ_UNTIL, 1024, taulas_config->timeout);
      y_log_message(Y_LOG_LEVEL_DEBUG, "Getting message: %s", buffer);
      if (strlen(buffer) > 0 && strncmp(ALERT_PREFIX, buffer, strlen(ALERT_PREFIX)) == 0) {
        y_log_message(Y_LOG_LEVEL_DEBUG, "This message is an alert");
        buffer[strlen(buffer) - 1] = '\0';
        tmp = json_loads(buffer+1, JSON_DECODE_ANY, NULL);
        if (tmp != NULL && json_is_string(json_object_get(tmp, "alert"))) {
          y_log_message(Y_LOG_LEVEL_DEBUG, "Sending alert to angharad");
          ulfius_init_request(&req);
          req.http_url = msprintf("%s/%s/%s/%s/%s", taulas_config->alert_url, "benoic", taulas_config->device_name, json_string_value(json_object_get(tmp, "alert")), "elert");
          res = ulfius_send_http_request(&req, NULL);
          if (res != U_OK) {
            y_log_message(Y_LOG_LEVEL_ERROR, "Error sending alert message");
          } else {
            y_log_message(Y_LOG_LEVEL_DEBUG, "Alert sent succesfully");
          }
          ulfius_clean_request(&req);
        } else {
          y_log_message(Y_LOG_LEVEL_ERROR, "Error decoding alert message");
        }
        json_decref(tmp);
      }
      pthread_mutex_unlock(&taulas_config->lock);
    }
  }
}

/**
 * Detect if a device is available, then updates config structure
 */
int detect_device_arduino(struct _taulas_config * taulas_config) {
  int i,  to_return = 0;
  char * filename;
  
  if (taulas_config != NULL && taulas_config->serial_pattern != NULL) {
    for (i=0; i<128; i++) {
      filename = msprintf("%s%d", taulas_config->serial_pattern, i);
      taulas_config->serial_fd = serialport_init(filename, taulas_config->baud);
      if (taulas_config->serial_fd != -1) {
        serialport_flush(taulas_config->serial_fd);
        taulas_config->device_name = get_name_arduino(taulas_config);
        taulas_config->serial_path = nstrdup(filename);
        if (taulas_config->device_name != NULL) {
          to_return = 1;
        }
        serialport_close(taulas_config->serial_fd);
      }
      free(filename);
    }
  }
  return to_return;
}

/**
 * Connect the arduino device through the serial port
 */
int connect_device_arduino(struct _taulas_config * taulas_config) {
  taulas_config->serial_fd = serialport_init(taulas_config->serial_path, taulas_config->baud);
  if (taulas_config->serial_fd != -1) {
    serialport_flush(taulas_config->serial_fd);
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "Error, seria not connected");
  }
  return taulas_config->serial_fd;
}

/**
 * Get the name of the connected arduino
 */
char * get_name_arduino(struct _taulas_config * taulas_config) {
  char buffer[1025], * to_return = NULL;
  json_t * tmp;
  char * command = COMMAND_PREFIX "NAME" COMMAND_SUFFIX;
  int res;
  
  res = serialport_write(taulas_config->serial_fd, command);
  if (res == 0) {
    serialport_read_until(taulas_config->serial_fd, buffer, READ_UNTIL, 1024, taulas_config->timeout);
    buffer[strlen(buffer) - 1] = '\0';
    tmp = json_loads(buffer+strlen("NAME")+2, JSON_DECODE_ANY, NULL);
    if (tmp != NULL) {
      to_return = nstrdup(json_string_value(json_object_get(tmp, "value")));
      json_decref(tmp);
    } else {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error parsing response: %s", buffer);
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "Error sending name command");
  }
  
  return to_return;
}

/**
 * Send a command to the arduino, then read and parse the response
 */
json_t * send_command_arduino(struct _taulas_config * taulas_config, const char * command) {
  char buffer[1025];
  json_t * to_return = NULL;
  char * serial_command;
  int strlen_prefix_command = strchr(command, '/')!=NULL?strchr(command, '/')-command:strlen(command);
  
  if (taulas_config != NULL && command != NULL) {
    if (pthread_mutex_lock(&taulas_config->lock)) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error getting mutex");
    } else {
      serial_command = msprintf("%s%s%s", COMMAND_PREFIX, command, COMMAND_SUFFIX);
      if (serialport_write(taulas_config->serial_fd, serial_command) == 0) {
        if (!serialport_read_until(taulas_config->serial_fd, buffer, READ_UNTIL, 1024, taulas_config->timeout)) {
          buffer[strlen(buffer) - 1] = '\0';
          to_return = json_loads(buffer+strlen_prefix_command+2, JSON_DECODE_ANY, NULL);
          if (to_return == NULL) {
            y_log_message(Y_LOG_LEVEL_ERROR, "Error parsing buffer %s", buffer+strlen_prefix_command+2);
          }
        } else {
          y_log_message(Y_LOG_LEVEL_ERROR, "Error reading response");
        }
      } else {
        y_log_message(Y_LOG_LEVEL_ERROR, "Error sending command");
        serialport_close(taulas_config->serial_fd);
        if (detect_device_arduino(taulas_config) && connect_device_arduino(taulas_config) != -1) {
          y_log_message(Y_LOG_LEVEL_INFO, "Reconnect arduino succesfull");
        }
      }
      free(serial_command);
      pthread_mutex_unlock(&taulas_config->lock);
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "Error input parameters");
  }
  
  return to_return;
}

/**
 * Callback function used to send the command to the Arduino, then send back arduino response
 */
int callback_send_command (const struct _u_request * request, struct _u_response * response, void * user_data) {
  struct _taulas_config * taulas_config = (struct _taulas_config *)user_data;
  
  if (taulas_config != NULL) {
    response->json_body = send_command_arduino(taulas_config, u_map_get(request->map_url, "command"));
    if (response->json_body == NULL) {
      y_log_message(Y_LOG_LEVEL_ERROR, "Error sending command");
      response->status = 500;
    } else if (json_object_get(response->json_body, "error") != NULL) {
      response->status = 404;
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "Error taulas_config is NULL");
    response->status = 500;
  }
  
  return U_OK;
}

/**
 * Callback function used to update the alert url
 */
int callback_get_alert_url (const struct _u_request * request, struct _u_response * response, void * user_data) {
  struct _taulas_config * taulas_config = (struct _taulas_config *)user_data;
  
  if (taulas_config != NULL) {
    if (u_map_get(request->map_url, "url") != NULL) {
      taulas_config->alert_url = strdup(u_map_get(request->map_url, "url"));
    }
  } else {
    y_log_message(Y_LOG_LEVEL_ERROR, "Error taulas_config is NULL");
    response->status = 500;
  }
  
  return U_OK;
}

/**
 * Default callback function
 * Send endpoints available and status 404
 */
int callback_default (const struct _u_request * request, struct _u_response * response, void * user_data) {
  struct _taulas_config * taulas_config = (struct _taulas_config *)user_data;
  char * command_url = msprintf("/%s?command=<YOUR_COMMAND>", taulas_config->prefix);
  char * set_alert_url = msprintf("/%s/alertCb?url=<YOUR_URL_CALLBACK>", taulas_config->prefix);
  
  response->status = 404;
  response->json_body = json_pack("{ssss}", "command_url", command_url, "set_alert_url", set_alert_url);
  free(command_url);
  free(set_alert_url);
  return U_OK;
}

/**
 * Root url callback function
 * Send endpoints available
 */
int callback_root (const struct _u_request * request, struct _u_response * response, void * user_data) {
  struct _taulas_config * taulas_config = (struct _taulas_config *)user_data;
  char * command_url = msprintf("/%s?command=<YOUR_COMMAND>", taulas_config->prefix);
  char * set_alert_url = msprintf("/%s/alertCb?url=<YOUR_URL_CALLBACK>", taulas_config->prefix);
  
  response->json_body = json_pack("{ssss}", "command_url", command_url, "set_alert_url", set_alert_url);
  free(command_url);
  free(set_alert_url);
  return U_OK;
}
