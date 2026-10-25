/*-------------------------------------------------------
 *
 * http_client.c
 *
 * General purpose file to perform HTTP/HTTPS transfers
 * to/from a client
 *
 *------------------------------------------------------
 *
 * This is a generic HTTP/HTTPS client to a server
 *
 * The softwareis based on
 * https://github.com/espressif/esp-idf/blob/8760e6d2a7e19913bc40675dd71f374bcd51b0ae/examples/protocols/esp_http_client/main/esp_http_client_example.c
 *
 *
 * See http_client.h for the various compilation options
 *
 *-----------------------------------------------------*/

#include <string.h>
#include <sys/param.h>
#include <stdlib.h>
#include <ctype.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_tls.h"
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#include "esp_crt_bundle.h"
#endif

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include "esp_system.h"

#include "esp_http_client.h"

#include "serial_io.h"
#include "freETarget.h"
#include "http_client.h"
#include "diag_tools.h"
#include "json.h"

#define MAX_HTTP_RECV_BUFFER   512
#define MAX_HTTP_OUTPUT_BUFFER 2048

/* Root cert for howsmyssl.com, taken from howsmyssl_com_root_cert.pem

   The PEM file was extracted from the output of this command:
   openssl s_client -showcerts -connect www.howsmyssl.com:443 </dev/null

   The CA root cert is the last cert given in the chain of certs.

   To embed it in the app binary, the PEM file is named
   in the component.mk COMPONENT_EMBED_TXTFILES variable.
*/
#if ( BUILD_HTTPS )
const char howsmyssl_com_root_cert_pem_start[] asm("_binary_howsmyssl_com_root_cert_pem_start");
const char howsmyssl_com_root_cert_pem_end[] asm("_binary_howsmyssl_com_root_cert_pem_end");

const char postman_root_cert_pem_start[] asm("_binary_postman_root_cert_pem_start");
const char postman_root_cert_pem_end[] asm("_binary_postman_root_cert_pem_end");
#endif

/*----------------------------------------------------------------
 *
 * @function: http_client_init
 *
 * @brief:    Initialize the HTTP client and prepare it for operation
 *
 * @return:   TRUE if the client is ready for operation
 *
 *---------------------------------------------------------------
 *
 * Read the nonvol into RAM.
 *
 * If the results is uninitalized then force the factory default.
 * Then check for out of bounds and reset those values
 *
 *------------------------------------------------------------*/
void http_client_init(void)
{
  ESP_ERROR_CHECK(esp_netif_init());
#if ( BUILD_HTTP )
  ESP_ERROR_CHECK(esp_event_loop_create_default());
#endif
}

#if ( BUILD_HTTP )
/*----------------------------------------------------------------
 *
 * @function: _http_event_handler
 *
 * @brief:    Initialize the HTTP client and prepare it for operation
 *
 * @return:   Error state
 *
 *---------------------------------------------------------------
 *
 * Event handler call back for HTTP events
 *
 *------------------------------------------------------------*/
