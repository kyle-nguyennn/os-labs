#include <stdlib.h>
#include <pthread.h>

#include "gfclient-student.h"
#include "workload.h"

#define MAX_THREADS 1024
#define PATH_BUFFER_SIZE 512

#define USAGE                                                             \
  "usage:\n"                                                              \
  "  gfclient_download [options]\n"                                       \
  "options:\n"                                                            \
  "  -h                  Show this help message\n"                        \
  "  -p [server_port]    Server port (Default: 29458)\n"                  \
  "  -t [nthreads]       Number of threads (Default 8 Max: 1024)\n"       \
  "  -w [workload_path]  Path to workload file (Default: workload.txt)\n" \
  "  -s [server_addr]    Server address (Default: 127.0.0.1)\n"           \
  "  -n [num_requests]   Request download total (Default: 16)\n"

/* OPTIONS DESCRIPTOR ====================================================== */
static struct option gLongOptions[] = {
    {"server", required_argument, NULL, 's'},
    {"port", required_argument, NULL, 'p'},
    {"help", no_argument, NULL, 'h'},
    {"nthreads", required_argument, NULL, 't'},
    {"workload", required_argument, NULL, 'w'},
    {"nrequests", required_argument, NULL, 'n'},
    {NULL, 0, NULL, 0}};
  
typedef struct {
  char* server;
  unsigned short port;
  int n_requests;
} worker_args_t;

static void Usage() { fprintf(stderr, "%s", USAGE); }

static void localPath(char *req_path, char *local_path) {
  static int counter = 0;

  sprintf(local_path, "%s-%06d", &req_path[1], counter++);
}

static FILE *openFile(char *path) {
  char *cur, *prev;
  FILE *ans;

  /* Make the directory if it isn't there */
  prev = path;
  while (NULL != (cur = strchr(prev + 1, '/'))) {
    *cur = '\0';

    if (0 > mkdir(&path[0], S_IRWXU)) {
      if (errno != EEXIST) {
        perror("Unable to create directory");
        exit(EXIT_FAILURE);
      }
    }

    *cur = '/';
    prev = cur;
  }

  if (NULL == (ans = fopen(&path[0], "w"))) {
    perror("Unable to open file");
    exit(EXIT_FAILURE);
  }

  return ans;
}

/* Callbacks ========================================================= */
static void writecb(void *data, size_t data_len, void *arg) {
  FILE *file = (FILE *)arg;
  fwrite(data, 1, data_len, file);
}

static unsigned short int counter = 0;
static pthread_mutex_t counter_mutex;

int download(char* server, int port, char* req_path) {
  gfcrequest_t *gfr = NULL;
  char local_path[PATH_BUFFER_SIZE];
  int returncode = 0;
  localPath(req_path, local_path);

  FILE *file = openFile(local_path);

  gfr = gfc_create();
  gfc_set_path(&gfr, req_path);

  gfc_set_port(&gfr, port);
  gfc_set_server(&gfr, server);
  gfc_set_writearg(&gfr, file);
  gfc_set_writefunc(&gfr, writecb);

  fprintf(stdout, "Requesting %s%s\n", server, req_path);

  if (0 > (returncode = gfc_perform(&gfr))) {
    fprintf(stdout, "gfc_perform returned an error %d\n", returncode);
    fclose(file);
    if (0 > unlink(local_path))
      fprintf(stderr, "warning: unlink failed on %s\n", local_path);
  } else {
    fclose(file);
  }

  if (gfc_get_status(&gfr) != GF_OK) {
    if (0 > unlink(local_path)) {
      fprintf(stderr, "warning: unlink failed on %s\n", local_path);
    }
  }

  fprintf(stdout, "Status: %s\n", gfc_strstatus(gfc_get_status(&gfr)));
  fprintf(stdout, "Received %zu of %zu bytes\n", gfc_get_bytesreceived(&gfr),
          gfc_get_filelen(&gfr));

  gfc_cleanup(&gfr);
  return 0;
}

void* download_thread(void* args) {
  worker_args_t* worker_args = (worker_args_t*)args; // constant throughout the thread's life
  char* req_path;
  while (1) {
    // acquire mutex here to check for end of workload and get next req_path
    req_path = NULL;
    int request_id = -1;
    pthread_mutex_lock(&counter_mutex);
    if (counter < worker_args->n_requests) {
      req_path =  workload_get_path();
      request_id = counter++;
    }
    pthread_mutex_unlock(&counter_mutex);
    
    if (req_path != NULL){
      if (strlen(req_path) > PATH_BUFFER_SIZE) {
        fprintf(stderr, "Request path exceeded maximum of %d characters\n.", PATH_BUFFER_SIZE);
        continue; // TODO: maybe not exit in the thread, but need a way to tell the main thread that the download failed
      }

      if (0 > download(worker_args->server, worker_args->port, req_path)) {
        fprintf(stderr, "Error downloading file %s\n", req_path);
      } else {
        printf("Request %d complete\n", request_id);
      }
    } else {
      return NULL;
    }
  }
}

/* Main ========================================================= */
int main(int argc, char **argv) {
  /* COMMAND LINE OPTIONS ============================================= */
  char *workload_path = "workload.txt";
  char *server = "localhost";
  unsigned short port = 29458;
  int option_char = 0;
  int nthreads = 8;

  int nrequests = 14;

  setbuf(stdout, NULL);  // disable caching

  // Parse and set command line arguments
  while ((option_char = getopt_long(argc, argv, "p:n:hs:t:r:w:", gLongOptions,
                                    NULL)) != -1) {
    switch (option_char) {

      case 'w':  // workload-path
        workload_path = optarg;
        break;
      case 's':  // server
        server = optarg;
        break;
      case 'r': // nrequests
      case 'n': // nrequests
        nrequests = atoi(optarg);
        break;
      case 'p':  // port
        port = atoi(optarg);
        break;
      case 't':  // nthreads
        nthreads = atoi(optarg);
        break;
      default:
        Usage();
        exit(1);


      case 'h':  // help
        Usage();
        exit(0);
    }
  }

  if (EXIT_SUCCESS != workload_init(workload_path)) {
    fprintf(stderr, "Unable to load workload file %s.\n", workload_path);
    exit(EXIT_FAILURE);
  }
  if (port > 65331) {
    fprintf(stderr, "Invalid port number\n");
    exit(EXIT_FAILURE);
  }
  if (nthreads < 1 || nthreads > MAX_THREADS) {
    fprintf(stderr, "Invalid amount of threads\n");
    exit(EXIT_FAILURE);
  }
  gfc_global_init();

  // initialize results
  pthread_mutex_init(&counter_mutex, NULL);

  // add your threadpool creation here
  pthread_t* thread_pool = (pthread_t*)malloc(nthreads * sizeof(pthread_t));
  worker_args_t args;
  args.server = server;
  args.port = port;
  args.n_requests = nrequests; // read-only args so can share
  for (int i=0; i<nthreads; i++) {
    pthread_create(&thread_pool[i], NULL, download_thread, &args);
  }
  for (int i=0; i<nthreads; i++) {
    pthread_join(thread_pool[i], NULL);
  }
  // TODO: handle workers go rogue
  printf("Doanloaded all files.\n");
  // clean up
  free(thread_pool);

  /*
   * note that when you move the above logic into your worker thread, you will
   * need to coordinate with the boss thread here to effect a clean shutdown.
   */

  gfc_global_cleanup();  /* use for any global cleanup for AFTER your thread
                          pool has terminated. */

  return 0;
}