esp_err_t _http_event_handler(esp_http_client_event_t *evt // Event being processed
)
{
  static char *output_buffer;                              // Buffer to store response of http request from event handler
  static int   output_len;                                 // Stores number of bytes read

                                                           /*
                                                            *  Switch to the event that got us here
                                                            */
  switch ( evt->event_id )
  {
    case HTTP_EVENT_ERROR:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_ERROR");)
      break;

    case HTTP_EVENT_ON_CONNECTED:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_ON_CONNECTED");)
      break;

    case HTTP_EVENT_HEADER_SENT:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_HEADER_SENT");)
      break;

    case HTTP_EVENT_ON_HEADER:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);)
      break;

    case HTTP_EVENT_ON_DATA:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);)
      // Clean the buffer in case of a new request
      if ( output_len == 0 && evt->user_data )
      {
        // we are just starting to copy the output data into the use
        memset(evt->user_data, 0, MAX_HTTP_OUTPUT_BUFFER);
      }
      /*
       *  Check for chunked encoding is added as the URL for chunked encoding used in this example returns binary data.
       *  However, event handler can also be used in case chunked encoding is used.
       */
      if ( !esp_http_client_is_chunked_response(evt->client) )
      {
        // If user_data buffer is configured, copy the response into the buffer
        int copy_len = 0;
        if ( evt->user_data )
        {
          // The last byte in evt->user_data is kept for the NULL character in case of out-of-bound access.
          copy_len = MIN(evt->data_len, (MAX_HTTP_OUTPUT_BUFFER - output_len));
          if ( copy_len )
          {
            memcpy(evt->user_data + output_len, evt->data, copy_len);
          }
        }
        else
        {
          int content_len = esp_http_client_get_content_length(evt->client);
          if ( output_buffer == NULL )
          {
            // We initialize output_buffer with 0 because it is used by strlen() and similar functions therefore should be null terminated.
            output_buffer = (char *)calloc(content_len + 1, sizeof(char));
            output_len    = 0;
            if ( output_buffer == NULL )
            {
              DLT(DLT_COMMUNICATION, printf("Failed to allocate memory for output buffer");)
              return ESP_FAIL;
            }
          }
          copy_len = MIN(evt->data_len, (content_len - output_len));
          if ( copy_len )
          {
            memcpy(output_buffer + output_len, evt->data, copy_len);
          }
        }
        output_len += copy_len;
      }

      break;

    case HTTP_EVENT_ON_FINISH:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_ON_FINISH");)
      if ( output_buffer != NULL )
      {
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;

    case HTTP_EVENT_DISCONNECTED:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_DISCONNECTED");)
      int       mbedtls_err = 0;
      esp_err_t err         = esp_tls_get_and_clear_last_error((esp_tls_error_handle_t)evt->data, &mbedtls_err, NULL);
      if ( err != 0 )
      {
        DLT(DLT_COMMUNICATION, printf("Last esp error code: 0x%x", err);)
        DLT(DLT_COMMUNICATION, printf("Last mbedtls failure: 0x%x", mbedtls_err);)
      }
      if ( output_buffer != NULL )
      {
        free(output_buffer);
        output_buffer = NULL;
      }
      output_len = 0;
      break;

    case HTTP_EVENT_REDIRECT:
      DLT(DLT_COMMUNICATION, printf("HTTP_EVENT_REDIRECT");)
      esp_http_client_set_header(evt->client, "From", "user@example.com");
      esp_http_client_set_header(evt->client, "Accept", "text/html");
      esp_http_client_set_redirection(evt->client);
      break;
  }
  return ESP_OK;
}

/*----------------------------------------------------------------
 *
 * @function: http_rest_with_url
 *
 * @brief:    REST server called by a URL
 *
 * @return:   None
 *
 *---------------------------------------------------------------
 *
 * Event handler call back for HTTP events
 *
 * The url is of the form http://my_url.com/option/option...
 *
 *------------------------------------------------------------*/
void http_rest_with_url(char *url,    // URL being accessed
                        int   method, // Method being used
                        char *payload // Payload being transferred
)
{
  esp_http_client_config_t config = {
      .host  = url,
      .path  = "/get",
      .query = "esp",
#if ( BUILD_EVENT_HDR )
      .event_handler = _http_event_handler,
#endif
      .user_data             = payload, // Where data comes from and goes to
      .disable_auto_redirect = true,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

                                        /*
                                         *  Switch to the method being used
                                         */
  switch ( method )
  {
#if ( INCLUDE_GET )
    case METHOD_GET:
      esp_http_client_set_method(client, HTTP_METHOD_GET);
      if ( esp_http_client_perform(client) == ESP_OK )
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP GET Status = %d, content_length = %lld",
                                                     esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));))
      }
      else
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP GET request failed %s", url);))
      }
      break;
#endif

#if ( INCLUDE_POST )
    case METHOD_POST:
      esp_http_client_set_url(client, url);
      esp_http_client_set_method(client, HTTP_METHOD_POST);
      esp_http_client_set_header(client, "Content-Type", "application/json");
      esp_http_client_set_post_field(client, payload, strlen(payload));
      if ( esp_http_client_perform(client) == ESP_OK )
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP POST Status = %d, content_length = %" PRId64,
                                                     esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));))
      }
      else
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP POST request failed:");))
      }
      break;
#endif

#if ( INCLUDE_PUT )
    case METHOD_PUT:
      esp_http_client_set_url(client, url);
      esp_http_client_set_method(client, HTTP_METHOD_PUT);
      err = esp_http_client_perform(client);
      if ( err == ESP_OK )
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP PUT Status = %d, content_length = %lld",
                                                     esp_http_client_get_status_code(client), esp_http_client_get_content_length(client));))
      }
      else
      {
        DLT(DLT_COMMUNICATION, SEND(CONSOLE, sprintf(_xs, "HTTP PUT request failed:");))
      }
      break;
#endif

#if ( INCLUDE_PATCH )
    case METHOD_PATCH:
      esp_http_client_set_url(client, url);
      esp_http_client_set_method(client, HTTP_METHOD_PATCH);
      esp_http_client_set_post_field(client, NULL, 0);
      err = esp_http_client_perform(client);
      if ( err == ESP_OK )
      {
        ESP_LOGI(TAG, "HTTP PATCH Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
      }
      else
      {
        DLT(DLT_CRITICAL, printf("HTTP PATCH request failed: %s", esp_err_to_name(err));)
      }
      break;
#endif

#if ( INCLUDE_DELETE )
    case METHOD_DELETE:
      esp_http_client_set_url(client, url);
      esp_http_client_set_method(client, HTTP_METHOD_DELETE);
      err = esp_http_client_perform(client);
      if ( err == ESP_OK )
      {
        ESP_LOGI(TAG, "HTTP DELETE Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
      }
      else
      {
        DLT(DLT_CRITICAL, printf("HTTP DELETE request failed: %s", esp_err_to_name(err));)
      }
      break;
#endif

#if ( INCLUDE_HEAD )
    case METHOD_HEAD:
      esp_http_client_set_url(client, url);
      esp_http_client_set_method(client, HTTP_METHOD_HEAD);
      err = esp_http_client_perform(client);
      if ( err == ESP_OK )
      {
        ESP_LOGI(TAG, "HTTP HEAD Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                 esp_http_client_get_content_length(client));
      }
      else
      {
        DLT(DLT_CRITICAL, printf("HTTP HEAD request failed: %s", esp_err_to_name(err));)
      }
      break;
#endif
  }
  /*
   *  Method complete, clean up and go back
   */
  esp_http_client_cleanup(client);
  return;
}

#endif

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#if ( BUILD_HTTPS )
/*----------------------------------------------------------------
 *
 * @function: https_rest_with_url
 *
 * @brief:    REST server called by a URL
 *
 * @return:   None
 *
 *---------------------------------------------------------------
 *
 * Event handler call back for HTTP events
 *
 * The url is of the form http://my_url.com/option/option...
 *
 *------------------------------------------------------------*/
static void https_with_url(char *url, int method char *payload)
{
  esp_http_client_config_t config = {
      .url               = url,
      .event_handler     = _http_event_handler,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };

  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t                err    = esp_http_client_perform(client);

  if ( err == ESP_OK )
  {
    ESP_LOGI(TAG, "HTTPS Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
             esp_http_client_get_content_length(client));
  }
  else
  {
    ESP_LOGE(TAG, "Error perform http request %s", esp_err_to_name(err));
  }
  esp_http_client_cleanup(client);
}
#endif                              // CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#endif

#if ( BUILD_HTTP )
void http_download_chunk(char *url, // URL to read
                         char *payload)
{
  esp_http_client_config_t config = {
      .url           = url,
      .event_handler = _http_event_handler,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t                err    = esp_http_client_perform(client);

  if ( err == ESP_OK )
  {
    DLT(DLT_COMMUNICATION, printf("HTTP chunk encoding Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                                  esp_http_client_get_content_length(client));)
  }
  else
  {
    DLT(DLT_COMMUNICATION, printf("Error perform http request %s", esp_err_to_name(err));)
  }
  esp_http_client_cleanup(client);
}

void http_perform_as_stream_reader(char *url, // URL to read from
                                   char *payload)
{
  char *buffer = malloc(MAX_HTTP_RECV_BUFFER + 1);
  if ( buffer == NULL )
  {
    DLT(DLT_COMMUNICATION, printf("Cannot malloc http receive buffer");)
    return;
  }
  esp_http_client_config_t config = {
      .url = url,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t                err;
  if ( (err = esp_http_client_open(client, 0)) != ESP_OK )
  {
    DLT(DLT_COMMUNICATION, printf("Failed to open HTTP connection: %s", esp_err_to_name(err));)
    free(buffer);
    return;
  }
  int content_length = esp_http_client_fetch_headers(client);
  int total_read_len = 0, read_len;
  if ( total_read_len < content_length && content_length <= MAX_HTTP_RECV_BUFFER )
  {
    read_len = esp_http_client_read(client, buffer, content_length);
    if ( read_len <= 0 )
    {
      DLT(DLT_COMMUNICATION, printf("Error read data");)
    }
    buffer[read_len] = 0;
    DLT(DLT_COMMUNICATION, printf("read_len = %d", read_len);)
  }
  DLT(DLT_COMMUNICATION, printf("HTTP Stream reader Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                                esp_http_client_get_content_length(client));)
  esp_http_client_close(client);
  esp_http_client_cleanup(client);
  free(buffer);
}
#endif

#if ( BUILD_HTTPS )
void https_async(char *url,    // URL to connect to
                 char *payload // Messages to send
)
{
  esp_http_client_config_t config = {
      .url           = url,
      .event_handler = _http_event_handler,
      .cert_pem      = postman_root_cert_pem_start,
      .is_async      = true,
      .timeout_ms    = 5000,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);
  esp_err_t                err;
  esp_http_client_set_method(client, HTTP_METHOD_POST);
  esp_http_client_set_post_field(client, payload, strlen(payload));
  while ( 1 )
  {
    err = esp_http_client_perform(client);
    if ( err != ESP_ERR_HTTP_EAGAIN )
    {
      break;
    }
  }
  if ( err == ESP_OK )
  {
    DLT(DLT_COMMUNICATION, printf("HTTPS Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                                  esp_http_client_get_content_length(client));)
  }
  else
  {
    DLT(DLT_COMMUNICATION, printf("Error perform http request %s", esp_err_to_name(err));)
  }
  esp_http_client_cleanup(client);

  // Test HTTP_METHOD_HEAD with is_async enabled
  config.url               = url;
  config.event_handler     = _http_event_handler;
  config.crt_bundle_attach = esp_crt_bundle_attach;
  config.is_async          = true;
  config.timeout_ms        = 5000;

  client = esp_http_client_init(&config);
  esp_http_client_set_method(client, HTTP_METHOD_HEAD);

  while ( 1 )
  {
    err = esp_http_client_perform(client);
    if ( err != ESP_ERR_HTTP_EAGAIN )
    {
      break;
    }
  }
  if ( err == ESP_OK )
  {
    DLT(DLT_COMMUNICATION, printf("HTTPS Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                                  esp_http_client_get_content_length(client));)
  }
  else
  {
    DLT(DLT_COMMUNICATION, printf("Error perform http request %s", esp_err_to_name(err));)
  }
  esp_http_client_cleanup(client);
}
#endif

#if ( BUILD_SIMPLE )
/*----------------------------------------------------------------
 *
 * POST shots to a remote REST server
 *
 *---------------------------------------------------------------
 *
 * Every shot can be sent as a JSON document to a REST server
 * configured by the user (from the JSON interface or from the
 * /remote web page on port 81):
 *
 *  REMOTE_URL      - Where the shot is POSTed (http:// or https://)
 *  REMOTE_KEY      - Access key (typically a GUID)
 *  REMOTE_KEY_MODE - 0: Add "apikey":"<key>" to the JSON payload
 *                    1: Send the key in an X-API-KEY HTTP header
 *  REMOTE_ACTIVE   - 1 to enable the transfer
 *
 * A network transfer can take several seconds (DNS lookup, TLS
 * handshake, slow or missing server), so it must never be done
 * from the shot processing loop.  Instead http_post_shot() makes
 * a copy of the score on the heap and puts a pointer to it on a
 * queue.  http_post_task() takes the scores off the queue one at
 * a time and does the actual POST.
 *
 * If the server cannot keep up and the queue is full, the newest
 * shot is dropped rather than stalling the target.
 *
 *------------------------------------------------------------*/
static QueueHandle_t http_post_queue       = NULL; // Queue of (char *) scores waiting to be POSTed
int                  http_post_last_status = 0;    // Last HTTP status (0 = none sent, -1 = connection failed)

static void http_post_task(void *parameters);      // Task that empties the queue

/*----------------------------------------------------------------
 *
 * @function: http_post_init
 *
 * @brief:    Create the queue and the task used to POST shots
 *
 * @return:   None
 *
 *---------------------------------------------------------------
 *
 * Called once from app_main() after the other tasks have been
 * started.
 *
 * The task is given an 8K stack because the TLS handshake used
 * by https:// URLs runs on the stack of the calling task.
 *
 *------------------------------------------------------------*/
void http_post_init(void)
{
  if ( http_post_queue != NULL ) // Already running
  {
    return;
  }

  http_post_queue = xQueueCreate(HTTP_POST_QUEUE_SIZE, sizeof(char *));
  if ( http_post_queue == NULL )
  {
    DLT(DLT_CRITICAL, printf("http_post_init: Unable to create the POST queue");)
    return;
  }

  xTaskCreate(http_post_task, "http_post_task", 8 * 1024, NULL, 0, NULL); // Lowest (background) priority

  /*
   *  All done, return
   */
  return;
}

/*----------------------------------------------------------------
 *
 * @function: http_post_shot
 *
 * @brief:    Queue a JSON score to be POSTed to the remote server
 *
 * @return:   None
 *
 *---------------------------------------------------------------
 *
 * The score normally comes from build_json_score(), which starts
 * the JSON with "{{{{{" to wake up the PC client.  That is not
 * valid JSON for a REST server, so the extra braces are removed
 * before the score is queued.
 *
 * This function never blocks.  It returns immediately if the
 * remote server is disabled, has no URL, or the queue is full.
 *
 * The caller's buffer (usually _xs) can be reused as soon as
 * this function returns.
 *
 *------------------------------------------------------------*/
void http_post_shot(const char *json) // JSON score to send
{
  const char *start;                  // Start of the JSON document
  char       *copy;                   // Copy of the JSON placed on the queue

  if ( (http_post_queue == NULL)      // Not initialized
       || (json_remote_active == 0)   // or disabled
       || (json_remote_url[0] == 0) ) // or nowhere to send it
  {
    return;
  }

  /*
   *  Find the start of the JSON and skip the wake up braces
   */
  start = strchr(json, '{');
  if ( start == NULL )
  {
    DLT(DLT_COMMUNICATION, printf("http_post_shot: Not a JSON document");)
    return;
  }
  while ( *(start + 1) == '{' )
  {
    start++;
  }

  /*
   *  Copy it onto the heap and hand it over to the POST task
   */
  copy = strdup(start);
  if ( copy == NULL )
  {
    DLT(DLT_COMMUNICATION, printf("http_post_shot: Out of memory, shot dropped");)
    return;
  }

  if ( xQueueSend(http_post_queue, &copy, 0) != pdTRUE ) // Do not wait if the queue is full
  {
    DLT(DLT_COMMUNICATION, printf("http_post_shot: Queue full, shot dropped");)
    free(copy);
  }

  /*
   *  All done, return
   */
  return;
}

/*----------------------------------------------------------------
 *
 * @function: http_post_task
 *
 * @brief:    Take scores off the queue and POST them to the server
 *
 * @return:   Never
 *
 *---------------------------------------------------------------
 *
 * For each score:
 *
 *  1 - Copy the score into the payload buffer and, if the key is
 *      sent in the body, add ,"apikey":"<key>" in front of the
 *      closing }
 *  2 - Open a client to REMOTE_URL.  The ESP-IDF certificate
 *      bundle is attached so that https:// servers with a public
 *      certificate are accepted.
 *  3 - Add the X-API-KEY header if the key is sent in the header
 *  4 - POST the payload and remember the HTTP status code so that
 *      it can be shown on the /remote web page
 *
 * The URL and key are copied at the start of each transfer so a
 * change made from the web page cannot alter them half way
 * through a POST.
 *
 * Only printf() is used for diagnostics because the shared _xs
 * buffer belongs to the shot processing loop.
 *
 *------------------------------------------------------------*/
static void http_post_task(void *parameters)
{
  static char              payload[HTTP_POST_PAYLOAD_SIZE]; // JSON sent to the server
  static char              url[URL_SIZE];                   // Copy of json_remote_url
  static char              key[URL_SIZE];                   // Copy of json_remote_key
  char                    *shot;                            // Score taken off the queue
  char                    *close_brace;                     // Location of the closing }
  esp_http_client_handle_t client;                          // HTTP client for this transfer
  esp_err_t                err;                             // Result of the transfer

  while ( 1 )
  {
    if ( xQueueReceive(http_post_queue, &shot, portMAX_DELAY) != pdTRUE )
    {
      continue;
    }

    /*
     *  Take a snapshot of the settings and build the payload
     */
    strncpy(url, json_remote_url, sizeof(url) - 1);
    url[sizeof(url) - 1] = 0;
    strncpy(key, json_remote_key, sizeof(key) - 1);
    key[sizeof(key) - 1] = 0;

    strncpy(payload, shot, sizeof(payload) - 1);
    payload[sizeof(payload) - 1] = 0;
    free(shot);                                             // The heap copy is no longer needed

    if ( (key[0] != 0) && (json_remote_key_mode == REMOTE_KEY_IN_BODY) )
    {
      close_brace = strrchr(payload, '}');
      if ( (close_brace != NULL)                            // Room for ,"apikey":"<key>"}
           && ((size_t)(close_brace - payload) + strlen(key) + sizeof(", \"apikey\":\"\"}") < sizeof(payload)) )
      {
        sprintf(close_brace, ", \"apikey\":\"%s\"}", key);
      }
      else
      {
        DLT(DLT_COMMUNICATION, printf("http_post_task: No room for the apikey");)
      }
    }

    /*
     *  Set up the client
     */
    esp_http_client_config_t config = {
        .url        = url,
        .method     = HTTP_METHOD_POST,
        .timeout_ms = HTTP_POST_TIMEOUT_MS,
#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
        .crt_bundle_attach = esp_crt_bundle_attach, // Accept servers signed by a public CA (https://)
#endif
    };
    client = esp_http_client_init(&config);
    if ( client == NULL )
    {
      DLT(DLT_COMMUNICATION, printf("http_post_task: Unable to create a client for %s", url);)
      http_post_last_status = -1;
      continue;
    }

    esp_http_client_set_header(client, "Content-Type", "application/json");
    if ( (key[0] != 0) && (json_remote_key_mode == REMOTE_KEY_IN_HEADER) )
    {
      esp_http_client_set_header(client, "X-API-KEY", key);
    }
    esp_http_client_set_post_field(client, payload, strlen(payload));

    /*
     *  Send it and report the result
     */
    DLT(DLT_COMMUNICATION, printf("POST %s %s", url, payload);)
    err = esp_http_client_perform(client);
    if ( err == ESP_OK )
    {
      http_post_last_status = esp_http_client_get_status_code(client);
      DLT(DLT_COMMUNICATION, printf("HTTP POST Status = %d", http_post_last_status);)
    }
    else
    {
      http_post_last_status = -1;
      DLT(DLT_COMMUNICATION, printf("HTTP POST to %s failed: %s", url, esp_err_to_name(err));)
    }

    esp_http_client_cleanup(client);
  }
}
#endif

#if CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#if ( BUILD_HTTP )
/*----------------------------------------------------------------
 *
 * @function: http_partial_download
 *
 * @brief:    Only download part of the header
 *
 * @return:   None
 *
 *---------------------------------------------------------------
 *
 *
 * The url is of the form http://my_url.com/option/option...
 *
 *------------------------------------------------------------*/
void http_partial_download(char *url,         // URL to access
                           char *range_string // Range message, ex "bytes=26-37"
)
{
  esp_http_client_config_t config = {
      .url               = url,
      .event_handler     = _http_event_handler,
      .crt_bundle_attach = esp_crt_bundle_attach,
  };
  esp_http_client_handle_t client = esp_http_client_init(&config);

  // Download a file excluding first 10 bytes
  esp_http_client_set_header(client, "Range", range_string);
  if ( esp_http_client_perform(client) == ESP_OK )
  {
    DLT(DLT_COMMUNICATION, printf("HTTP Status = %d, content_length = %" PRId64, esp_http_client_get_status_code(client),
                                  esp_http_client_get_content_length(client));)
  }
  else
  {
    DLT(DLT_COMMUNICATION, printf("HTTP request failed");)
  }
  esp_http_client_cleanup(client);
}
#endif // CONFIG_MBEDTLS_CERTIFICATE_BUNDLE
#endif
